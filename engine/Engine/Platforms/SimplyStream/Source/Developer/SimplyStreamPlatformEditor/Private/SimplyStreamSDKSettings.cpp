// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SimplyStreamSDKSettings.h"
#include "ISimplyStreamTargetPlatformModule.h"
#include "Modules/ModuleManager.h"

//#include "EngineTypes.h"

#define LOCTEXT_NAMESPACE "FSimplyStreamPlatformEditorModule"

DEFINE_LOG_CATEGORY_STATIC(SimplyStreamSDKSettings, Log, All);

USimplyStreamSDKSettings::USimplyStreamSDKSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TargetPlatformModule(nullptr)
{
}

#if WITH_EDITOR
void USimplyStreamSDKSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (!TargetPlatformModule)
	{
		TargetPlatformModule = FModuleManager::LoadModulePtr<ISimplyStreamTargetPlatformModule>("SimplyStreamTargetPlatform");
	}
	if (TargetPlatformModule)
	{
		UpdateGlobalUserConfigFile();
		TargetPlatformModule->RefreshAvailableDevices();
	}
}

#endif //WITH_EDITOR
#undef LOCTEXT_NAMESPACE
