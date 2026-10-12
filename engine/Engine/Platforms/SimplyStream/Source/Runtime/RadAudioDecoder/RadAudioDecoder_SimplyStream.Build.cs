// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class RadAudioDecoder_SimplyStream : RadAudioDecoder
{
    protected override string[] GetRadAudioPlatformString(ReadOnlyTargetRules Target, out bool bIsWindowsLib)
    {
        bIsWindowsLib = false;
		return new string[] { Target.Architecture == UnrealArch.wasm32 ? "wasm32" : "wasm64" };
    }

	public RadAudioDecoder_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		string Architecture = Target.Architecture == UnrealArch.wasm32 ? "wasm32" : "wasm64";
		string LibCandidate = Path.Combine(GetModuleDirectoryForSubClass(GetType()).FullName, "..", "SDK", "Lib", "libradaudio_decoder_" + Architecture + ".a");
		if (!File.Exists(LibCandidate))
		{
			throw new BuildException("SimplyStream RAD Audio archive is missing for architecture {0}: {1}", Target.Architecture, LibCandidate);
		}
		System.Console.WriteLine("[RadAudioDecoder_SimplyStream] selected archive = " + LibCandidate);
	}
}
