// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformManualResetEvent.h"
// Relative include on purpose: this header is consumed by engine Core (ParkingLot.cpp),
// whose include paths don't reach wasm_utils; a file-relative path always resolves.
#include "../../../../ThirdParty/wasm_utils/wndr_probe.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace UE::HAL::Private
{

// wndr probe v3 (2026-07-04): ParkingLot waits (UE::FMutex, FEventCount, task waits, …)
// do NOT go through the FEvent factory, so the field freeze reported blockSite=0 with
// site 5 armed while the perf trace showed a ParkingLot park. Wrap the generic
// manual-reset event (the ParkingLot's wait primitive on this platform) and mark
// UNTIMED game-thread waits as block-site 6. Timed waits pass through unprobed —
// self-recovering by construction.
//
// wndr probe v4 (2026-08-16): site-6 stalls now SELF-IDENTIFY. The heartbeat can only
// say "blockSite=6 (some ParkingLot wait)"; it cannot say WHICH FMutex/FEventCount/task
// wait parked the game thread. So an untimed game-thread wait first runs a bounded
// 3s wait; if that trips, the (still-running, not-yet-parked) game thread dumps its own
// C+JS call stack to the browser console via emscripten_log — naming the exact wait
// site — and only then falls into the untimed park. If the wait later completes, that
// is logged too (distinguishes "slow" from "dead"). Semantics are unchanged: the
// generic event is state-based (bWait), so a timed wait followed by an untimed wait
// is exactly equivalent to one untimed wait.
class FWndrProbedManualResetEvent
{
public:
	FWndrProbedManualResetEvent() = default;
	FWndrProbedManualResetEvent(const FWndrProbedManualResetEvent&) = delete;
	FWndrProbedManualResetEvent& operator=(const FWndrProbedManualResetEvent&) = delete;

	void Reset() { Inner.Reset(); }
	bool Poll() { return Inner.Poll(); }

	void Wait()
	{
		ProbedUntimedWait();
	}

	bool WaitFor(FMonotonicTimeSpan WaitTime)
	{
		if (WaitTime.IsInfinity())
		{
			ProbedUntimedWait();
			return true;
		}
		return Inner.WaitFor(WaitTime);
	}

	bool WaitUntil(FMonotonicTimePoint WaitTime)
	{
		if (WaitTime.IsInfinity())
		{
			ProbedUntimedWait();
			return true;
		}
		return Inner.WaitUntil(WaitTime);
	}

	void Notify() { Inner.Notify(); }

private:
	void ProbedUntimedWait()
	{
		FWndrProbeScope WndrProbe(6);
#if defined(__EMSCRIPTEN__)
		if (WndrProbeIsGameThread())
		{
			if (Inner.WaitFor(FMonotonicTimeSpan::FromSeconds(3.0)))
			{
				return;
			}
			// Not woken within 3s: this is (or is about to become) the blockSite=6
			// field stall. Name the wait site with our own call stack, then park.
			// V8 defaults Error.stackTraceLimit to 10 frames, which truncated the
			// dump exactly at the interesting caller — raise it in THIS worker's
			// JS context before capturing.
			EM_ASM({ Error.stackTraceLimit = 96; });
			emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK | EM_LOG_JS_STACK,
				"[WndrSite6] game-thread UNTIMED ParkingLot wait exceeded 3s (event=%p) — call stack of the blocked wait follows",
				(void*)this);
			Inner.Wait();
			emscripten_log(EM_LOG_ERROR,
				"[WndrSite6] the >3s game-thread ParkingLot wait (event=%p) eventually completed — slow, not dead",
				(void*)this);
			return;
		}
#endif
		Inner.Wait();
	}

	FGenericPlatformManualResetEvent Inner;
};

} // UE::HAL::Private

namespace UE { using FPlatformManualResetEvent = UE::HAL::Private::FWndrProbedManualResetEvent; }
