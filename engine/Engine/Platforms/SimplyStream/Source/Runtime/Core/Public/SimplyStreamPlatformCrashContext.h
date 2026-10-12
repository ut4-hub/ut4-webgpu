// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"

struct CORE_API FSimplyStreamPlatformCrashContext : public FGenericCrashContext
{
	FSimplyStreamPlatformCrashContext(ECrashContextType InType, const TCHAR* InErrorMessage)
		: FGenericCrashContext(InType, InErrorMessage)
	{
	}

};

typedef FSimplyStreamPlatformCrashContext FPlatformCrashContext;
