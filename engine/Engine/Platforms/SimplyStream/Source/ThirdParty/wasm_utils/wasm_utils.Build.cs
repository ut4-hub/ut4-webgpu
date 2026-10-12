// Copyright Wonder Interactive Inc, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class wasm_utils : ModuleRules
{
	public wasm_utils(ReadOnlyTargetRules Target) : base(Target)
	{
		//Type = ModuleType.External;
		PublicSystemIncludePaths.Add(ModuleDirectory);

		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("AtomicQueue");
	}
}
