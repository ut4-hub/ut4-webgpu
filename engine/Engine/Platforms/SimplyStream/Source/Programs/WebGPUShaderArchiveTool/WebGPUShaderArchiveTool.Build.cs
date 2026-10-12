// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WebGPUShaderArchiveTool : ModuleRules
{
	public WebGPUShaderArchiveTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"Core",
				"Projects",
				"RenderCore",
				"RHI",
			}
		);
	}
}
