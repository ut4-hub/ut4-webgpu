// Copyright Wonder Interactive Inc, Inc. All Rights Reserved.

using UnrealBuildTool;

public class wgsl_pack : ModuleRules
{
	public wgsl_pack(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = (Target.Platform == UnrealTargetPlatform.Mac) ? CppStandardVersion.Cpp20 : CppStandardVersion.Latest;
		PublicSystemIncludePaths.Add(ModuleDirectory);
		bWarningsAsErrors = false;

		// wndr: single-file module. Adaptive unity excludes a recently-edited .cpp from the
		// unity blob and compiles it standalone — but for a 1-file module that leaves the unity
		// object empty and UBT fails to link the standalone object, so every pack()/unpack()/
		// wgsl_pack_init() symbol goes undefined the moment wgsl_pack.cpp is touched. Unity buys
		// nothing for one file, so disable it here for deterministic linkage.
		bUseUnity = false;

		PrivateDependencyModuleNames.Add("Core");

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			ForceIncludeFiles.Add(System.IO.Path.Combine(EngineDirectory, "Platforms/SimplyStream/Source/MacEnumFix.h"));
		}
	}
}
