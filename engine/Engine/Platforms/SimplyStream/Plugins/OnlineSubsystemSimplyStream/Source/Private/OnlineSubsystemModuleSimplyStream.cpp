// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemSimplyStreamModule.h"
#include "OnlineSubsystemSimplyStream.h"
#include "Core.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FOnlineSubsystemSimplyStreamModule, OnlineSubsystemSimplyStream);

/**
 * Class responsible for creating instance(s) of the subsystem
 */
class FOnlineFactorySimplyStream : public IOnlineFactory
{
public:
	FOnlineFactorySimplyStream() {}
	virtual ~FOnlineFactorySimplyStream() {}

	virtual IOnlineSubsystemPtr CreateSubsystem(FName InstanceName)
	{
		FOnlineSubsystemSimplyStreamPtr OnlineSub = MakeShared<FOnlineSubsystemSimplyStream, ESPMode::ThreadSafe>(InstanceName);
		if (OnlineSub->Init())
		{
			return OnlineSub;
		}
		else
		{
			OnlineSub->Shutdown();
			return nullptr;
		}
	}
};

void FOnlineSubsystemSimplyStreamModule::StartupModule()
{
	UE_LOG(LogOnline, Log, TEXT("OnlineSubsystemSimplyStreamModule::StartupModule()"));
	SimplyStreamFactory = new FOnlineFactorySimplyStream();

	// Create and register our singleton factory with the main online subsystem for easy access
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.RegisterPlatformService(SIMPLYSTREAM_SUBSYSTEM, SimplyStreamFactory);
}

void FOnlineSubsystemSimplyStreamModule::ShutdownModule()
{
	UE_LOG(LogOnline, Log, TEXT("OnlineSubsystemSimplyStreamModule::ShutdownModule()"));

	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.UnregisterPlatformService(SIMPLYSTREAM_SUBSYSTEM);
	
	delete SimplyStreamFactory;
	SimplyStreamFactory = nullptr;
}

