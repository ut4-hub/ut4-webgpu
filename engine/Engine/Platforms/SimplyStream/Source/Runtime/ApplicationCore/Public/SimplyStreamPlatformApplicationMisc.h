// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

struct APPLICATIONCORE_API FSimplyStreamPlatformApplicationMisc : public FGenericPlatformApplicationMisc
{
	static void PostInit();
	static class GenericApplication* CreateApplication();

	static bool AnchorWindowWindowPositionTopLeft()
	{
		// UE expects mouse coordinates in screen space. SDL/SimplyStream canvas provides in client space.
		return true;
	}
};

typedef FSimplyStreamPlatformApplicationMisc FPlatformApplicationMisc;
