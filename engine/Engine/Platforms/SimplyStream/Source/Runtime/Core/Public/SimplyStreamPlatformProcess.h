// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	SimplyStreamProcess.h: KickStart platform Process functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformProcess.h"

/** Dummy process handle for platforms that use generic implementation. */
struct FProcHandle : public TProcHandle<void*, nullptr>
{
public:
	/** Default constructor. */
	FORCEINLINE FProcHandle()
		: TProcHandle()
	{}

	/** Initialization constructor. */
	FORCEINLINE explicit FProcHandle( HandleType Other )
		: TProcHandle( Other )
	{}
};

/**
 * SimplyStream implementation of the Process OS functions
 **/
struct CORE_API FSimplyStreamPlatformProcess : public FGenericPlatformProcess
{
	static const TCHAR* ComputerName();
	static const TCHAR* BaseDir();
	static void Sleep(float Seconds);
	static void SleepNoStats(float Seconds);
	static void SleepInfinite();
	static void wndr_yield();
	static inline void Yield()
	{
		wndr_yield();
	}
	static void YieldCycles(uint64 Cycles);
	static class FEvent* CreateSynchEvent(bool bIsManualReset = 0);
	static class FRunnableThread* CreateRunnableThread();
	static bool SupportsMultithreading();
	static void LaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error );
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
	static bool SkipWaitForStats();
};

typedef FSimplyStreamPlatformProcess FPlatformProcess;
