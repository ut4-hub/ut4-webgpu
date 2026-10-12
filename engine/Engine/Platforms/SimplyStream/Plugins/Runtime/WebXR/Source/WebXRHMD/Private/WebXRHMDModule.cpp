// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebXRHMD.h"
#include "IHeadMountedDisplayModule.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

/**
 * WebXR HMD module. Hard-gated on the -webxr command line argument, which the page only injects
 * when ?webxr=1 was requested AND navigator.xr reports immersive-vr support (simplystream.html).
 * Without it, CreateTrackingSystem returns null and non-XR sessions are completely unaffected —
 * including the disconnected-module fallback in UEngine::InitializeHMDDevice, which would
 * otherwise instantiate the first registered HMD module anyway.
 */
class FWebXRHMDModule : public IHeadMountedDisplayModule
{
public:
	virtual FString GetModuleKeyName() const override
	{
		return TEXT("WebXR");
	}

	virtual bool IsHMDConnected() override
	{
		return FParse::Param(FCommandLine::Get(), TEXT("webxr"));
	}

	virtual TSharedPtr<IXRTrackingSystem, ESPMode::ThreadSafe> CreateTrackingSystem() override
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("webxr")))
		{
			return nullptr;
		}
		UE_LOG(LogHMD, Log, TEXT("WebXR: creating tracking system (-webxr present)"));
		return MakeShared<FWebXRHMD, ESPMode::ThreadSafe>();
	}
};

IMPLEMENT_MODULE(FWebXRHMDModule, WebXRHMD)
