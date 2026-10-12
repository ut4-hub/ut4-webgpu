// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SimplyStreamMisc.cpp: SimplyStream implementations of misc functions
=============================================================================*/

#include "SimplyStreamPlatformMisc.h"
#include "SimplyStreamPlatformProcess.h"
#include "SimplyStreamJavaScriptFx.h"
#include "Containers/StringConv.h"
#include "Misc/ScopeLock.h"
#include <emscripten/trace.h>

THIRD_PARTY_INCLUDES_START
	//#include "unicode/locid.h"
THIRD_PARTY_INCLUDES_END
#include "GenericPlatform/GenericPlatformCrashContext.h"
THIRD_PARTY_INCLUDES_START
	#include <ctime>
#include "../Public/SimplyStreamPlatformMisc.h"
//#include <OpenGLDrv/Public/gl_helper.h>
THIRD_PARTY_INCLUDES_END

//#include "archive.h"
#undef min
#undef max
#include <algorithm>
#include "Misc/CoreMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include <cstdio>

extern bool IsUsingLowCoreCount();
void FSimplyStreamMisc::PlatformInit()
{
	UE_LOG(LogInit, Log, TEXT("SimplyStreamUsingLowCores: %d"), IsUsingLowCoreCount());
	// Identity.
#if 0
	UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName());
	UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName());
	UE_LOG(LogInit, Log, TEXT("Current Culture: %s"), *FSimplyStreamMisc::GetDefaultLocale());

	// Timer resolution.
	UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle());
#endif
}

// Defines the PlatformFeatures module name for SimplyStream, used by PlatformFeatures.h.
const TCHAR* FSimplyStreamMisc::GetPlatformFeaturesModuleName()
{
	return TEXT("SimplyStreamPlatformFeatures");
}

FString FSimplyStreamMisc::GetDefaultLocale()
{
	char AsciiCultureName[512];
	//icu::Locale ICUDefaultLocale = icu::Locale::getDefault();
	//return FString(ICUDefaultLocale.getName());
	return FString("");
}

EAppReturnType::Type FSimplyStreamMisc::MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
{
	UE_LOG(LogTemp, Error, TEXT("MessageBox type %d Caption %s Text %s"), (int)MsgType, Text, Caption);
	return static_cast<EAppReturnType::Type>(1);
}

void (*GSimplyStreamCrashHandler)(const FGenericCrashContext& Context) = nullptr;

extern "C"
{
	// callback from javascript.
	void on_fatal(const char* msg, const char* error)
	{
#ifdef __EMSCRIPTEN_TRACING__
		emscripten_log(EM_LOG_CONSOLE, "Fatal Error: Closing trace!");
		emscripten_trace_close();
#endif
		// !!JM todo: pass msg & error to a crash context? Must be copied?
		if (GSimplyStreamCrashHandler)
		{
			FString Message = FString::Printf(TEXT("Fatal error: %s"), ANSI_TO_TCHAR(msg));
			FGenericCrashContext Ctx(ECrashContextType::Crash, *Message);
			GSimplyStreamCrashHandler(Ctx);
		}
	}
}

void FSimplyStreamMisc::SetCrashHandler(void(* CrashHandler)(const FGenericCrashContext& Context)) {
	GSimplyStreamCrashHandler = CrashHandler;
}

bool FSimplyStreamMisc::GetUseVirtualJoysticks()
{
	return false;
}
int32 FSimplyStreamMisc::NumberOfCores()
{
	// WNDR SINGLETHREAD: report 1 core so UE runs single-threaded on wasm.
	// The prior 4 spawned task-graph/LowLevelTasks + IO worker threads that
	// raced the render/RHI thread -> intermittent refcount-underflow /
	// stack-cookie / ensure-fail crashes during init. Truly single-threaded
	// (matches -onethread intent) eliminates the systemic concurrency race.
	return 1;
	if (IsUsingLowCoreCount())
		return 2;
	else
		return 4;
	int core_count = EM_ASM_INT({
		return wonder_num_logical_cores() || 1;
		});
	return std::min(core_count, 8);
}

int32 FSimplyStreamMisc::NumberOfWorkerThreadsToSpawn()
{
	return 0; // WNDR SINGLETHREAD: no task-graph/LowLevelTasks workers -> tasks run on the calling thread, no RHI race
	if (IsUsingLowCoreCount())
		return 2;
	else
		return 4;
	//int32 _NumberOfCores = NumberOfCores();
	//return std::max(_NumberOfCores - 1, 1);
}

int32 FSimplyStreamMisc::NumberOfIOWorkerThreadsToSpawn()
{
	if (IsUsingLowCoreCount())
		return 2;
	else {
		auto t = NumberOfWorkerThreadsToSpawn();
		if (t <= 2)
			return 2;
		else
			return 4;
	}
}

bool FSimplyStreamMisc::AllowThreadHeartBeat()
{
	return false;
}

void FSimplyStreamMisc::LocalPrint(const TCHAR* Str) {
	if (Str)
	{
		std::fprintf(stderr, "%s", TCHAR_TO_UTF8(Str));
		std::fflush(stderr);
	}
}
bool FSimplyStreamMisc::UseRenderThread()
{
	// Dedicated servers should not use a rendering thread
	if (IsRunningDedicatedServer())
	{
		return false;
	}
	return true;
}

const TCHAR* FSimplyStreamMisc::GetNullRHIShaderFormat()
{
	// SimplyStream uses WebGPU shader formats, not DirectX
	// Check command line for explicit shader model request
	if (FParse::Param(FCommandLine::Get(), TEXT("sm5")))
	{
		return TEXT("SF_WEBGPU_SM5");
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("es31")) ||
	         FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES31")) ||
	         FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES3_1")))
	{
		return TEXT("SF_WEBGPU_ES31");
	}
	else
	{
		// Limit to the highest SM supported by the project
		TArray<FString> TargetedShaderFormats;
		GConfig->GetArray(TEXT("/Script/SimplyStreamRuntimeSettings.SimplyStreamRuntimeSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);

		// Prefer SM5 if available
		if (TargetedShaderFormats.Contains(TEXT("SF_WEBGPU_SM5")))
		{
			return TEXT("SF_WEBGPU_SM5");
		}
		// Fall back to ES31 if that's what's targeted
		if (TargetedShaderFormats.Contains(TEXT("SF_WEBGPU_ES31")))
		{
			return TEXT("SF_WEBGPU_ES31");
		}
	}

	// Default to SM5 which is more widely supported
	return TEXT("SF_WEBGPU_SM5");
}

int32 FSimplyStreamMisc::GetMaxRefreshRate()
{
	int32 Result = FGenericPlatformMisc::GetMaxRefreshRate();

	// always at least 60Hz
	if (Result < 60)
	{
		Result = 60;
	}

	// get result from raf

	return Result;
}

void FSimplyStreamMisc::GetEnvironmentVariable(const TCHAR* InVariableName, TCHAR* Result, int32 ResultLength)
{
	FString VariableName = InVariableName;
	VariableName.ReplaceInline(TEXT("-"), TEXT("_"));
	ANSICHAR* AnsiResult = getenv(TCHAR_TO_ANSI(*VariableName));
	if (AnsiResult)
	{
		FCString::Strncpy(Result, UTF8_TO_TCHAR(AnsiResult), ResultLength);
	}
	else
	{
		*Result = 0;
	}
}

FString FSimplyStreamMisc::GetEnvironmentVariable(const TCHAR* InVariableName)
{
	FString VariableName = InVariableName;
	VariableName.ReplaceInline(TEXT("-"), TEXT("_"));
	ANSICHAR* AnsiResult = getenv(TCHAR_TO_ANSI(*VariableName));
	if (AnsiResult)
	{
		return UTF8_TO_TCHAR(AnsiResult);
	}
	else
	{
		return FString();
	}
}

void FSimplyStreamMisc::SetEnvironmentVar(const TCHAR* InVariableName, const TCHAR* Value)
{
	FString VariableName = InVariableName;
	VariableName.ReplaceInline(TEXT("-"), TEXT("_"));
	if (Value == NULL || Value[0] == TEXT('\0'))
	{
		unsetenv(TCHAR_TO_ANSI(*VariableName));
	}
	else
	{
		setenv(TCHAR_TO_ANSI(*VariableName), TCHAR_TO_ANSI(Value), 1);
	}
}
