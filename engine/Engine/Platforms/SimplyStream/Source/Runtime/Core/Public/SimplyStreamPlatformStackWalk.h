// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	SimplyStreamStackWalk.h: SimplyStream platform stack walk functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformStackWalk.h"

/**
* SimplyStream platform stack walking
*/
struct CORE_API FSimplyStreamPlatformStackWalk : public FGenericPlatformStackWalk
{
	typedef FGenericPlatformStackWalk Parent;

	static void ProgramCounterToSymbolInfo(uint64 ProgramCounter,FProgramCounterSymbolInfo& out_SymbolInfo);
	static uint32 CaptureStackBackTrace(uint64* BackTrace,uint32 MaxDepth,void* Context = nullptr);
	//Additional
	static int32 GetStackBackTraceString(char* OutputString, int32 MaxLen);
};

typedef FSimplyStreamPlatformStackWalk FPlatformStackWalk;
