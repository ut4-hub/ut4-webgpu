// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class zlib_SimplyStream : zlib
{
	public zlib_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		//string zlibPath = Path.Combine(Target.SimplyStreamPlatform.PlatformThirdPartySourceDirectory, "zlib", Version);

		// v1.2.8
		//PublicIncludePaths.Add(zlibPath + "/include");
		//PublicAdditionalLibraries.Add(Path.Combine(zlibPath, "zlib.a"));
	}
}
