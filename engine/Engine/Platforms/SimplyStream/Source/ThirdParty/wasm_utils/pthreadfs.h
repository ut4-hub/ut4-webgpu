#if PLATFORM_WASM
#pragma once
#include "archive.h"
#include "wndr_probe.h"
#include "wndr_js_abi.h"
//#include "tuplet.h"
#include "wasm_atomic.h"
#include "AtomicQueue.h"
#include <atomic>
#include <ranges>
#include <pthread.h>
// replace this with simplystream_proxy
#define MY_FSQ_EM_ASM_0(q, code) ([=]() -> int {auto waitId = q.getWaitId(); q.push([waitId, qPtr = &q]() { EM_ASM({ (async() => { code; _unwaitId2(WNDR_JS_POINTER($0),$1); })(); }, qPtr, waitId); return 1; });return waitId;} ())
#define MY_FSQ_EM_ASM_1(q, code, ...) ([=]() -> int {auto waitId = q.getWaitId(); q.push([=, qPtr = &q]() { EM_ASM({ (async() => { code; _unwaitId2(WNDR_JS_POINTER($1),$2); })(); }, __VA_ARGS__, qPtr, waitId); return 1; });return waitId;} ())
#define MY_FSQ_EM_ASM_2(q, code, ...) ([=]() -> int {auto waitId = q.getWaitId(); q.push([=, qPtr = &q]() { EM_ASM({ (async() => { code; _unwaitId2(WNDR_JS_POINTER($2),$3); })(); }, __VA_ARGS__, qPtr, waitId); return 1; });return waitId;} ())
#define MY_FSQ_EM_ASM_3(q, code, ...) ([=]() -> int {auto waitId = q.getWaitId(); q.push([=, qPtr = &q]() { EM_ASM({ (async() => { code; _unwaitId2(WNDR_JS_POINTER($3),$4); })(); }, __VA_ARGS__, qPtr, waitId); return 1; });return waitId;} ())
#define MY_FSQ_EM_ASM_4(q, code, ...) ([=]() -> int {auto waitId = q.getWaitId(); q.push([=, qPtr = &q]() { EM_ASM({ (async() => { code; _unwaitId2(WNDR_JS_POINTER($4),$5); })(); }, __VA_ARGS__, qPtr, waitId); return 1; });return waitId;} ())
#define MY_FSQ_EM_ASM_5(q, code, ...) ([=]() -> int {auto waitId = q.getWaitId(); q.push([=, qPtr = &q]() { EM_ASM({ (async() => { code; _unwaitId2(WNDR_JS_POINTER($5),$6); })(); }, __VA_ARGS__, qPtr, waitId); return 1; });return waitId;} ())
#define MY_FSQ_EM_ASM_6(q, code, ...) ([=]() -> int {auto waitId = q.getWaitId(); q.push([=, qPtr = &q]() { EM_ASM({ (async() => { code; _unwaitId2(WNDR_JS_POINTER($6),$7); })(); }, __VA_ARGS__, qPtr, waitId); return 1; });return waitId;} ())

#define GET_FS_FIRST_ARG(_0, ...) _0
#define GET_FS_FIRST_ARG_OF_VA_ARGS(...) GET_FS_FIRST_ARG(__VA_ARGS__, dummy)

#define GET_MY_FSQ_EM_ASM(_0,_1,_2,_3,_4,_5,_6,_7,NAME,...) NAME
//#define FS_QUEUE_ASM_ASYNC(q, code, ...) q.push(std::move([=, qPtr = &q]() { EM_ASM({ (async() => { code; })(); }, __VA_ARGS__); return 0; }));
#define FS_QUEUE_ASM_SYNC(...) {const auto waitId = GET_MY_FSQ_EM_ASM(__VA_ARGS__, MY_FSQ_EM_ASM_6, MY_FSQ_EM_ASM_5, MY_FSQ_EM_ASM_4, MY_FSQ_EM_ASM_3, MY_FSQ_EM_ASM_2, MY_FSQ_EM_ASM_1, MY_FSQ_EM_ASM_0)(__VA_ARGS__); GET_FS_FIRST_ARG_OF_VA_ARGS(__VA_ARGS__).waitOnId(waitId); }
#define FS_QUEUE_ASM(...) GET_MY_FSQ_EM_ASM(__VA_ARGS__, MY_FSQ_EM_ASM_6, MY_FSQ_EM_ASM_5, MY_FSQ_EM_ASM_4, MY_FSQ_EM_ASM_3, MY_FSQ_EM_ASM_2, MY_FSQ_EM_ASM_1, MY_FSQ_EM_ASM_0)(__VA_ARGS__);
#if PLATFORM_WASM || 1
struct EmQueue {
	using MethodQueue = atomic_queue::AtomicQueue2<std::function<int()>, 4096, true, true, true, false>;
	MethodQueue method_queue;
	Atomic<uint32_t> waitId; // u32s are gaurenteed to be lock free
	Atomic<uint32_t>* waitIds = new Atomic<uint32_t>[65536]; // 262.14kb, can reduce by 32 times by using AtomicBoolBitSet, but measure performance first
	Atomic<uint32_t> activeThreads;
	Atomic<uint32_t> total_actions_in_flight;
	Atomic<uint32_t> idleWakeEpoch;
	const uint32_t actionsPerThread = 50;
	const uint32_t totalActions = 200;
	static thread_local uint32_t actions_in_flight_for_thread;
	EmQueue(const EmQueue&) = delete;
	EmQueue(EmQueue&&) = delete;
	EmQueue& operator=(const EmQueue&) = delete;
	EmQueue& operator=(EmQueue&&) = delete;

	void unwaitId(uint16_t id) {
		waitIds[id] = 0;
		waitIds[id].notify();
	}
	uint16_t getWaitId() {
		auto id = ++waitId;
		waitIds[id & 0xFFFF] = 1; // because using uint32_t, we need to mask it, otherwise we can use uint16_t but no gaurentee of lock free
		return id;
	}
	void waitOnId(uint16_t id) {
		FWndrProbeScope WndrProbe(3); // untimed JS-bridge wait — see wndr_probe.h
		waitIds[id].wait(1);
	}
	void push(std::function<int()> func) {
		method_queue.push(std::move(func));
		idleWakeEpoch.fetch_add(1);
		idleWakeEpoch.notify(1);
	}
	uint32_t GetIdleWaitUs() const;
	void our_proxy_loop() {
		//static thread_local pthread_t tid = pthread_self();
		if (actions_in_flight_for_thread > actionsPerThread || total_actions_in_flight > totalActions) {
			EM_ASM(Module.channel.port2.postMessage(""));
			return;
		}
		std::function<int()> evt;
		const uint32_t ObservedEpoch = idleWakeEpoch.load();
		if (method_queue.try_pop(evt)) {
			uint32_t curActionsForThread = actions_in_flight_for_thread++;
			uint32_t curActions = total_actions_in_flight++;
			//wonder_log::info("actions in flight for thread: {}, total actions in flight: {}", curActionsForThread, curActions);
			evt();
		}
		else {
			const uint32_t IdleWaitUs = GetIdleWaitUs();
			GWndrFsProxyIdleWaitUs.store(IdleWaitUs, std::memory_order_relaxed);
			if (IdleWaitUs > 0) {
				GWndrFsProxyIdleWaits.fetch_add(1, std::memory_order_relaxed);
				const WaitResult Result = idleWakeEpoch.wait(
					ObservedEpoch,
					std::chrono::microseconds(IdleWaitUs));
				if (Result == WaitResult::TimedOut) {
					GWndrFsProxyIdleTimeouts.fetch_add(1, std::memory_order_relaxed);
				}
				else {
					GWndrFsProxyIdleSignals.fetch_add(1, std::memory_order_relaxed);
				}
			}
		}
		// Always yield to JS so fetch/promise callbacks run at least once per millisecond.
		EM_ASM(Module.channel.port2.postMessage(""));
	}
	void* ThreadMain();
	static void* ThreadMainStatic(void* args) {
		return ((EmQueue*)args)->ThreadMain();
	}
	void create_new_thread() {
		activeThreads++;
		pthread_t our_proxy_thread;
		pthread_create(&our_proxy_thread, 0, &EmQueue::ThreadMainStatic, this);
	}
	void destroy_thread(pthread_t our_proxy_thread) {
		checkNoEntry();
#if 0
		wonder_log::info("destroying thread");
		activeThreads--;
		if (activeThreads == 0) {
			create_new_thread();
		} else {
			auto actions = method_queue.was_size();
			auto need_new_thread = actions / actionsPerThread - activeThreads;
			if (need_new_thread) {
				create_new_thread();
			}
		}
		pthread_exit(0);
#endif
	}
	bool start_our_proxy_thread() {
		// start with 4 threads, can add more if needed at runtime
		pthread_t our_proxy_thread[4];
		pthread_create(&our_proxy_thread[0], 0, &EmQueue::ThreadMainStatic, this);
		// if we want multiple threads again, measure performance, and split the session ws into its own em_queue
		//pthread_create(&our_proxy_thread[1], 0, &EmQueue::ThreadMainStatic, this);
		//pthread_create(&our_proxy_thread[2], 0, &EmQueue::ThreadMainStatic, this);
		//pthread_create(&our_proxy_thread[3], 0, &EmQueue::ThreadMainStatic, this);
		return true;
	};
	void Init() {
		start_our_proxy_thread();
	}
	EmQueue() {
	}
};
#endif
#endif
