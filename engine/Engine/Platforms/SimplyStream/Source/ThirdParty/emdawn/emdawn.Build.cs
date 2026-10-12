// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class emdawn : ModuleRules
{
	public emdawn(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		string ModuleDir = GetModuleDirectoryForSubClass(GetType()).FullName;
		//PublicAdditionalLibraries.Add(Path.Combine(ModuleDir, "Private/libemdawnwebgpu_c.a"));
		PublicAdditionalLibraries.Add(Path.Combine(ModuleDir, "Private/library_webgpu_enum_tables.js"));
		PublicAdditionalLibraries.Add(Path.Combine(ModuleDir, "Private/library_webgpu_generated_sig_info.js"));
		PublicAdditionalLibraries.Add(Path.Combine(ModuleDir, "Private/library_webgpu_generated_struct_info.js"));
		//PublicAdditionalLibraries.Add(Path.Combine(ModuleDir, "Private/library_html5_webgpu.js"));
		PublicAdditionalLibraries.Add(Path.Combine(ModuleDir, "Private/library_webgpu.js"));
	}
}
