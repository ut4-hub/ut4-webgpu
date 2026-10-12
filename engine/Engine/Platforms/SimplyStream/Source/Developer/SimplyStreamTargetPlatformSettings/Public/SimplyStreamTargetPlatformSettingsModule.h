// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

/*------------------------------------------------------------------------------------
ISimplyStreamTargetPlatformSettingsModule interface
------------------------------------------------------------------------------------*/

#include "CoreMinimal.h"
#include "Interfaces/ITargetPlatformSettingsModule.h"
#include "SimplyStreamTargetPlatformSettings.h"

class ISimplyStreamTargetPlatformSettingsModule : public ITargetPlatformSettingsModule
{
public:
	virtual void GetSinglePlatformSettings(TMap<FString, ITargetPlatformSettings*>& OutMap) = 0;
	virtual ITargetPlatformSettings* GetMultiPlatformSettings() = 0;
};
