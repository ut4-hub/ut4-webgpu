// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class OodleNetworkHandlerComponent_SimplyStream : OodleNetworkHandlerComponent
{
	string LibExt { get { return ".a"; } }
	protected override string ReleaseLibraryName { get { return "liboodlenetwork" + LibExt; } }
	protected override string DebugLibraryName { get { return "liboodlenetwork" + LibExt; } }

	public OodleNetworkHandlerComponent_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicAdditionalLibraries.RemoveAll(Library => Library.EndsWith(ReleaseLibraryName) && !File.Exists(Library));
	}
}
