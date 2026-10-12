// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SimplyStreamTargetPlatformSettings : ModuleRules
{
    public SimplyStreamTargetPlatformSettings(ReadOnlyTargetRules Target) : base(Target)
	{
        BinariesSubFolder = "SimplyStream";
		// We need a short name here since this can run afoul very easily of the `MAX_PATH` limit when
		// combined with building other targets that make use of this.
		ShortName = "SimplyStreamTPSet";

		PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
				"TargetPlatform",
				"DesktopPlatform",
				"Projects"
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
