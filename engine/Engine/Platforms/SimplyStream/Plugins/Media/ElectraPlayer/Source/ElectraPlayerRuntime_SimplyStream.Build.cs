// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class ElectraPlayerRuntime_SimplyStream : ElectraPlayerRuntime
	{
		public ElectraPlayerRuntime_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
		{
			// ElectraPlayerRuntime consumes ElectraSamples' native-only texture
			// interface. It is unsupported on wasm and must not enter the
			// all-modules precompile closure.
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
