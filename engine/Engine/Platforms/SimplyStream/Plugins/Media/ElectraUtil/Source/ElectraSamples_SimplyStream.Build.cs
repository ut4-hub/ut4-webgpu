// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class ElectraSamples_SimplyStream : ElectraSamples
	{
		public ElectraSamples_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
		{
			// Electra's public texture-sample API exists only for its supported
			// native platforms. Keep this module out of the wasm all-modules
			// precompile closure; projects should deny ElectraPlayer on SimplyStream.
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
