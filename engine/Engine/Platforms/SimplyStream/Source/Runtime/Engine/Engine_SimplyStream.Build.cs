// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Engine_SimplyStream : Engine
{
	public Engine_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		// remove some unsupported formats

		PublicDependencyModuleNames.Add("SimplyStreamJS");
		PublicDependencyModuleNames.Add("WndrZSTD");
	}
}
