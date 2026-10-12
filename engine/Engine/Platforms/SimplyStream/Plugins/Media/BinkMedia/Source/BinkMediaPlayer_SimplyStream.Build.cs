// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BinkMediaPlayer_SimplyStream : BinkMediaPlayer
{
	public BinkMediaPlayer_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		// Does not build for SimplyStream: pointer-to-UINTa truncation on wasm32, and BinkUnrealSimplyStream.a does not exist.
		// Nothing on this platform references it; keep it out of -AllModules/-Precompile producer builds
		// (Upgrade/installed_engine_plan.md B1).
		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
