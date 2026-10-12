// wndr thread-block probe (2026-07-04). Field freezes park the GAME THREAD in an
// UNTIMED wait (perf-trace evidence: opaque infinite Atomics.wait; every timed/logged
// wait site stayed silent). Each untimed block site marks itself here before waiting;
// a main-thread JS heartbeat (simplystream.js) reads the marker via the exported
// getters when the frame counter stalls and names the site in the user-visible
// console — freezes self-identify instead of needing a CDP/tracing session.
//
// Site ids (keep in sync with the [WndrHeartbeat] decoder string in simplystream.js):
//   0 = not in a probed wait (unknown/other)
//   1 = WebGPU waitOnPipeline (async pipeline compile wait)
//   2 = wgpu proxy push_and_wait / Send-sync (waitOnId on the wgpu method queue)
//   3 = fsQueue/EmQueue waitOnId (FS_QUEUE_ASM_SYNC JS bridge)
//   4 = VFS View::wait_for_ready (asset fetch/decompress wait)
//   5 = INFINITE FEvent::Wait on the game thread (frame-end sync / render fence /
//       task-graph wait — discriminate via gameFrame vs renderFrame vs presentedFrame
//       in the heartbeat report)
//   6 = UNTIMED ParkingLot wait on the game thread (UE::FMutex contention, FEventCount,
//       task waits — the wait family FEvent-factory probing cannot see)
//   8 = MAIN_THREAD_EM_ASM sync proxy from the game thread (blocks until the browser
//       main thread services emscripten's proxy queue — untimed; wrap call sites with
//       WNDR_PROBED_MAIN_THREAD_EM_ASM)
//   9 = emscripten_websocket_send_binary from a pthread (emscripten INTERNALLY sync-proxies
//       websocket ops to the main thread — an unprobed opaque mailbox wait)
#pragma once
#include <atomic>
#include <cstdint>
#include <pthread.h>

inline std::atomic<uint32_t>  GWndrGameThreadProbe{0};
inline std::atomic<uintptr_t> GWndrGameThreadPthread{0};
inline std::atomic<uint32_t>  GWndrNetTravelStage{0};
inline std::atomic<uint64_t>  GWndrWgpuProxyQueued{0};
inline std::atomic<uint64_t>  GWndrWgpuProxyStarted{0};
inline std::atomic<uint64_t>  GWndrWgpuProxyFinished{0};
inline std::atomic<uint32_t>  GWndrWgpuProxyLastQueuedId{0};   // encoded method id: 0 = none, stored id = WebGPU ID + 1
inline std::atomic<uint32_t>  GWndrWgpuProxyCurrentId{0};      // encoded method id: 0 = idle, stored id = WebGPU ID + 1
inline std::atomic<uint32_t>  GWndrWgpuProxyLastFinishedId{0}; // encoded method id: 0 = none, stored id = WebGPU ID + 1
inline std::atomic<uint64_t>  GWndrWgpuQueueSubmitCalls{0};
inline std::atomic<uint64_t>  GWndrWgpuQueueSubmittedCommandBuffers{0};
inline std::atomic<uint64_t>  GWndrWgpuQueueWorkDoneCallbacks{0};
inline std::atomic<uint32_t>  GWndrWgpuQueueLastWorkDoneStatus{0};
inline std::atomic<uint64_t>  GWndrWgpuPresentBegins{0};
inline std::atomic<uint64_t>  GWndrWgpuPresentEnds{0};
inline std::atomic<uint32_t>  GWndrWgpuPresentCurrentStage{0};
inline std::atomic<uint32_t>  GWndrWgpuPresentLastStage{0};
inline std::atomic<uint32_t>  GWndrGameTickPhase{0};
inline std::atomic<uint64_t>  GWndrGameTickEntered{0};
inline std::atomic<uint64_t>  GWndrGameTickEngineDone{0};
inline std::atomic<uint64_t>  GWndrGameTickPlatformDone{0};
inline std::atomic<uint64_t>  GWndrBrowserFrameSyncs{0};
inline std::atomic<uint64_t>  GWndrBrowserXRFrameSyncs{0};
inline std::atomic<uint32_t>  GWndrWgpuProxyIdleWaitUs{1000};
inline std::atomic<uint64_t>  GWndrWgpuProxyIdleWaits{0};
inline std::atomic<uint64_t>  GWndrWgpuProxyIdleTimeouts{0};
inline std::atomic<uint64_t>  GWndrWgpuProxyIdleSignals{0};
inline std::atomic<uint32_t>  GWndrFsProxyIdleWaitUs{1000};
inline std::atomic<uint64_t>  GWndrFsProxyIdleWaits{0};
inline std::atomic<uint64_t>  GWndrFsProxyIdleTimeouts{0};
inline std::atomic<uint64_t>  GWndrFsProxyIdleSignals{0};

enum : uint32_t
{
	WNDR_NET_STAGE_NONE = 0,
	WNDR_NET_STAGE_OSS_JOIN_START = 10,
	WNDR_NET_STAGE_OSS_JOIN_DUPLICATE = 11,
	WNDR_NET_STAGE_OSS_JOIN_ENDPOINT_READY = 12,
	WNDR_NET_STAGE_OSS_JOIN_NO_ENDPOINT = 13,
	WNDR_NET_STAGE_OSS_JOIN_SUCCESS_DELEGATE = 14,
	WNDR_NET_STAGE_OSS_JOIN_FAILURE_DELEGATE = 15,
	WNDR_NET_STAGE_OSS_RESOLVE_CONNECT_STRING = 16,
	WNDR_NET_STAGE_OSS_RESOLVE_CONNECT_STRING_FAILED = 17,
	WNDR_NET_STAGE_OSS_REGISTER_START = 20,
	WNDR_NET_STAGE_OSS_REGISTER_DUPLICATE = 21,
	WNDR_NET_STAGE_OSS_REGISTER_COMPLETE = 22,
	WNDR_NET_STAGE_OSS_UNREGISTER_START = 30,
	WNDR_NET_STAGE_OSS_UNREGISTER_SUPPRESS_FALLBACK = 31,
	WNDR_NET_STAGE_OSS_UNREGISTER_SUPPRESS_PENDING_TRAVEL = 32,
	WNDR_NET_STAGE_OSS_UNREGISTER_HTTP = 33,
	WNDR_NET_STAGE_WT_INIT_CONNECT = 40,
	WNDR_NET_STAGE_WT_CLIENT_OPEN = 41,
	WNDR_NET_STAGE_WT_FALLBACK_SCHEDULED = 42,
	WNDR_NET_STAGE_WT_NETWORK_FAILURE_RETRY = 43,
	WNDR_NET_STAGE_WT_DESTROY = 49,
	WNDR_NET_STAGE_POST_LOAD_MAP = 50,
};

enum : uint32_t
{
	WNDR_WGPU_PRESENT_STAGE_BEGIN = 1,
	WNDR_WGPU_PRESENT_STAGE_WAIT_BLIT_PIPELINE = 10,
	WNDR_WGPU_PRESENT_STAGE_GET_CURRENT_TEXTURE = 20,
	WNDR_WGPU_PRESENT_STAGE_RECORD_BLIT = 30,
	WNDR_WGPU_PRESENT_STAGE_FLUSH_SUBMISSION = 40,
	WNDR_WGPU_PRESENT_STAGE_QUEUE_END_OF_FRAME = 50,
	WNDR_WGPU_PRESENT_STAGE_WAIT_FRAME_READY = 60,
	WNDR_WGPU_PRESENT_STAGE_PLATFORM_PRESENT = 70,
	WNDR_WGPU_PRESENT_STAGE_DONE = 80,
};

enum : uint32_t
{
	WNDR_GAME_TICK_PHASE_IDLE = 0,
	WNDR_GAME_TICK_PHASE_ENGINE_TICK = 10,
	WNDR_GAME_TICK_PHASE_PLATFORM_TICK = 20,
	WNDR_GAME_TICK_PHASE_DO_EVENTS = 21,
	WNDR_GAME_TICK_PHASE_SNAPSHOT = 22,
	WNDR_GAME_TICK_PHASE_SESSION_EVENTS = 23,
	WNDR_GAME_TICK_PHASE_SCHEDULE_NEXT = 30,
};

inline void WndrMarkNetTravelStage(uint32_t Stage)
{
	GWndrNetTravelStage.store(Stage, std::memory_order_relaxed);
}

inline uint32_t WndrEncodeWgpuMethodId(uint16_t MethodId)
{
	return (uint32_t)MethodId + 1u;
}

inline void WndrMarkWgpuProxyQueued(uint16_t MethodId)
{
	GWndrWgpuProxyLastQueuedId.store(WndrEncodeWgpuMethodId(MethodId), std::memory_order_relaxed);
	GWndrWgpuProxyQueued.fetch_add(1, std::memory_order_relaxed);
}

inline void WndrMarkWgpuProxyStarted(uint16_t MethodId)
{
	const uint32_t EncodedId = WndrEncodeWgpuMethodId(MethodId);
	GWndrWgpuProxyCurrentId.store(EncodedId, std::memory_order_relaxed);
	GWndrWgpuProxyStarted.fetch_add(1, std::memory_order_relaxed);
}

inline void WndrMarkWgpuProxyFinished(uint16_t MethodId)
{
	GWndrWgpuProxyLastFinishedId.store(WndrEncodeWgpuMethodId(MethodId), std::memory_order_relaxed);
	GWndrWgpuProxyFinished.fetch_add(1, std::memory_order_relaxed);
	GWndrWgpuProxyCurrentId.store(0, std::memory_order_relaxed);
}

inline void WndrMarkWgpuQueueSubmit(uint32_t CommandBufferCount)
{
	GWndrWgpuQueueSubmitCalls.fetch_add(1, std::memory_order_relaxed);
	GWndrWgpuQueueSubmittedCommandBuffers.fetch_add(CommandBufferCount, std::memory_order_relaxed);
}

inline void WndrMarkWgpuQueueWorkDone(uint32_t Status)
{
	GWndrWgpuQueueLastWorkDoneStatus.store(Status, std::memory_order_relaxed);
	GWndrWgpuQueueWorkDoneCallbacks.fetch_add(1, std::memory_order_relaxed);
}

inline void WndrMarkWgpuPresentStage(uint32_t Stage)
{
	GWndrWgpuPresentCurrentStage.store(Stage, std::memory_order_relaxed);
	GWndrWgpuPresentLastStage.store(Stage, std::memory_order_relaxed);
}

inline void WndrMarkGameTickEnter()
{
	GWndrGameTickPhase.store(WNDR_GAME_TICK_PHASE_ENGINE_TICK, std::memory_order_relaxed);
	GWndrGameTickEntered.fetch_add(1, std::memory_order_relaxed);
}

inline void WndrMarkGameTickEngineDone()
{
	GWndrGameTickEngineDone.fetch_add(1, std::memory_order_relaxed);
	GWndrGameTickPhase.store(WNDR_GAME_TICK_PHASE_PLATFORM_TICK, std::memory_order_relaxed);
}

inline void WndrMarkGameTickPhase(uint32_t Phase)
{
	GWndrGameTickPhase.store(Phase, std::memory_order_relaxed);
}

inline void WndrMarkGameTickPlatformDone()
{
	GWndrGameTickPlatformDone.fetch_add(1, std::memory_order_relaxed);
	GWndrGameTickPhase.store(WNDR_GAME_TICK_PHASE_IDLE, std::memory_order_relaxed);
}

inline void WndrMarkBrowserFrameSync(bool bXR)
{
	GWndrBrowserFrameSyncs.fetch_add(1, std::memory_order_relaxed);
	if (bXR)
	{
		GWndrBrowserXRFrameSyncs.fetch_add(1, std::memory_order_relaxed);
	}
}

struct FWndrWgpuPresentScope
{
	FWndrWgpuPresentScope()
	{
		GWndrWgpuPresentBegins.fetch_add(1, std::memory_order_relaxed);
		WndrMarkWgpuPresentStage(WNDR_WGPU_PRESENT_STAGE_BEGIN);
	}

	~FWndrWgpuPresentScope()
	{
		GWndrWgpuPresentCurrentStage.store(0, std::memory_order_relaxed);
		GWndrWgpuPresentEnds.fetch_add(1, std::memory_order_relaxed);
	}
};

inline bool WndrProbeIsGameThread()
{
	const uintptr_t Gt = GWndrGameThreadPthread.load(std::memory_order_relaxed);
	return Gt != 0 && Gt == (uintptr_t)pthread_self();
}

struct FWndrProbeScope
{
	bool bSet = false;
	uint32_t Prev = 0;
	explicit FWndrProbeScope(uint32_t Site)
	{
		if (WndrProbeIsGameThread())
		{
			Prev = GWndrGameThreadProbe.exchange(Site, std::memory_order_relaxed);
			bSet = true;
		}
	}
	~FWndrProbeScope()
	{
		if (bSet) { GWndrGameThreadProbe.store(Prev, std::memory_order_relaxed); } // restore OUTER site, not 0
	}
};

// Wrap a synchronous MAIN_THREAD_EM_ASM call site so a wedged proxy queue names itself
// (site 8) in the [WndrHeartbeat] stall report. Same arguments as MAIN_THREAD_EM_ASM.
#define WNDR_PROBED_MAIN_THREAD_EM_ASM(...) do { FWndrProbeScope _WndrProbeMT(8); MAIN_THREAD_EM_ASM(__VA_ARGS__); } while (0)
