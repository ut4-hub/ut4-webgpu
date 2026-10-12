// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class uLangCore_SimplyStream : uLangCore
{
	public uLangCore_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		// Does not build for SimplyStream: std::nullptr_t misuse + unrecognized-platform error in FloatingPointState.cpp.
		// Nothing on this platform references it; keep it out of -AllModules/-Precompile producer builds
		// (Upgrade/installed_engine_plan.md B1).
		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
