// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Launch_SimplyStream : Launch
{
	public Launch_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] {
			"DawnRHI",
			// SimplyStream's own Web Audio + AudioWorklet mixer (no SDL/OpenAL dep).
			// Pairs with the AudioMixerModuleName setting in SimplyStreamEngine.ini.
			"SimplyStreamAudioMixer",
			"Analytics",
			"AnalyticsET",
			"wasm_utils",
			"PakFile"
		});
		PrivateIncludePaths.Add("Runtime/PakFile/Private");

		// PRECOMPILED-OBJECT INJECTION (stock-emsdk bring-up): link SimplyStream's shipped
		// no-source platform module objects so their symbols resolve at link. Glob every .o in
		// each module dir (filenames vary: Module.X.cpp.o vs X.cpp.o).
		string PreDir = "/mnt/vms/ss-build/UnrealEngine/Engine/Platforms/SimplyStream/Intermediate/Build/SimplyStream/wasm/UnrealGame/Development";
		foreach (string M in new string[] { "wasm_utils","SimplyStreamJS","SimplyStreamAudioMixer","SimplyStreamPlatformFeatures","WebGPURHI","emdawn","webgpu_utils","wgsl_pack","WndrZSTD" })
		{
			string Dir = System.IO.Path.Combine(PreDir, M);
			if (System.IO.Directory.Exists(Dir))
			{
				foreach (string Obj in System.IO.Directory.GetFiles(Dir, "*.o"))
				{
					PublicAdditionalLibraries.Add(Obj);
				}
			}
		}

		//PublicSystemIncludePaths.Add("A:/dev/UE/Engine/Source/ThirdParty/ICU/icu4c-64_1/include");

		//AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL3");
	}
}
