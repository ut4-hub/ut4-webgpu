// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemSimplyStream : ModuleRules
{
	public OnlineSubsystemSimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDefinitions.Add("ONLINESUBSYSTEMSIMPLYSTREAM_PACKAGE=1");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"Json",
				"JsonUtilities",
				"HTTP",
				"Sockets",
			}
		);
	}
}
