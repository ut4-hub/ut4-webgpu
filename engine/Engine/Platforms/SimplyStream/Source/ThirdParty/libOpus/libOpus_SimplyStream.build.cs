// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class libOpus_SimplyStream : libOpus
{

	public libOpus_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		string LibCandidate = Target.Architecture == UnrealArch.wasm32
			? Path.Combine(LibRootDirectory, OpusVersion, "lib", "wasm32", "libopus.a")
			: Path.Combine(LibRootDirectory, OpusVersion, "lib", "libopus.a");
		if (!File.Exists(LibCandidate))
		{
			throw new BuildException("SimplyStream Opus archive is missing for architecture {0}: {1}", Target.Architecture, LibCandidate);
		}
		System.Console.WriteLine("[libOpus_SimplyStream] selected archive = " + LibCandidate);
		PublicAdditionalLibraries.Add(LibCandidate);
		//PublicAdditionalLibraries.Add(Path.Combine(LibRootDirectory, OpusVersion, "lib", "libspeex_resampler.o"));
	}
}
