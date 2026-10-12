// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UElibPNG_SimplyStream : UElibPNG
{
	protected override string LibPNGVersion { get { return "libPNG-1.6.44"; } }
	protected override string LibRootDirectory { get { return Target.SimplyStreamPlatform.PlatformThirdPartySourceDirectory; } }
	protected override string LibPNGPath { get { return Path.GetDirectoryName(base.LibPNGPath); } } // strip off /lib and add h5LibPath

	public UElibPNG_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		string LibCandidate = Target.Architecture == UnrealArch.wasm32
			? Path.Combine(LibPNGPath, "lib", "wasm32", "libpng.a")
			: Path.Combine(LibPNGPath, "libpng64.a");
		if (!File.Exists(LibCandidate))
		{
			throw new BuildException("SimplyStream libPNG archive is missing for architecture {0}: {1}", Target.Architecture, LibCandidate);
		}
		System.Console.WriteLine("[UElibPNG_SimplyStream] selected archive = " + LibCandidate);
		PublicAdditionalLibraries.Add(LibCandidate);
	}
}
