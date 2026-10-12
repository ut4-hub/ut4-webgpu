// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Vorbis_SimplyStream : Vorbis
{
	protected override string LibRootDirectory { get { return Target.SimplyStreamPlatform.PlatformThirdPartySourceDirectory; } }
	protected override string VorbisLibPath { get { return Path.GetDirectoryName(base.VorbisLibPath); } } // strip off /lib and add h5LibPath

	public Vorbis_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		string LibCandidate = Target.Architecture == UnrealArch.wasm32
			? Path.Combine(VorbisLibPath, "lib", "wasm32", "libvorbis.a")
			: Path.Combine(VorbisLibPath, "libvorbis.a");
		if (!File.Exists(LibCandidate))
		{
			throw new BuildException("SimplyStream Vorbis archive is missing for architecture {0}: {1}", Target.Architecture, LibCandidate);
		}
		System.Console.WriteLine("[Vorbis_SimplyStream] selected archive = " + LibCandidate);
		PublicAdditionalLibraries.Add(LibCandidate);
	}
}
