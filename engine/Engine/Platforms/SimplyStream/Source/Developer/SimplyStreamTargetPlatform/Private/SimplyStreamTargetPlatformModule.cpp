// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interfaces/ITargetPlatformModule.h"
#include "Modules/ModuleManager.h"
#include "Common/TargetPlatformBase.h"
#include "SimplyStreamBulkDataCookedIndex.h"

class FSimplyStreamTargetPlatformModule	: public ITargetPlatformModule
{
public:
	virtual void StartupModule() override
	{
		// This module is loaded whenever the SimplyStream target platform is selected,
		// including commandlet cooks. PlatformEditor is an editor-UI module and is not a
		// reliable cook hook.
		InstallSimplyStreamBulkDataCookedIndexing();
	}

	virtual void ShutdownModule() override
	{
		UninstallSimplyStreamBulkDataCookedIndexing();
	}

	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms) override
	{
	}

	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms, TArray<ITargetPlatformSettings*> TargetPlatformSettings, TArray<ITargetPlatformControls*> TargetPlatformControls)
	{
		for (ITargetPlatformControls* TargetPlatformControlsIt : TargetPlatformControls)
		{
			TargetPlatforms.Add(new FTargetPlatformMerged(TargetPlatformControlsIt->GetTargetPlatformSettings(), TargetPlatformControlsIt));
		}
	}
};


IMPLEMENT_MODULE(FSimplyStreamTargetPlatformModule, SimplyStreamTargetPlatform);
