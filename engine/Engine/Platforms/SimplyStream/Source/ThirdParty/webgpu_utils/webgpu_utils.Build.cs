// Copyright Wonder Interactive Inc, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class webgpu_utils : ModuleRules
{
	public webgpu_utils(ReadOnlyTargetRules Target) : base(Target)
	{
		//Type = ModuleType.External;
		PublicSystemIncludePaths.Add(ModuleDirectory);

		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("AtomicQueue");
		// wasm_utils.h is canonical in the wasm_utils module. We used to ship a
		// drifted copy at this module's root; we pick up the canonical header via
		// include-paths only (no link-time edge), because the file is header-only
		// and a full module dep re-creates the Core → … → wasm_utils → Core cycle
		// that UBT 5.7 rejects.
		PublicIncludePathModuleNames.Add("wasm_utils");
	}
}
