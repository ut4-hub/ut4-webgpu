// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WebXRHMD : ModuleRules
{
	public WebXRHMD(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"HeadMountedDisplay",
				"XRBase",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RHI",
				"RenderCore",
				"Renderer",
				"Slate",
				"SlateCore",
			}
		);
	}
}
