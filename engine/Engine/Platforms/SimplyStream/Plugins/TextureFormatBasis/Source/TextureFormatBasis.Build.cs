// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class TextureFormatBasis : ModuleRules
{
	protected string DynamicLibNamePrefix;
	protected string DynamicLibNameSuffix;

	protected void SetDynamicLibNameStrings()
	{
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			DynamicLibNamePrefix = "basisu_";
			DynamicLibNameSuffix = ".dll";
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			DynamicLibNamePrefix = "libbasisu.";
			DynamicLibNameSuffix = ".dylib";
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			DynamicLibNamePrefix = "libbasisu.";
			DynamicLibNameSuffix = ".so";
		}
		else if (Target.Platform == UnrealTargetPlatform.LinuxArm64)
		{
			DynamicLibNamePrefix = "libbasisu.";
			DynamicLibNameSuffix = ".so";
		}
		else
		{
			throw new BuildException("Platform {0} not supported in TextureFormatBasis.", Target.Platform);
		}

		PublicDefinitions.Add("TFB_DLL_PREFIX=\"" + DynamicLibNamePrefix + "\"");
		PublicDefinitions.Add("TFB_DLL_SUFFIX=\"" + DynamicLibNameSuffix + "\"");
	}

	protected void AddDynamicLibsForVersion(string Version)
	{
		string PlatformDir = Target.Platform.ToString();

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			PlatformDir = "Win64";
		}

		string DynamicLibName = DynamicLibNamePrefix + Version + DynamicLibNameSuffix;
		string SdkBase = Path.Combine(ModuleDirectory, "..", "Sdks", Version);
		string FullDynamicLibName = Path.Combine(SdkBase, "redist", PlatformDir, DynamicLibName);

		if (!File.Exists(FullDynamicLibName))
		{
			throw new BuildException("Platform {0} can't find dynamic lib for  TextureFormatBasis {1}.", Target.Platform, FullDynamicLibName);
		}

		FullDynamicLibName = "$(EngineDir)/" + UnrealBuildTool.Utils.MakePathRelativeTo(FullDynamicLibName, EngineDirectory);

		RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", DynamicLibName), FullDynamicLibName, StagedFileType.NonUFS);
	}

	protected void AddPrivateIncludePathIfExists(string IncludePath)
	{
		if (Directory.Exists(IncludePath))
		{
			PrivateIncludePaths.Add(IncludePath);
		}
	}

	public TextureFormatBasis(ReadOnlyTargetRules Target) : base(Target)
	{
		IWYUSupport = IWYUSupport.None;
		CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
		// wndr 5.8: vendored basis_universal (transcoder/jpgd) has many unreachable `break;` after `return` that
		// the native build flags as -Werror,-Wunreachable-code-break. Don't patch vendored upstream for a pedantic
		// style warning.
		CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Off;

		// wndr 5.8: basis_universal .cpp files use file-scoped `using namespace basist;`. Under a unity build that
		// leaks across the concatenated TUs into basisu_comp.cpp/headers, where 5.8's newer engine `basist` now
		// also defines hdr_modes / astc_6x6_hdr -> ambiguous lookups. The encoder is written to be compiled
		// per-file, so turn off unity for this module (proper fix, not a suppression).
		bUseUnity = false;

		ShortName = "TextureFormatBasis";

		PrivateDefinitions.Add("BASISU_NO_ITERATOR_DEBUG_LEVEL=1");

		PrivateIncludePathModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"TargetPlatform",
			"TextureCompressor",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"DerivedDataCache", // for FCacheBucket
			"ImageCore",
			"TextureBuild",
		});

		PublicDefinitions.Add(string.Format("BASISU_HASHMAP_TEST={0}", 0));
		PublicDefinitions.Add(string.Format("BASISU_VALIDATE_UASTC_ENC={0}", 0));
		PublicDefinitions.Add(string.Format("BASISU_USE_ASTC_DECOMPRESS={0}", 0));
		PublicDefinitions.Add(string.Format("BASISU_SUPPORT_SSE={0}", Target.Architecture == UnrealArch.X64 ? 1 : 0));
		PublicDefinitions.Add(string.Format("BASISU_SUPPORT_OPENCL={0}", 0));
		PublicDefinitions.Add(string.Format("BASISD_SUPPORT_KTX2_ZSTD={0}", 0));

		if (Target.bBuildEditor == true) // Cook-only module
		{
			SetDynamicLibNameStrings();

			// dynamic libs , for every version, not just current :
			//AddDynamicLibsForVersion("1.60"); // not currently needed, as we compile the source

			AddPrivateIncludePathIfExists(Path.Combine(ModuleDirectory, "Private"));
			AddPrivateIncludePathIfExists(Path.Combine(ModuleDirectory, "Private", "encoder"));
			AddPrivateIncludePathIfExists(Path.Combine(ModuleDirectory, "Private", "transcoder"));

			PublicDefinitions.Add("WITH_BASIS_COMPRESSOR=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_BASIS_COMPRESSOR=0");
		}
	}
}
