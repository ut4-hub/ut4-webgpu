// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class FreeType2_SimplyStream : FreeType2
{
	protected override string FreeType2Version { get { return "FreeType2-2.10.0"; } }

	public FreeType2_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		string LibCandidate = Target.Architecture == UnrealArch.wasm32
			? Path.Combine(FreeType2LibPath, "wasm32", "libfreetype.a")
			: Path.Combine(FreeType2LibPath, "libfreetype64.a");
		if (!File.Exists(LibCandidate))
		{
			throw new BuildException("SimplyStream FreeType archive is missing for architecture {0}: {1}", Target.Architecture, LibCandidate);
		}
		System.Console.WriteLine("[FreeType2_SimplyStream] selected archive = " + LibCandidate);
		PublicAdditionalLibraries.Add(LibCandidate);
	}
}
