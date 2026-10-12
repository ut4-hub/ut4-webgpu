// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class VorbisFile_SimplyStream : VorbisFile
{
	protected override string LibRootDirectory { get { return Target.SimplyStreamPlatform.PlatformThirdPartySourceDirectory; } }
	protected override string VorbisFileLibPath { get { return Path.GetDirectoryName(base.VorbisFileLibPath); } } // strip off /lib and add h5LibPath

	public VorbisFile_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		string LibCandidate = Target.Architecture == UnrealArch.wasm32
			? Path.Combine(VorbisFileLibPath, "lib", "wasm32", "libvorbisfile.a")
			: Path.Combine(VorbisFileLibPath, "libvorbisfile.a");
		if (!File.Exists(LibCandidate))
		{
			throw new BuildException("SimplyStream VorbisFile archive is missing for architecture {0}: {1}", Target.Architecture, LibCandidate);
		}
		System.Console.WriteLine("[VorbisFile_SimplyStream] selected archive = " + LibCandidate);
		PublicAdditionalLibraries.Add(LibCandidate);
	}
}
