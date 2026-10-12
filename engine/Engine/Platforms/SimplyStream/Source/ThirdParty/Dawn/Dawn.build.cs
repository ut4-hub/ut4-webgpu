using UnrealBuildTool;
using System.IO;
public class Dawn : ModuleRules
{
	protected virtual string DawnRootPath {
		get { return ModuleDirectory; }
	}

	protected virtual string DawnIncRootPath {
		get { return Path.Combine(DawnRootPath, "include"); }
	}

	protected virtual string DawnLibRootPath {
		get { return Path.Combine(DawnRootPath, "lib"); }
	}

	public Dawn(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		CppStandard = (Target.Platform == UnrealTargetPlatform.Mac) ? CppStandardVersion.Cpp20 : CppStandardVersion.Latest;
		string platform = Target.Platform.ToString();

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			platform = "windows";
			PublicSystemIncludePaths.Add(DawnIncRootPath);
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Tint");
			PublicAdditionalLibraries.Add(Path.Combine(DawnLibRootPath, platform, "webgpu_dawn.lib"));
			RuntimeDependencies.Add(Path.Combine(DawnLibRootPath, platform, "webgpu_dawn.dll"));
			PublicDelayLoadDLLs.Add("webgpu_dawn.dll");
			PublicDefinitions.Add("WGPU_SHARED_LIBRARY=1");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			platform = "mac";
			PublicSystemIncludePaths.Add(DawnIncRootPath);
			//AddEngineThirdPartyPrivateStaticDependencies(Target, "MTLPP");
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"MetalCPP"
			);
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"MetalShaderConverter"
			);
            PublicFrameworks.AddRange(new string[] { "QuartzCore", "IOSurface" });
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Tint");
			PublicWeakFrameworks.Add("Metal");
			PublicAdditionalLibraries.Add(Path.Combine(DawnLibRootPath, platform, "libdawn.a"));
			PublicAdditionalLibraries.Add(Path.Combine(DawnLibRootPath, platform, "libabsl.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			platform = "linux";
			PublicSystemIncludePaths.Add(DawnIncRootPath);
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Tint");
			PublicAdditionalLibraries.Add(Path.Combine(DawnLibRootPath, platform, "libdawn.a"));
		}
	}
}
