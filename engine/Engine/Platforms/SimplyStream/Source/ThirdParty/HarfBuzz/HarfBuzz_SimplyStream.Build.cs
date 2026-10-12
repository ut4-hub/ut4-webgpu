// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class HarfBuzz_SimplyStream : HarfBuzz
{
	protected override string HarfBuzzVersion => "harfbuzz-2.4.0";
	protected override string LibRootDirectory => PlatformModuleDirectory;
	protected override string LibHarfBuzzRootPath => Path.Combine(LibRootDirectory, HarfBuzzVersion, "lib");

	public HarfBuzz_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		// Can't be used without our dependencies
		if (!Target.bCompileFreeType || !Target.bCompileICU)
		{
			return;
		}

		// base added 0 for unknown platform
		PublicDefinitions.Remove("WITH_HARFBUZZ=0");
		PublicDefinitions.Add("WITH_HARFBUZZ=1");

		string LibCandidate = Target.Architecture == UnrealArch.wasm32
			? Path.Combine(LibHarfBuzzRootPath, "wasm32", "libharfbuzz.a")
			: Path.Combine(LibHarfBuzzRootPath, "libharfbuzz64b.a");
		if (!File.Exists(LibCandidate))
		{
			throw new BuildException("SimplyStream HarfBuzz archive is missing for architecture {0}: {1}", Target.Architecture, LibCandidate);
		}
		System.Console.WriteLine("[HarfBuzz_SimplyStream] selected archive = " + LibCandidate);
		PublicAdditionalLibraries.Add(LibCandidate);
	}
}
