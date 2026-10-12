// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ICU_SimplyStream : ICU
{
	protected override string ICUVersion =>  ICU64VersionString;
	protected override string ICULibRootPath => GetModuleDirectoryForSubClass(GetType()).FullName;

	public ICU_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		string LibCandidate = Target.Architecture == UnrealArch.wasm32
			? Path.Combine(ICULibPath, "wasm32", "libicu.a")
			: Path.Combine(ICULibPath, "libicu64b.a");
		if (!File.Exists(LibCandidate))
		{
			throw new BuildException("SimplyStream ICU archive is missing for architecture {0}: {1}", Target.Architecture, LibCandidate);
		}
		System.Console.WriteLine("[ICU_SimplyStream] ICULibPath = " + ICULibPath);
		System.Console.WriteLine("[ICU_SimplyStream] selected archive = " + LibCandidate);
		PublicAdditionalLibraries.Add(LibCandidate);
	}
}
