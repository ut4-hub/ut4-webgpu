// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

// wndr IAS benchmark (Phase 2) — host-side per-package repartitioner. Standalone console Program (NOT an engine-source
// edit; lives entirely in the SimplyStream platform tree). Mirrors Engine/Source/Programs/IoStoreOnDemand/IasTool so it
// reuses the already-warm Mac dependency closure (Core + IoStoreOnDemandCore). Reads a cooked .utoc/.ucas and emits a
// per-package .uondemandtoc + content-hashed .iopart partitions, the 3rd partition layout for the head-to-head bench
// (vs per-chunk = IasTool -MaxPartitionSize=0, and grouped = IasTool default 2 MiB).
[SupportedPlatforms("Win64", "Linux", "Mac")]
public class WndrRepartitionTarget : TargetRules
{
	public WndrRepartitionTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		LaunchModuleName = "WndrRepartition";
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		bBuildDeveloperTools = false;
		bBuildWithEditorOnlyData = false;
		bCompileAgainstApplicationCore = false;
		bCompileAgainstCoreUObject = false;
		bCompileAgainstEngine = false;
		bCompileICU = false;
		bIsBuildingConsoleApplication = true;
		bForceEnableExceptions = true;
		bWithLiveCoding = false;
		bWithServerCode = false;
	}
}
