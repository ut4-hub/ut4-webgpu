// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHI.h"
#include "Modules/ModuleManager.h"

FDynamicRHI* PlatformCreateDynamicRHI()
{
	FDynamicRHI* DynamicRHI = NULL;
	// Load the dynamic RHI module.
#define USE_WEBGPU 1
#if USE_WEBGPU
	IDynamicRHIModule* DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("DawnRHI"));
#else
	IDynamicRHIModule* DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("OpenGLDrv"));
#endif

	// Create the dynamic RHI.
	DynamicRHI = DynamicRHIModule->CreateRHI();
	return DynamicRHI;
}
