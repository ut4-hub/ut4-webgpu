using UnrealBuildTool;
using System.IO;
public class Tint : ModuleRules
{
	protected virtual string TintRootPath {
		get { return ModuleDirectory; }
	}

	protected virtual string TintIncRootPath {
		get { return Path.Combine(TintRootPath, "include"); }
	}

	protected virtual string TintLibRootPath {
		get { return Path.Combine(TintRootPath, "lib"); }
	}

	protected void AddPublicSystemIncludePathIfExists(string IncludePath)
	{
		if (Directory.Exists(IncludePath))
		{
			PublicSystemIncludePaths.Add(IncludePath);
		}
	}

	public Tint(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		CppStandard = (Target.Platform == UnrealTargetPlatform.Mac) ? CppStandardVersion.Cpp20 : CppStandardVersion.Latest;
		string platform = Target.Platform.ToString();

		AddPublicSystemIncludePathIfExists(TintIncRootPath);
		AddPublicSystemIncludePathIfExists(Path.Combine(TintIncRootPath, "src"));
		AddPublicSystemIncludePathIfExists(Path.Combine(TintIncRootPath, "src", "tint"));
		AddPublicSystemIncludePathIfExists(Path.Combine(TintIncRootPath, "tint"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			platform = "windows";
			RuntimeDependencies.Add(Path.Combine(TintLibRootPath, platform, "tintw.dll"));
			PublicDelayLoadDLLs.Add("tintw.dll");
			PublicAdditionalLibraries.Add(Path.Combine(TintLibRootPath, platform, "tintw.lib"));
			// tintw.dll only exports the tintw_* C API; the SPIR-V optimizer pass the cooker runs before tintw
			// (WebGPUShaderCompiler.cpp) needs spvtools::Optimizer, so link the static SPIRV-Tools archives here
			// (the Mac/Linux branches link libSPIRV-Tools.a for the same reason). -opt must precede the base lib.
			PublicAdditionalLibraries.Add(Path.Combine(TintLibRootPath, platform, "SPIRV-Tools-opt.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(TintLibRootPath, platform, "SPIRV-Tools.lib"));
			PublicDefinitions.Add("WGPU_SHARED_LIBRARY=1");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			platform = "mac";
			PublicAdditionalLibraries.Add(Path.Combine(TintLibRootPath, platform, "libtint.a"));
			PublicAdditionalLibraries.Add(Path.Combine(TintLibRootPath, platform, "libSPIRV-Tools.a"));
			PublicAdditionalLibraries.Add(Path.Combine(TintLibRootPath, platform, "libtintw.dylib"));
			RuntimeDependencies.Add(Path.Combine(TintLibRootPath, platform, "libtintw.dylib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			platform = "linux";
			PublicAdditionalLibraries.Add(Path.Combine(TintLibRootPath, platform, "libtint.a"));
			PublicAdditionalLibraries.Add(Path.Combine(TintLibRootPath, platform, "libSPIRV-Tools.a"));
		}
	}
}
