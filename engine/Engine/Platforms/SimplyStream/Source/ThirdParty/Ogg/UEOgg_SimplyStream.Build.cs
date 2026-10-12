// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UEOgg_SimplyStream : UEOgg
{
	protected override string LibRootDirectory { get { return Target.SimplyStreamPlatform.PlatformThirdPartySourceDirectory; } }
	protected override string OggLibPath { get { return Path.GetDirectoryName(base.OggLibPath); } }

	public UEOgg_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		string LibCandidate = Target.Architecture == UnrealArch.wasm32
			? Path.Combine(OggLibPath, "lib", "wasm32", "libogg.a")
			: Path.Combine(OggLibPath, "libogg.a");
		if (!File.Exists(LibCandidate))
		{
			throw new BuildException("SimplyStream Ogg archive is missing for architecture {0}: {1}", Target.Architecture, LibCandidate);
		}
		System.Console.WriteLine("[UEOgg_SimplyStream] selected archive = " + LibCandidate);
		PublicAdditionalLibraries.Add(LibCandidate);
	}
}
