// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class uLangUE_SimplyStream : uLangUE
{
	public uLangUE_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		// Compiles itself, but depends on uLangCore which does not build for wasm — excluding uLangUE
		// breaks the dependency pull that dragged uLangCore back into -AllModules producer builds
		// (Upgrade/installed_engine_plan.md B1). Nothing on this platform references Verse/uLang.
		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
