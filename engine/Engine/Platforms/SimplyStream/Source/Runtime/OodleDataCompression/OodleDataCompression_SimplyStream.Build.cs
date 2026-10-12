// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class OodleDataCompression_SimplyStream : OodleDataCompression
{
	protected override string OodleVersion { get { return "2.9.14"; } }
	protected override string LibRootDirectory { get { return GetModuleDirectoryForSubClass(GetType()).FullName; } }
	protected override string ReleaseLibraryName { get { return Target.Architecture == UnrealArch.wasm32 ? Path.Combine("wasm32", "liboodle.a") : "liboodle_O2b.a"; } }
	protected override string DebugLibraryName { get { return ReleaseLibraryName; } }

	public OodleDataCompression_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		string LibCandidate = Path.Combine(LibRootDirectory, "Sdks", OodleVersion, "lib", ReleaseLibraryName);
		if (!File.Exists(LibCandidate))
		{
			throw new BuildException("SimplyStream Oodle archive is missing for architecture {0}: {1}", Target.Architecture, LibCandidate);
		}
		System.Console.WriteLine("[OodleDataCompression_SimplyStream] selected archive = " + LibCandidate);
	}
}
