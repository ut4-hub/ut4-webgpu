// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// HEADER_UNIT_UNSUPPORTED - Unsupported platform

#include "CoreTypes.h"

#if PLATFORM_USE_PTHREADS

#include "Misc/AssertionMacros.h"
#include <pthread.h>
#include <errno.h>
#include <atomic>

namespace UE
{

/**
 * A shared (read/write) mutex that does not support recursive locking.
 *
 * Prefer FSharedMutex.
 */
class FPThreadsSharedMutex final
{
public:
	FPThreadsSharedMutex(const FPThreadsSharedMutex&) = delete;
	FPThreadsSharedMutex& operator=(const FPThreadsSharedMutex&) = delete;

	FPThreadsSharedMutex()
	{
		int Err = pthread_rwlock_init(&Mutex, nullptr);
		checkf(Err == 0, TEXT("pthread_rwlock_init failed with error: %d"), Err);
	}

	~FPThreadsSharedMutex()
	{
		int Err = pthread_rwlock_destroy(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_destroy failed with error: %d"), Err);
	}

	// WNDR SINGLETHREAD-RECURSION (wasm): with worker threads gone, previously
	// cross-thread lock acquisitions (AssetRegistry mount callbacks, etc.) now
	// re-enter on the SAME thread. pthread_rwlock is NON-recursive -> wrlock
	// returns EBUSY(16) -> checkf fires. Make the WRITE lock recursive per-thread
	// (owner id + depth) and let a read acquired while holding the write pass
	// through (holding write implies read access). Gated to wasm; native is
	// unchanged (locks stay genuinely cross-thread there).
#if PLATFORM_WASM
	bool TryLock()
	{
		const uint64 Self = (uint64)pthread_self();
		if (WriteOwnerId.load(std::memory_order_relaxed) == Self) { ++WriteDepth; return true; }
		int Err = pthread_rwlock_trywrlock(&Mutex);
		if (Err == 0) { WriteOwnerId.store(Self, std::memory_order_relaxed); WriteDepth = 1; return true; }
		return false;
	}

	void Lock()
	{
		const uint64 Self = (uint64)pthread_self();
		if (WriteOwnerId.load(std::memory_order_relaxed) == Self) { ++WriteDepth; return; }
		int Err = pthread_rwlock_wrlock(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_wrlock failed with error: %d"), Err);
		WriteOwnerId.store(Self, std::memory_order_relaxed);
		WriteDepth = 1;
	}

	void Unlock()
	{
		const uint64 Self = (uint64)pthread_self();
		if (WriteOwnerId.load(std::memory_order_relaxed) == Self && WriteDepth > 0)
		{
			if (--WriteDepth == 0)
			{
				WriteOwnerId.store(0, std::memory_order_relaxed);
				int Err = pthread_rwlock_unlock(&Mutex);
				checkf(Err == 0, TEXT("pthread_rwlock_unlock failed with error: %d"), Err);
			}
			return;
		}
		int Err = pthread_rwlock_unlock(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_unlock failed with error: %d"), Err);
	}

	bool TryLockShared()
	{
		if (WriteOwnerId.load(std::memory_order_relaxed) == (uint64)pthread_self()) { return true; }
		int Err = pthread_rwlock_tryrdlock(&Mutex);
		return Err == 0;
	}

	void LockShared()
	{
		if (WriteOwnerId.load(std::memory_order_relaxed) == (uint64)pthread_self()) { return; }
		int Err = pthread_rwlock_rdlock(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_rdlock failed with error: %d"), Err);
	}

	void UnlockShared()
	{
		if (WriteOwnerId.load(std::memory_order_relaxed) == (uint64)pthread_self()) { return; }
		int Err = pthread_rwlock_unlock(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_unlock failed with error: %d"), Err);
	}
#else
	bool TryLock() { int Err = pthread_rwlock_trywrlock(&Mutex); return Err == 0; }
	void Lock() { int Err = pthread_rwlock_wrlock(&Mutex); checkf(Err == 0, TEXT("pthread_rwlock_wrlock failed with error: %d"), Err); }
	void Unlock() { int Err = pthread_rwlock_unlock(&Mutex); checkf(Err == 0, TEXT("pthread_rwlock_unlock failed with error: %d"), Err); }
	bool TryLockShared() { int Err = pthread_rwlock_tryrdlock(&Mutex); return Err == 0; }
	void LockShared() { int Err = pthread_rwlock_rdlock(&Mutex); checkf(Err == 0, TEXT("pthread_rwlock_rdlock failed with error: %d"), Err); }
	void UnlockShared() { int Err = pthread_rwlock_unlock(&Mutex); checkf(Err == 0, TEXT("pthread_rwlock_unlock failed with error: %d"), Err); }
#endif

private:
	pthread_rwlock_t Mutex;
#if PLATFORM_WASM
	std::atomic<uint64> WriteOwnerId{0};
	uint32 WriteDepth = 0;
#endif
};

} // UE

#endif // PLATFORM_USE_PTHREADS
