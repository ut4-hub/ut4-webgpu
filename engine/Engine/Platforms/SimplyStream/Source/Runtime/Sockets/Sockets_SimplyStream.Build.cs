// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Sockets_SimplyStream : Sockets
{
	public Sockets_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("PLATFORM_SOCKETSUBSYSTEM=FName(TEXT(\"SimplyStream\"))");
	}
}
