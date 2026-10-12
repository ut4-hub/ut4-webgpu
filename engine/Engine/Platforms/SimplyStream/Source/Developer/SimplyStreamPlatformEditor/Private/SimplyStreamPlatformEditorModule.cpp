// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SimplyStreamTargetSettings.h"
#include "SimplyStreamTargetSettingsCustomization.h"
#include "ISettingsModule.h"
#include "SimplyStreamSDKSettings.h"


#define LOCTEXT_NAMESPACE "FSimplyStreamPlatformEditorModule"


/**
 * Module for SimplyStream platform editor utilities
 */
class FSimplyStreamPlatformEditorModule
	: public IModuleInterface
{
	// IModuleInterface interface

	virtual void StartupModule() override
	{
		// register settings
		static FName PropertyEditor("PropertyEditor");
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
		PropertyModule.RegisterCustomClassLayout(
			"SimplyStreamTargetSettings",
			FOnGetDetailCustomizationInstance::CreateStatic(&FSimplyStreamTargetSettingsCustomization::MakeInstance)
			);

		PropertyModule.NotifyCustomizationModuleChanged();

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "SimplyStream",
				LOCTEXT("TargetSettingsName", "SimplyStream"),
				LOCTEXT("TargetSettingsDescription", "Settings for SimplyStream"),
				GetMutableDefault<USimplyStreamTargetSettings>()
			);
		}
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "SimplyStream");
//			SettingsModule->UnregisterSettings("Project", "Platforms", "SimplyStreamSDK");
		}
	}
};


IMPLEMENT_MODULE(FSimplyStreamPlatformEditorModule, SimplyStreamPlatformEditor);

#undef LOCTEXT_NAMESPACE
