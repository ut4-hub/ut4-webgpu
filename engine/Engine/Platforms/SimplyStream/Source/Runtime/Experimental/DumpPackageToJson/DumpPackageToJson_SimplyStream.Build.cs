// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DumpPackageToJson_SimplyStream : DumpPackageToJson
{
	public DumpPackageToJson_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		// Does not build for SimplyStream: uses C++ `try`; exceptions are force-disabled on this platform.
		// Nothing on this platform references it; keep it out of -AllModules/-Precompile producer builds
		// (Upgrade/installed_engine_plan.md B1).
		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
