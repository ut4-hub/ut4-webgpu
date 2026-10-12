// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SimplyStreamProcess.cpp: SimplyStream implementations of Process functions
=============================================================================*/

#include "SimplyStreamPlatformProcess.h"
#ifdef __EMSCRIPTEN_PTHREADS__
#include <unistd.h>
#include <sys/time.h>
#include <emscripten/threading.h>
#include "HAL/PThreadEvent.h"
#endif
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreStats.h"
#include "Misc/App.h"
#include "Misc/SingleThreadEvent.h"
#include "Misc/CoreDelegates.h"

#include <emscripten/emscripten.h>
#include <string>

const TCHAR* FSimplyStreamPlatformProcess::ComputerName()
{
	return TEXT("Browser");
}

const TCHAR* FSimplyStreamPlatformProcess::BaseDir()
{
	return TEXT("");
}

DECLARE_CYCLE_STAT(TEXT("CPU Stall - SimplyStreamSleep"),STAT_SimplyStreamSleep,STATGROUP_CPUStalls);

void FSimplyStreamPlatformProcess::Sleep( float Seconds )
{
	SCOPE_CYCLE_COUNTER(STAT_SimplyStreamSleep);
	//FThreadIdleStats::FScopeIdle Scope;
	SleepNoStats(Seconds);
}
thread_local std::atomic<int> futexVar(0);
extern "C" {
	extern void __emscripten_atomics_sleep(double);
}
void FSimplyStreamPlatformProcess::SleepNoStats(float Seconds)
{
	uint32 Milliseconds = (uint32)(Seconds * 1000.0);
	if (Milliseconds == 0)
	{
		//EM_ASM({Atomics.pause();});
		return;
	}
	__emscripten_atomics_sleep(Milliseconds);
#if 0
	usleep(Seconds * 1000); return; // comment this out to use futex based sleep, is the below the same?
	if (Milliseconds == 0)
	{
		// maybe just yield?
		emscripten_futex_wait(&futexVar, 0, Milliseconds);
	}
	else
	{
		emscripten_futex_wait(&futexVar, 0, Milliseconds);
	}
#endif
}

void FSimplyStreamPlatformProcess::SleepInfinite()
{
	// stop this thread forever
#ifdef __EMSCRIPTEN_PTHREADS__
	if (emscripten_is_main_browser_thread())
	{
		checkNoEntry();
	}
	emscripten_futex_wait(&futexVar, 0, INFINITY);
#endif
}
void FSimplyStreamPlatformProcess::wndr_yield()
{
	//EM_ASM({Atomics.pause();});
}
// best we can do in the browser is pause
void FSimplyStreamPlatformProcess::YieldCycles(uint64 Cycles)
{
	//EM_ASM({ Atomics.pause($0); }, (uint32)Cycles);
}
#include "SimplyStreamPlatformRunnableThread.h"

FRunnableThread* FSimplyStreamPlatformProcess::CreateRunnableThread()
{
#ifdef __EMSCRIPTEN_PTHREADS__
	return new FSimplyStreamRunnablePThread();
#else
	return new FSimplyStreamRunnableThread();
#endif
}

#include "../../../ThirdParty/wasm_utils/wndr_probe.h"
// Probed event: an INFINITE Wait on the GAME thread marks block-site 5 so the
// [WndrHeartbeat] monitor can name it when the frame counter stalls (field freezes
// park the game thread in exactly such a wait — perf-trace evidence). Timed waits
// pass through unprobed: they are self-recovering by construction.
class FWndrProbedEvent final : public FPThreadEvent
{
public:
	virtual bool Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats = false) override
	{
		if (WaitTime == MAX_uint32 && WndrProbeIsGameThread())
		{
			FWndrProbeScope WndrProbe(5);
			return FPThreadEvent::Wait(WaitTime, bIgnoreThreadIdleStats);
		}
		return FPThreadEvent::Wait(WaitTime, bIgnoreThreadIdleStats);
	}
};

class FEvent* FSimplyStreamPlatformProcess::CreateSynchEvent( bool bIsManualReset /*= 0*/ )
{
#ifdef __EMSCRIPTEN_PTHREADS__
	FEvent* Event = NULL;
	if (FPlatformProcess::SupportsMultithreading())
	{
		// Allocate the new object
		Event = new FWndrProbedEvent();
	}
	else
	{
		// Fake event.
		Event = new FSingleThreadEvent();
	}
	// If the internal create fails, delete the instance and return NULL.
	// wndr 5.8: FEvent::Create is deprecated for *direct* use (callers should use the pool), but this IS the platform's
	// event factory (the one place allowed to construct + Create an event) — same as FGenericPlatformProcess::CreateSynchEvent.
	// Suppress the deprecation just around the intended call.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (!Event->Create(bIsManualReset))
	{
		delete Event;
		Event = NULL;
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	return Event;
#else
	FEvent* Event = new FSingleThreadEvent();
	return Event;
#endif
}

bool FSimplyStreamPlatformProcess::SupportsMultithreading()
{
	return true;
}

void FSimplyStreamPlatformProcess::LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error)
{
	if (FCoreDelegates::ShouldLaunchUrl.IsBound() && !FCoreDelegates::ShouldLaunchUrl.Execute(URL))
	{
		if (Error)
		{
			*Error = TEXT("LaunchURL cancelled by delegate");
		}
		return;
	}

	auto TmpURL = StringCast<ANSICHAR>(URL);
}

const TCHAR* FSimplyStreamPlatformProcess::ExecutableName(bool bRemoveExtension)
{
	// For SimplyStream, try to get the executable name from the project name set by setProjectStuff
	// If FApp already has a project name set, use that
	if (FApp::HasProjectName())
	{
		return FApp::GetProjectName();
	}

	// Otherwise, use the project_name from setProjectStuff
	extern std::string project_name;
	static FString CachedExecutableName;

	if (CachedExecutableName.IsEmpty() && !project_name.empty())
	{
		CachedExecutableName = FString(ANSI_TO_TCHAR(project_name.c_str()));
	}

	if (CachedExecutableName.IsEmpty())
	{
		CachedExecutableName = TEXT("UnrealGame");
	}

	return *CachedExecutableName;
}

bool FSimplyStreamPlatformProcess::SkipWaitForStats()
{
	return false;
}
