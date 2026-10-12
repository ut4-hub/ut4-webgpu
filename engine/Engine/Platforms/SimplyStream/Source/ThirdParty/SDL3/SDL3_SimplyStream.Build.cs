// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class SDL3_SimplyStream : SDL3
{
	protected override string LibRootDirectory { get { return Target.SimplyStreamPlatform.PlatformThirdPartySourceDirectory; } }

	public SDL3_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicAdditionalLibraries.Add(Path.Combine(Path.GetDirectoryName(base.SDL3LibPath), "libSDL2.a"));
	}
}
