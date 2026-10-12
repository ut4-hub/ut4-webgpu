// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SimplyStreamPlatformApplicationMisc.h"
#include "SimplyStreamApplication.h"
#include "Modules/ModuleManager.h"

#ifdef SIMPLYSTREAM_USE_SDL2
THIRD_PARTY_INCLUDES_START
#include "SDL.h"
THIRD_PARTY_INCLUDES_END
#endif

void FSimplyStreamPlatformApplicationMisc::PostInit()
{
	FModuleManager::Get().LoadModule("MapPakDownloader");
}

GenericApplication* FSimplyStreamPlatformApplicationMisc::CreateApplication()
{
	return FSimplyStreamApplication::CreateSimplyStreamApplication();
}
