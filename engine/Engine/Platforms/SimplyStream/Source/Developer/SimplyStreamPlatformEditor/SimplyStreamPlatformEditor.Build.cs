// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SimplyStreamPlatformEditor : ModuleRules
{
	public SimplyStreamPlatformEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "SimplyStream";

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"InputCore",
				"Engine",
				"Slate",
				"SlateCore",
				"EditorStyle", // needed?
				"EditorWidgets",
				"DesktopWidgets",
				"PropertyEditor",
				"SharedSettingsWidgets",
				"SourceControl",
				"TargetPlatform",
				"SimplyStreamTargetPlatform",
				"SimplyStreamTargetPlatformSettings",
				// WITH_EDITOR only?
				"UnrealEd", // needed?
				"DesktopPlatform",
				"AudioSettingsEditor",
				"RenderCore",
				"RHI",
				"MaterialShaderQualitySettings",
				"MainFrame",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);
	}
}
