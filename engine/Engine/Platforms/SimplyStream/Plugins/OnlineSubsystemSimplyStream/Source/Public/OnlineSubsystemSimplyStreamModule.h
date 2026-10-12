// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

/**
 * Module for the SimplyStream Online Subsystem
 */
class FOnlineSubsystemSimplyStreamModule : public IModuleInterface
{
private:
	/** Class responsible for creating instance(s) of the subsystem */
	class FOnlineFactorySimplyStream* SimplyStreamFactory;

public:
	FOnlineSubsystemSimplyStreamModule() :
		SimplyStreamFactory(nullptr)
	{}

	virtual ~FOnlineSubsystemSimplyStreamModule() {}

	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override { return false; }
	virtual bool SupportsAutomaticShutdown() override { return false; }
};
