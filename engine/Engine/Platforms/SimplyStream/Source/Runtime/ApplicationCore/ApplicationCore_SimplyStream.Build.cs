// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ApplicationCore_SimplyStream : ApplicationCore
{
	public ApplicationCore_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("SimplyStreamJS");
		PrivateDependencyModuleNames.Add("AtomicQueue");
		PrivateDependencyModuleNames.Add("webgpu_utils");
		//PrivateDependencyModuleNames.Add("Engine");
	}
}
