#pragma once
#include "wasm_utils.h"

inline constexpr auto nanoseconds_in_a_second = 1'000'000'000LL;  // 10^9
inline constexpr auto nanoseconds_in_a_minute = 60 * nanoseconds_in_a_second;  // 60 * 10^9
inline constexpr auto timeout_in_ns = 30 * nanoseconds_in_a_second;  // 30 seconds
using namespace std::chrono_literals;
#ifdef max
#undef max
#endif
constexpr std::chrono::nanoseconds INFINITE_WAIT{ std::numeric_limits<unsigned long long>::max() };
enum class WaitResult {
	OK,
	NotEqual,
	TimedOut,
};
// include c atomic
#include <atomic>
#if defined(LINUX)
#include <linux/futex.h>
#include <sys/syscall.h>
constexpr std::time_t constexpr_nanos_to_secs(std::chrono::nanoseconds ns) {
	return std::chrono::duration_cast<std::chrono::seconds>(ns).count();
}

// A constexpr function to convert nanoseconds to timespec
constexpr timespec constexpr_nanos_to_timespec(std::chrono::nanoseconds ns) {
	return timespec{
		constexpr_nanos_to_secs(ns),
		static_cast<long>((ns - std::chrono::seconds(constexpr_nanos_to_secs(ns))).count())
	};
}
FORCEINLINE_DEBUGGABLE timespec convert_chrono_to_timespec(std::chrono::nanoseconds ns) {
	timespec ts;
	auto secs = std::chrono::duration_cast<std::chrono::seconds>(ns);
	ns -= secs;
	ts.tv_sec = secs.count();
	ts.tv_nsec = ns.count();
	return ts;
}
#elif defined(_WIN32)
#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#if WITH_ENGINE
#include "Windows/AllowWindowsPlatformAtomics.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/HideWindowsPlatformAtomics.h"
#else
#include <Windows.h>
#endif
#include <synchapi.h>
#endif
#if defined(__APPLE__)
#include <os/lock.h>
#endif
template <typename T>
class Atomic {
public:
	T val;
	constexpr Atomic() : val(0) {
		if constexpr (sizeof(T) != 4 && sizeof(T) != 8) {
			//static_assert(false, "Unsupported atomic type");
		}
		// does it need to store atomically at construction?
		//emscripten_atomic_store_u32(&val, 0);
	}
	constexpr Atomic(T v) : val(v) {
		if constexpr (sizeof(T) != 4 && sizeof(T) != 8) {
			//static_assert(false, "Unsupported atomic type");
		}
	}
	static timespec make_timespec(std::chrono::nanoseconds ns) {
		return timespec{ static_cast<long>(ns.count()) / 1000000000, static_cast<long>(ns.count()) % 1000000000 };
	}
	constexpr WaitResult wait(const T old, const std::chrono::nanoseconds time = INFINITE_WAIT) {
		static_assert(sizeof(T) == 4 || sizeof(T) == 8, "Unsupported atomic type");
#ifdef _MSC_VER
		// convert to ms
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time);
		if (WaitOnAddress(&val, (PVOID)&old, sizeof(T), time != INFINITE_WAIT ? DWORD(ms.count()) : -1)) {
			return WaitResult::OK;
		}
		else {
			if (val == old) {
				return WaitResult::TimedOut;
			}
			else {
				return WaitResult::NotEqual;
			}
		}
#elif defined(__APPLE__)
    os_unfair_lock lock = OS_UNFAIR_LOCK_INIT;
    os_unfair_lock_lock(&lock);
    if (val != old) {
        os_unfair_lock_unlock(&lock);
        return WaitResult::NotEqual;
    }
    if (time == INFINITE_WAIT) {
        while (val == old) {
            os_unfair_lock_assert_owner(&lock);
            os_unfair_lock_unlock(&lock);
            sched_yield();
            os_unfair_lock_lock(&lock);
        }
        os_unfair_lock_unlock(&lock);
        return WaitResult::OK;
    } else {
        auto end_time = std::chrono::steady_clock::now() + time;
        while (val == old) {
            os_unfair_lock_assert_owner(&lock);
            os_unfair_lock_unlock(&lock);
            if (std::chrono::steady_clock::now() >= end_time) {
                return WaitResult::TimedOut;
            }
            sched_yield();
            os_unfair_lock_lock(&lock);
        }
        os_unfair_lock_unlock(&lock);
        return WaitResult::OK;
    }
#elif defined(__EMSCRIPTEN__)
		if constexpr (sizeof(T) == 4) {
			return (WaitResult)__builtin_wasm_memory_atomic_wait32((int32_t*)&val, old, time.count());
		}
		else if constexpr (sizeof(T) == 8) {
			return (WaitResult)__builtin_wasm_memory_atomic_wait64((int64_t*)&val, old, time.count());
		}
#else
		// Create a constexpr timespec
		//constexpr timespec ts = constexpr_nanos_to_timespec(time);
#if defined(FUTEX_WAIT_PRIVATE)
		timespec ts = make_timespec(time);
		int ret = syscall(SYS_futex, &val, FUTEX_WAIT_PRIVATE, old, time == INFINITE_WAIT ? nullptr : &ts);
		if (ret == 0) {
			return WaitResult::OK;
		}
		else if (ret == -ETIMEDOUT) {
			return WaitResult::TimedOut;
		}
		else {
			return WaitResult::NotEqual;
		}
#endif
#endif
		return WaitResult::TimedOut;
	}
	constexpr T fetch_add(const T v) {
#ifdef _MSC_VER
		if constexpr (sizeof(T) == 4) {
			return ::_InterlockedExchangeAdd((volatile long*)&val, v);
		}
		else if constexpr (sizeof(T) == 8) {
			return ::_InterlockedExchangeAdd64((volatile int64*)&val, v);
		}
#else
		if constexpr (sizeof(T) == 4) {
			return __c11_atomic_fetch_add((_Atomic uint32*)&val, v, __ATOMIC_SEQ_CST);
		}
		else if constexpr (sizeof(T) == 8) {
			return __c11_atomic_fetch_add((_Atomic uint64*)&val, v, __ATOMIC_SEQ_CST);
		}
#endif
	}
	constexpr void store(const T v) {
		static_assert(sizeof(T) == 4 || sizeof(T) == 8, "Unsupported atomic type");
#ifdef _MSC_VER
		if constexpr (sizeof(T) == 4) {
			::_InterlockedExchange((volatile long*)&val, (int32)v);
		}
		else if constexpr (sizeof(T) == 8) {
			::_InterlockedExchange64((volatile int64*)&val, (int64)v);
		}
#else
		if constexpr (sizeof(T) == 4) {
			__c11_atomic_store((_Atomic(T)*)&val, v, __ATOMIC_SEQ_CST);
		}
		else if constexpr (sizeof(T) == 8) {
			__c11_atomic_store((_Atomic(T)*)&val, v, __ATOMIC_SEQ_CST);
		}
#endif
	}
	constexpr T exchange(const T v) {
#ifdef _MSC_VER
		if constexpr (sizeof(T) == 4) {
			return (T)::_InterlockedExchange((volatile long*)&val, (int32)v);
		}
		else if constexpr (sizeof(T) == 8) {
			return (T)::_InterlockedExchange64((volatile int64*)&val, (int64)v);
		}
#else
		if constexpr (sizeof(T) == 4)  {
			return (T)__c11_atomic_exchange((_Atomic uint32*)&val, (uint32)v, __ATOMIC_SEQ_CST);
		}
		else if constexpr (sizeof(T) == 8) {
			return (T)__c11_atomic_exchange((_Atomic uint64*)&val, (uint64)v, __ATOMIC_SEQ_CST);
		}
#endif
	}
	constexpr T load() const {
#ifdef _MSC_VER
		if constexpr (sizeof(T) == 4) {
			return (T)_InterlockedCompareExchange((volatile uint32*)&val, 0, 0);
		}
		else if constexpr (sizeof(T) == 8) {
			// Call the 64-bit atomic wait function
			return (T)_InterlockedCompareExchange64((volatile int64*)&val, 0, 0);
		}
#else
		if constexpr (sizeof(T) == 4) {
			// Call the 32-bit atomic wait function
			return (T)__c11_atomic_load((_Atomic(uint32)*)&val, __ATOMIC_SEQ_CST);
		}
		else if constexpr (sizeof(T) == 8) {
			// Call the 32-bit atomic wait function
			return (T)__c11_atomic_load((_Atomic(uint64)*)&val, __ATOMIC_SEQ_CST);
		}
#endif
	}
	constexpr Atomic& operator=(const T v) {
		store(v);
		return *this;
	}
	constexpr operator T() {
		return load();
	}
	constexpr Atomic& operator+=(const T delta) {
		static_assert(sizeof(T) == 4 || sizeof(T) == 8, "Unsupported atomic type");
#if defined(_MSC_VER)
		if constexpr (sizeof(T) == 4) {
			_InterlockedExchangeAdd(&val, delta);
		}
		else if constexpr (sizeof(T) == 8) {
			_InterlockedExchangeAdd64((volatile __int64*)&val, delta);
		}
#else
		if constexpr (sizeof(T) == 4) {
			__c11_atomic_fetch_add((_Atomic uint32*)&val, delta, __ATOMIC_SEQ_CST);
		}
		else if constexpr (sizeof(T) == 8) {
			__c11_atomic_fetch_add((_Atomic uint64*)&val, delta, __ATOMIC_SEQ_CST);
		}
#endif
		return *this;
	}
	constexpr Atomic& operator-=(const T delta) {
		static_assert(sizeof(T) == 4 || sizeof(T) == 8, "Unsupported atomic type");
#if defined(_MSC_VER)
		InterlockedExchangeAdd(&val, -delta);
#elif defined(__EMSCRIPTEN__)
		if constexpr (sizeof(T) == 4) {
			__c11_atomic_fetch_sub((_Atomic uint32*)&val, delta, __ATOMIC_SEQ_CST);
		}
		else if constexpr (sizeof(T) == 8) {
			__c11_atomic_fetch_sub((_Atomic uint64*) & val, delta, __ATOMIC_SEQ_CST);
		}
#else
		__atomic_sub_fetch(&val, delta, __ATOMIC_SEQ_CST);
#endif
		return *this;
	}
	constexpr void notify(const uint32_t count = -1) {
#if defined(_MSC_VER)
		if (count == -1) {
			WakeByAddressAll(&val);
		}
		else {
			for (uint32_t i = 0; i < count; ++i) {
				WakeByAddressSingle(&val);
			}
		}
#elif defined(__APPLE__)
    // On macOS, we don't need to do anything for notify
    // The waiting threads will check the condition on their own
    (void)count;
#elif defined(__EMSCRIPTEN__)
		__builtin_wasm_memory_atomic_notify((int*)&val, count);
#elif defined(LINUX)
		syscall(SYS_futex, &val, FUTEX_WAKE_PRIVATE, count, nullptr, nullptr, 0);
#endif
	}
	constexpr bool operator<(const T other) {
		T current = load();
		return current < other;
	}
	constexpr bool operator>(const T other) {
		T current = load();
		return current > other;
	}
	constexpr Atomic& operator++() {
		*this += 1;
		return *this;
	}
	constexpr Atomic operator++(int) {
		Atomic temp = *this;
		++(*this);
		return temp;
	}
	constexpr Atomic& operator--() {
		*this -= 1;
		return *this;
	}
	constexpr Atomic operator--(int) {
		Atomic temp = *this;
		--(*this);
		return temp;
	}
	constexpr bool compare_exchange_strong(T& expected, T desired) {
#ifdef _MSC_VER
		if constexpr (sizeof(T) == 4) {
			return _InterlockedCompareExchange(&val, (uint32)desired, (uint32&)expected) == expected;
		}
		else if constexpr (sizeof(T) == 8) {
			return (T)_InterlockedCompareExchange64((int64*)&val, (int64)desired, (int64&)expected) == expected;
		}
#elif defined(__EMSCRIPTEN__)
		if constexpr (sizeof(T) == 4) {
			return __c11_atomic_compare_exchange_strong((_Atomic T*)&val, &expected, desired, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
		}
		else if constexpr (sizeof(T) == 8) {
			return __c11_atomic_compare_exchange_strong((_Atomic T*)&val, &expected, desired, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
		}
#else
		return __atomic_compare_exchange_n(&val, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
	}
	constexpr bool compare_exchange_weak(T& expected, T desired) {
#ifdef _MSC_VER
		return _InterlockedCompareExchange(&val, desired, expected) == expected;
#elif defined(__EMSCRIPTEN__)
		return __c11_atomic_compare_exchange_weak((_Atomic T*)&val, &expected, desired, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
		return __atomic_compare_exchange_n(&val, &expected, desired, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
	}
};
