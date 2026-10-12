// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CQTest_SimplyStream : CQTest
{
	public CQTest_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		// Lyra's ShooterTestsRuntime references CQTest in Development. Shipping cannot contain
		// non-redistributable Developer modules, even in the generic -AllModules producer.
		PrecompileForTargets = Target.Type == TargetType.Game &&
			Target.Configuration == UnrealTargetConfiguration.Development
			? PrecompileTargetsType.Any
			: PrecompileTargetsType.None;
	}
}
