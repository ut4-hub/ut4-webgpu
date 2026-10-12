// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class BLAKE3_SimplyStream : BLAKE3
{
	public BLAKE3_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		string ModulePath = GetModuleDirectoryForSubClass(GetType()).FullName;
		string LibCandidate = Target.Architecture == UnrealArch.wasm32
			? Path.Combine(ModulePath, Version, "lib", "wasm32", "libBLAKE3.a")
			: Path.Combine(ModulePath, Version, "lib", "Release", "libBLAKE3.a");
		if (!File.Exists(LibCandidate))
		{
			throw new BuildException("SimplyStream BLAKE3 archive is missing for architecture {0}: {1}", Target.Architecture, LibCandidate);
		}
		System.Console.WriteLine("[BLAKE3_SimplyStream] selected archive = " + LibCandidate);
		PublicAdditionalLibraries.Add(LibCandidate);
	}
}
