// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64", "Linux", "Mac")]
public class WebGPUShaderArchiveToolTarget : TargetRules
{
	public WebGPUShaderArchiveToolTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		LaunchModuleName = "WebGPUShaderArchiveTool";
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		bBuildDeveloperTools = false;
		bBuildWithEditorOnlyData = true;
		bCompileAgainstApplicationCore = true;
		bCompileAgainstCoreUObject = false;
		bCompileAgainstEngine = false;
		bCompileICU = false;
		bIsBuildingConsoleApplication = true;
		bWithLiveCoding = false;
		bWithServerCode = false;
	}
}
