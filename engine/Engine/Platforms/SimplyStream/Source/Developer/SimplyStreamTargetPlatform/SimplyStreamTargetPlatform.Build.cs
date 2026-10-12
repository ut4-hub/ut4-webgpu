// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

using System;
using System.IO;

public class SimplyStreamTargetPlatform : ModuleRules
{
	public SimplyStreamTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "SimplyStream";

		PrecompileForTargets = PrecompileTargetsType.Any;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
//				"Core",
//				"CoreUObject",
//				"Sockets",
//				"TargetPlatform",
//				"DesktopPlatform",
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
				"InputCore",
				"Sockets",
				"TargetPlatform",
				"DesktopPlatform",
				"RHI",
			}
		);

		// SimplyStreamBulkDataCookedIndex.cpp (this module's Private/) #includes
		// Engine/Texture.h UNCONDITIONALLY -- not gated on bBuildRequiresCookedData
		// -- so the Engine dependency below must track bCompileAgainstEngine alone,
		// not be nested inside the bBuildRequiresCookedData check too. Discovered by
		// itpick/ut4-webgpu's DawnShaderFormatTest: a non-cooking Program target
		// with bCompileAgainstEngine=true + bBuildRequiresCookedData=true (real,
		// deliberate combo -- see that target's comments) hit "Engine/Texture.h
		// file not found" here because this dependency was skipped.
		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
//			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}

		// no need for all these modules if the program doesn't want developer tools at all (like UnrealFileServer)
		if (!Target.bBuildRequiresCookedData && Target.bBuildDeveloperTools)
		{
			if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
			{
				DynamicallyLoadedModuleNames.Add("SimplyStreamTargetPlatform");
			}
		}


		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);
	}
}
