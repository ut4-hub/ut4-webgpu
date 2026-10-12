// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	SimplyStreamMisc.h: SimplyStream platform misc functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMisc.h"
#include "CoreTypes.h"
#include "SimplyStreamDebugLogging.h"
#include "SimplyStreamSystemIncludes.h"
#include <emscripten/emscripten.h>

#define UE_DEBUG_BREAK_IMPL() PLATFORM_BREAK()

#ifdef __EMSCRIPTEN_PTHREADS__
#include <emscripten/threading.h>
#endif

/**
 * SimplyStream implementation of the misc OS functions
 */
struct CORE_API FSimplyStreamMisc : public FGenericPlatformMisc
{
	static void PlatformInit();
	static const TCHAR* GetPlatformFeaturesModuleName();
	static FString GetDefaultLocale();
	static void SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context));
	static EAppReturnType::Type MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption );

	FORCEINLINE static int32 GetMaxPathLength()
	{
		return SIMPLYSTREAM_MAX_PATH;
	}

	static bool GetUseVirtualJoysticks();

	static int32 NumberOfCores();

	static int32 NumberOfWorkerThreadsToSpawn();

	static int32 NumberOfIOWorkerThreadsToSpawn();

	static bool AllowThreadHeartBeat();

	FORCEINLINE static void MemoryBarrier()
	{
#ifdef __EMSCRIPTEN_PTHREADS__
		emscripten_atomic_fence();
#endif
	}

	/** Return true if a debugger is present.
	 * wndr 2026-07-05: this returned TRUE unconditionally, lying to the whole engine:
	 * UE disables the thread hang detector when a debugger is "present" (our field
	 * freezes had zero engine-side hang reports because of this), relaxes net/timeout
	 * paths, and takes ensure/check debug-break branches whose PLATFORM_BREAK here is
	 * log-only — so code expecting a real halt continued past failed invariants.
	 * Nothing attaches a native debugger to field wasm: tell the truth. */
	FORCEINLINE static bool IsDebuggerPresent()
	{
		return false;
	}

	static bool UseRenderThread();

	// Override to return WebGPU shader formats instead of DirectX
	static const TCHAR* GetNullRHIShaderFormat();

	static bool AllowAudioThread()
	{
#ifdef __EMSCRIPTEN_PTHREADS__
		return true;
#else
		return false; // note to self: GenericPlatform is true -- meaning, used fake threads if multithreading is not supported...
#endif
	}

	static bool ShouldDisplayTouchInterfaceOnFakingTouchEvents()
	{
		return false;
	}

	static bool DesktopTouchScreen()
	{
		return true;
	}

	static bool FullscreenSameAsWindowedFullscreen()
	{
		return true;
	}

	static bool AllowLocalCaching()
	{
		return true;
	}

	FORCEINLINE static void Prefetch(const void* Ptr)
	{
		// no-op
	};


	FORCEINLINE static void Prefetch(const void* Ptr, int32 Offset)
	{
		// no-op
	};

	FORCEINLINE static void PrefetchBlock(const void* Ptr, int32 NumBytes)
	{
		// no-op
	}

	static void LocalPrint(const TCHAR* Str);
	static int32 GetMaxRefreshRate();

	static void GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength);
	static FString GetEnvironmentVariable(const TCHAR* VariableName);
	static void SetEnvironmentVar(const TCHAR* VariableName, const TCHAR* Value);
};

typedef FSimplyStreamMisc FPlatformMisc;
