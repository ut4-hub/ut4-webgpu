// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CQTestEnhancedInput_SimplyStream : CQTestEnhancedInput
{
	public CQTestEnhancedInput_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		// ShooterTestsRuntime also references this DeveloperTool module. Its base rules build for
		// SimplyStream; explicitly include it in the generic precompiled producer target.
		PrecompileForTargets = PrecompileTargetsType.Any;
	}
}
