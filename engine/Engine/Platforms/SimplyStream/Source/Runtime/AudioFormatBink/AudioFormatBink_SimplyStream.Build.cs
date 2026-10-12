// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AudioFormatBink_SimplyStream : AudioFormatBink
{
	public AudioFormatBink_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "..", "..", "Runtime", "BinkAudioDecoder", "SDK", "BinkAudio", "Lib", "libbink.a"));


		// base set this to None
		PrecompileForTargets = PrecompileTargetsType.Default;
	}
}
