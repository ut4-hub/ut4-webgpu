// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	SimplyStreamTime.h: SimplyStream platform Time functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformTime.h"
#include "SimplyStreamSystemIncludes.h"

#include <time.h>
#include <emscripten/emscripten.h>

extern double global_start;
#define USE_HP (UE_TRACE_ENABLED && 0)
#if USE_HP
extern std::atomic<uint64_t> highPrecisionCounter;
extern uint64_t highPrecisionFrequency;
#endif
struct CORE_API FSimplyStreamPlatformTime : public FGenericPlatformTime
{

#if !USE_HP
	static double InitTiming()
	{
		SecondsPerCycle = 1.0f / 200000.0f;
		SecondsPerCycle64 = 1.0 / 200000.0;
		return FSimplyStreamPlatformTime::Seconds();
	}

	// for SimplyStream - this returns the time since startup.
	static FORCEINLINE double Seconds()
	{
		return (emscripten_get_now()) * 0.001;
	}

	static FORCEINLINE uint32 Cycles()
	{
		return (uint32)((emscripten_get_now()) * 200.0);
	}
	static FORCEINLINE uint64 Cycles64()
	{
		return (uint64)((emscripten_get_now()) * 200.0);
	}
#else
	static double InitTiming()
	{
		SecondsPerCycle = 1.0f / highPrecisionFrequency;
		SecondsPerCycle64 = 1.0 / highPrecisionFrequency;
		return FSimplyStreamPlatformTime::Seconds();
	}

	static FORCEINLINE double Seconds()
	{
		return (double)highPrecisionCounter.load() * GetSecondsPerCycle() + 16777216.0;
	}

	static FORCEINLINE uint32 Cycles()
	{
		return (uint32)highPrecisionCounter.load();
	}
	static FORCEINLINE uint64 Cycles64()
	{
		return (uint64)highPrecisionCounter.load();
	}
#endif

};

typedef FSimplyStreamPlatformTime FPlatformTime;
