
using System.IO;
using UnrealBuildTool;

public class WebGPURHI : ModuleRules
{
	public WebGPURHI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
		IWYUSupport = IWYUSupport.None;
		// wndr 5.8: the native (Mac/Win/Linux) build enforces -Werror,-Wunreachable-code[-break]; the WebGPU switch
		// handlers have benign defensive `break;` after `return`. The wasm toolchain doesn't error on these. Keep
		// them visible but non-fatal on the host rather than churning ~40 harmless breaks.
		CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Warning;
		CppStandard = (Target.Platform == UnrealTargetPlatform.Mac) ? CppStandardVersion.Cpp20 : CppStandardVersion.Latest;

		//PublicDefinitions.Add($"PLATFORM_WASM={(Target.Platform == UnrealTargetPlatform.SimplyStream ? 1 : 0)}");

		PrecompileForTargets = PrecompileTargetsType.Any;
		//bPrecompile = true;
		//bUsePrecompiled = true;

		PrivateDependencyModuleNames.Add("wgsl_pack");
		if (Target.Platform != UnrealTargetPlatform.SimplyStream)
		{
			// Windows Platform
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "Dawn");
				PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Windows"));
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "Dawn");
				PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Linux"));
				PrivateDependencyModuleNames.Add("SDL3"); // UE 5.8 ships SDL3 (SDL2 module no longer exists)
				PublicSystemIncludePaths.Add("/usr/include/X11");
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "Dawn");
				PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mac"));
			}

			// Module dependencies
			PrivateDependencyModuleNames.Add("Slate");
			PrivateDependencyModuleNames.Add("SlateCore");
			PrivateDependencyModuleNames.Add("Core");
			PrivateDependencyModuleNames.Add("CoreUObject");
			PrivateDependencyModuleNames.Add("RHI");
			PrivateDependencyModuleNames.Add("RHICore");
			PrivateDependencyModuleNames.Add("RenderCore");
			PrivateDependencyModuleNames.Add("ApplicationCore");
			PrivateDependencyModuleNames.Add("Dawn");
			PrivateDependencyModuleNames.Add("AtomicQueue");
			PrivateDependencyModuleNames.Add("xxhash");
			PrivateDependencyModuleNames.Add("webgpu_utils");
			//PrivateDependencyModuleNames.Add("method_proxy");
		}
		else
		{
			PrivateDependencyModuleNames.Add("Core");
			PrivateDependencyModuleNames.Add("RHI");
			PrivateDependencyModuleNames.Add("RHICore");
			PrivateDependencyModuleNames.Add("RenderCore");
			PrivateDependencyModuleNames.Add("TraceLog");
			PrivateDependencyModuleNames.Add("AtomicQueue");
			PrivateDependencyModuleNames.Add("xxhash");
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/SimplyStream"));
			PrivateDependencyModuleNames.Add("emdawn");
			PrivateDependencyModuleNames.Add("webgpu_utils");
			//PrivateDependencyModuleNames.Add("method_proxy");
		}
	}
}
