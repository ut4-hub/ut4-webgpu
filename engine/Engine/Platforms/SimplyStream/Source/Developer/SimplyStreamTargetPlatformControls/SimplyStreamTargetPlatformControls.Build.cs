// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SimplyStreamTargetPlatformControls : ModuleRules
{
    public SimplyStreamTargetPlatformControls(ReadOnlyTargetRules Target) : base(Target)
	{
        BinariesSubFolder = "SimplyStream";
		SDKVersionRelevantPlatforms.Add(UnrealTargetPlatform.SimplyStream);

		// We need a short name here since this can run afoul very easily of the `MAX_PATH` limit when
		// combined with building other targets that make use of this.
		ShortName = "SimplyStreamTPCon";

		PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
				"TargetPlatform",
				"DesktopPlatform",
				"Projects",
				"SimplyStreamTargetPlatformSettings",
			}
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings"
			}
		);

		if (Target.bCompileAgainstEngine)
		{
			PublicIncludePathModuleNames.Add("Engine");
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}
	}
}
