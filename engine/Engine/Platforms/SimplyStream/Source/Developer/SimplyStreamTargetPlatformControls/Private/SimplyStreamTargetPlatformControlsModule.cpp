// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "SimplyStreamTargetPlatformControls.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Interfaces/ITargetPlatformControlsModule.h"
#include "../../../Runtime/Core/Public/SimplyStreamPlatformProperties.h"
#include "MultiPlatformTargetReceiptBuildWorkers.h"
#include "SimplyStreamTargetPlatformSettingsModule.h"

#define LOCTEXT_NAMESPACE "FSimplyStreamTargetPlatformControlsModule"

/**
 * Module for the SimplyStream target platform controls.
 */
class FSimplyStreamTargetPlatformControlsModule
	: public ITargetPlatformControlsModule
{
public:
	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms)
	{

	}
	/** Destructor. */
	~FSimplyStreamTargetPlatformControlsModule()
	{
	}
protected:
	virtual void GetTargetPlatformControls(TArray<ITargetPlatformControls*>& TargetPlatforms, FName& PlatformSettingsModuleName)
	{
		//if (FSimplyStreamTargetPlatformControls::IsUsable())
		//{
			// ensure this is only called once
			//check(SinglePlatforms.IsEmpty());

			TMap<FString, ITargetPlatformSettings*> OutMap;
			ITargetPlatformSettings* OutMulti = nullptr;
			ISimplyStreamTargetPlatformSettingsModule* Module = FModuleManager::GetModulePtr<ISimplyStreamTargetPlatformSettingsModule>(PlatformSettingsModuleName);
			if (Module != nullptr)
			{
				Module->GetSinglePlatformSettings(OutMap);
				OutMulti = Module->GetMultiPlatformSettings();
			}

			for (int32 Type = 0; Type < 2; Type++)
			{
				bool bIsClient = Type == 1;

				// flavorless must come first
				// "flavorless" (aka "vanilla") SimplyStream defaults to ASTC
				//SinglePlatforms.Add(new FSimplyStream_DefaultTargetPlatformControls(bIsClient, OutMap[ESimplyStreamTextureFormatCategory::Count]));
				//SinglePlatforms.Add(new FSimplyStream_ASTCTargetPlatformControls(bIsClient, OutMap[ESimplyStreamTextureFormatCategory::ASTC]));
				//SinglePlatforms.Add(new FSimplyStream_DXTTargetPlatformControls(bIsClient, OutMap[ESimplyStreamTextureFormatCategory::DXT]));
				//SinglePlatforms.Add(new FSimplyStream_ETCTargetPlatformControls(bIsClient, OutMap[ESimplyStreamTextureFormatCategory::ETC2]));

				// these are used in NotifyMultiSelectedFormatsChanged, so track in another array
				//MultiPlatforms.Add(new FSimplyStream_MultiTargetPlatformControls(bIsClient, OutMulti));
			}

			// join the single and the multi into one
			//TargetPlatforms.Append(SinglePlatforms);
			TargetPlatforms.Add(new FSimplyStreamTargetPlatformControls<FSimplyStreamPlatformProperties<false, false>>(false, OutMap[FSimplyStreamPlatformProperties<false, false>::PlatformName()], TEXT("")));
			TargetPlatforms.Add(new FSimplyStreamTargetPlatformControls<FSimplyStreamPlatformProperties<false, false>>(false, OutMap[FSimplyStreamPlatformProperties<false, false>::PlatformName() + FString("DXT")], TEXT("DXT")));
			TargetPlatforms.Add(new FSimplyStreamTargetPlatformControls<FSimplyStreamPlatformProperties<false, false>>(false, OutMap[FSimplyStreamPlatformProperties<false, false>::PlatformName() + FString("ETC2")], TEXT("ETC2")));
			TargetPlatforms.Add(new FSimplyStreamTargetPlatformControls<FSimplyStreamPlatformProperties<false, false>>(false, OutMap[FSimplyStreamPlatformProperties<false, false>::PlatformName() + FString("ASTC")], TEXT("ASTC")));
			TargetPlatforms.Add(new FSimplyStreamTargetPlatformControls<FSimplyStreamPlatformProperties<true, false>>(false, OutMap[FSimplyStreamPlatformProperties<true, false>::PlatformName() + FString("Server")], TEXT("")));
			TargetPlatforms.Add(new FSimplyStreamTargetPlatformControls<FSimplyStreamPlatformProperties<false, false>>(false, OutMulti, TEXT("Multi")));
			//TargetPlatforms.Add(new FSimplyStreamTargetPlatformControls<FSimplyStreamPlatformProperties<false, true>>(OutMulti, TEXT("MultiClient")));

			// set up the multi platforms now that we have all the other platforms ready to go
			//NotifyMultiSelectedFormatsChanged();
		//}
	}
#if 0
	virtual void NotifyMultiSelectedFormatsChanged() override
	{
		for (FSimplyStream_MultiTargetPlatformControls* TP : MultiPlatforms)
		{
			TP->LoadFormats(SinglePlatforms);
		}
		// @todo multi needs to be passed this event!
	}
#endif
private:
	/** Holds the specific types of target platforms for NotifyMultiSelectedFormatsChanged */
	//TArray<FSimplyStreamTargetPlatformControls*> SinglePlatforms;
	//TArray<FSimplyStream_MultiTargetPlatformControls*> MultiPlatforms;
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FSimplyStreamTargetPlatformControlsModule, SimplyStreamTargetPlatformControls);