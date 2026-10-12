// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ITargetPlatformModule.h"
/**
 * Interface for SimplyStreamTargetPlatformModule module.
 */
class ISimplyStreamTargetPlatformModule
	: public ITargetPlatformModule
{
public:
	/**
	 * Refresh the list of SimplyStream browsers that exist on the system
	 */
	virtual void RefreshAvailableDevices() = 0;

};
