// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Core_SimplyStream : Core
{
	public Core_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = (Target.Platform == UnrealTargetPlatform.Mac) ? CppStandardVersion.Cpp20 : CppStandardVersion.Latest;
		PrivateDependencyModuleNames.Add("SimplyStreamJS");
		PrivateDependencyModuleNames.Add("WndrZSTD");
		PublicDependencyModuleNames.Add("WndrZSTD");
		//PublicDependencyModuleNames.Add("webgpu_utils");
		PublicDefinitions.Add("IS_RUNNING_GAMETHREAD_ON_EXTERNAL_THREAD=1");
	}
}
