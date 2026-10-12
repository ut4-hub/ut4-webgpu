// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LibWebM_SimplyStream : LibWebM
{
	public LibWebM_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		//string webmPath = Path.Combine(Target.SimplyStreamPlatform.PlatformThirdPartySourceDirectory, "webm");

		//PublicIncludePaths.Add(webmPath  + "/include");
		//PublicAdditionalLibraries.Add(Path.Combine(webmPath, "webm" , Target.SimplyStreamPlatform.h5LibPath, "webm" + Target.SimplyStreamOptimizationSuffix + Target.SimplyStreamPlatform.libExt));
		//PublicAdditionalLibraries.Add(Path.Combine(webmPath, "libvpx", Target.SimplyStreamPlatform.h5LibPath, "libvpx" + Target.SimplyStreamOptimizationSuffix + Target.SimplyStreamPlatform.libExt));
	}
}
