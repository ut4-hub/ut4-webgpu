// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "SimplyStreamSDKSettings.generated.h"

class ISimplyStreamTargetPlatformModule;
class ITargetPlatformManagerModule;

USTRUCT()
struct FSimplyStreamDeviceMapping
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = SimplyStream_Devices, Meta = (DisplayName = "Browser name"))
	FString BrowserName;

	UPROPERTY(EditAnywhere, Category = SimplyStream_Devices, Meta = (DisplayName = "Browser filepath"))
	FFilePath BrowserPath;
};

/**
 * Implements the settings for the SimplyStream SDK setup.
 */
UCLASS(config=Engine, globaluserconfig)
class SIMPLYSTREAMPLATFORMEDITOR_API USimplyStreamSDKSettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	// Available browsers that can be used when launching SimplyStream builds.
	TArray<FSimplyStreamDeviceMapping> BrowserLauncher;

#if WITH_EDITOR

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface
	ITargetPlatformManagerModule * TargetManagerModule;
#endif
private:
	ISimplyStreamTargetPlatformModule* TargetPlatformModule;
};
