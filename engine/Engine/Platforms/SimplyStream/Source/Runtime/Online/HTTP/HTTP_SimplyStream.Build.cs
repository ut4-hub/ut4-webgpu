// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTTP_SimplyStream : HTTP
{
	public HTTP_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("SimplyStreamJS");
		PrivateDependencyModuleNames.Add("WndrZSTD");
		PrivateDependencyModuleNames.Add("webgpu_utils");
	}
}
