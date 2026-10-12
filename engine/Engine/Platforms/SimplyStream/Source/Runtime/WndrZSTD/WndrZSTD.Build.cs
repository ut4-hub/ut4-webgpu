using UnrealBuildTool;
using System.IO;
using System;

public class WndrZSTD : ModuleRules
{
	public WndrZSTD(ReadOnlyTargetRules Target) : base(Target)
	{
		//PrivateIncludePathModuleNames.Add("Core");
		//Type = ModuleType.External;
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/RCS"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

		PublicDependencyModuleNames.Add("OpenSSL");
		PublicDependencyModuleNames.Add("AtomicQueue");
		// wasm_utils owns the canonical wasm_utils.h (and friends). The
		// previous duplicate copy at Public/wasm_utils.h was deleted; we pick
		// up the headers via include-paths only (no link-time edge), because
		// the file is header-only (inline `wndr_get_now`, KEEP_IN_MODULE etc.)
		// and a full PublicDependencyModuleNames edge re-creates the
		// Core → WndrZSTD → wasm_utils → Core cycle that UBT 5.7 rejects.
		PublicIncludePathModuleNames.Add("wasm_utils");
		//PublicDependencyModuleNames.Add("webgpu_utils");
		if (true)
		{
			PublicDefinitions.Add("ASIO_STANDALONE");
			PublicDefinitions.Add("ASIO_NO_EXCEPTIONS");
			PublicDefinitions.Add("ASIO_NO_TYPEID");
			PublicDefinitions.Add("ASIO_HAS_STD_ARRAY");
			PublicDefinitions.Add("ASIO_HAS_STD_ATOMIC");
			PublicDefinitions.Add("ASIO_HAS_STD_SYSTEM_ERROR");
			PublicDefinitions.Add("ASIO_HAS_STD_TYPE_TRAITS");
			PublicDefinitions.Add("CAPTURE_ENABLED=0");
		}
		if (Target.Configuration == UnrealTargetConfiguration.Debug)
		{
			//OptimizeCode = CodeOptimization.Always;
		}
		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicDefinitions.Add("ZDICT_DISABLE_DEPRECATE_WARNINGS");
			PublicDefinitions.Add(string.Format("__STDC_WANT_SECURE_LIB__={0}", 1));
			PrivatePCHHeaderFile = "Private/ZSTDPrivatePCH.h";
			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"Core",
				"TraceLog",
				"CoreUObject",
			}
			);
		} else
        {
			PublicIncludePathModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"Core",
				"TraceLog",
				"CoreUObject",
			}
			);
		}
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PublicSystemLibraries.Add("synchronization.lib");
		}
		//bRequiresImplementModule = false;
		PCHUsage = PCHUsageMode.NoPCHs;
		CppStandard = (Target.Platform == UnrealTargetPlatform.Mac) ? CppStandardVersion.Cpp20 : CppStandardVersion.Latest;
	}

}
