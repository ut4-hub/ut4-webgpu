// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SimplyStreamPlatformFeatures : ModuleRules
{
	public SimplyStreamPlatformFeatures(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",          // wndr IAS bench: FCoreUObjectDelegates::PostLoadMapWithWorld (stats auto-dump)
				"Engine",
				"RenderCore",           // FShaderCodeLibrary::AddKnownChunkIDs for BootSet Chunk0 material shaders
				"webgpu_utils",
				"IoStoreOnDemandCore",  // wndr IAS (M2): IHttpIoDispatcher / FHttpIoDispatcher install seam
				"IoStoreOnDemand",      // wndr IAS (M6): force-load to bring up FOnDemandIoStore (mount the on-demand TOC)
				"PakFile",              // wndr IAS (BootSet P4 fix): UE::IoStore::MakeFileIoDispatcherBackend() + FFilePackageStoreBackend (PakFile/Private; non-exported, link via monolithic WASM)
			});

		// wndr IAS (BootSet P4 fix): the plugin-owned synchronous boot-container mount mirrors
		// FPakPlatformFile::InitializeNewAsyncIO, which needs PakFile/Private headers (FileIoDispatcherBackend.h,
		// IoDispatcherFileBackend.h, FilePackageStore.h). Pulling engine headers is allowed; only EDITING engine
		// files outside this plugin is not.
		PrivateIncludePaths.Add(System.IO.Path.Combine(EngineDirectory, "Source/Runtime/PakFile/Private"));
		// FileIoDispatcherBackend.h -> IO/IoDispatcherBackend.h UNCONDITIONALLY includes the Source-rooted
		// "Runtime/Core/Internal/IO/IoContainers.h" (+ IoOffsetLength.h). Engine/Source is normally already a global
		// include path for every module, so this is belt-and-suspenders to guarantee those Source-rooted Internal
		// includes resolve regardless of build-settings drift. Harmless if redundant (UBT de-dupes include paths).
		PrivateIncludePaths.Add(System.IO.Path.Combine(EngineDirectory, "Source"));
		// The backend-select guard includes the Core/Internal-rooted "IO/PlatformIoDispatcher.h"
		// (UE::FPlatformIoDispatcher::TryGet) — that spelling resolves only with Core's Internal dir on the search
		// path. bTreatAsEngineModule should grant it, but add it explicitly so the build can't miss it.
		PrivateIncludePaths.Add(System.IO.Path.Combine(EngineDirectory, "Source/Runtime/Core/Internal"));
	}
}
