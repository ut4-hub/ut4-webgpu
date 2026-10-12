// Copyright Epic Games, Inc. All Rights Reserved.
//
// Registers WasmAudioMixerLibrary.js as an emcc --js-library (auto-detected by
// the SimplyStream toolchain via .js extension in PublicAdditionalLibraries).

using UnrealBuildTool;
using System.IO;

public class SimplyStreamAudioMixer : ModuleRules
{
	public SimplyStreamAudioMixer(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/AudioMixer/Public");
		PrivateIncludePaths.Add("Runtime/AudioMixer/Private");

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core",
			"AudioMixerCore",
		});

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}

		// Register the AudioWorklet bridge with emcc (--js-library). The
		// SimplyStream toolchain auto-detects .js entries in PublicAdditionalLibraries
		// and forwards them with the right flag (see SimplyStreamToolChain.cs:821).
		PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "Private", "WasmAudioMixerLibrary.js"));
	}
}
