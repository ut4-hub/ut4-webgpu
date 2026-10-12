// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VerseCompiler_SimplyStream : VerseCompiler
{
	public VerseCompiler_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		// Does not build for SimplyStream: depends on uLangCore, which does not compile for wasm.
		// Nothing on this platform references it; keep it out of -AllModules/-Precompile producer builds
		// (Upgrade/installed_engine_plan.md B1).
		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
