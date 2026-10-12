// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	SimplyStreamOutputDevices.h: KickStart platform OutputDevices functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformOutputDevices.h"

struct CORE_API FSimplyStreamPlatformOutputDevices : FGenericPlatformOutputDevices
{
	static FOutputDevice*				GetLog();
};

// generic version
typedef FSimplyStreamPlatformOutputDevices FPlatformOutputDevices;
