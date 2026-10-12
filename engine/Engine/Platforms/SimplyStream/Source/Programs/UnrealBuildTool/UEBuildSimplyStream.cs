// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	partial struct UnrealArch
	{
		/// <summary>
		/// wasm32 target architecture
		/// </summary>
		public static UnrealArch wasm = FindOrAddByName("wasm", bIsX64: false);

		/// <summary>
		/// Native wasm32 target architecture. Kept distinct from the legacy "wasm"
		/// artifact identity used by MEMORY64=2.
		/// </summary>
		public static UnrealArch wasm32 = FindOrAddByName("wasm32", bIsX64: false);

		/// <summary>
		/// wasm64 target architecture
		/// </summary>
		public static UnrealArch wasm64 = FindOrAddByName("wasm64", bIsX64: true);


		private static Dictionary<UnrealArch, string> EmscriptenToolchainArchitectures = new()
		{
			{ UnrealArch.wasm,           "wasm" },
			{ UnrealArch.wasm32,         "wasm32" },
			{ UnrealArch.wasm64,         "wasm64" },
		};

		/// <summary>
		/// Windows-specific low level name for the generic platforms
		/// </summary>
		public string EmscriptenName
		{
			get
			{
				if (EmscriptenToolchainArchitectures.ContainsKey(this)) return EmscriptenToolchainArchitectures[this];
				throw new BuildException($"Unknown architecture {ToString()} passed to UnrealArch.EmscriptenName");
			}
		}

	}
	partial struct UnrealTargetPlatform
	{
		/// <summary>
		/// SimplyStream target platform
		/// </summary>
		public static UnrealTargetPlatform SimplyStream = FindOrAddByName("SimplyStream");
	}

	/// <summary>
	/// SimplyStream-specific target settings
	/// </summary>
	public class SimplyStreamTargetRules
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public SimplyStreamTargetRules()
		{
			XmlConfig.ApplyTo(this);
		}

		/// <summary>
		/// SimplyStream library extension
		/// </summary>
		public string libExt { get { return SimplyStreamToolChain.libExt; } }

		/// <summary>
		/// SimplyStream object file extension
		/// </summary>
		public string objExt { get { return SimplyStreamToolChain.objExt; } }

		/// <summary>
		/// Enable SIMD support
		/// </summary>
		public bool enableSIMD { get { return SimplyStreamToolChain.enableSIMD; } }

		/// <summary>
		/// SimplyStream thirdparty lib path
		/// </summary>
		public string h5LibPath { get { return SimplyStreamToolChain.h5LibPath; } }
	}

	/// <summary>
	/// Read-only wrapper for SimplyStream-specific target settings
	/// </summary>
	public class ReadOnlySimplyStreamTargetRules
	{
		/// <summary>
		/// The private mutable settings object
		/// </summary>
		private SimplyStreamTargetRules Inner;

		/// <summary>
		/// The path to the DPX specific ThirdParty source directory.
		/// </summary>
		public string PlatformThirdPartySourceDirectory => DirectoryReference.Combine(Unreal.EngineDirectory, "Platforms", "SimplyStream", "Source", "ThirdParty").FullName;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlySimplyStreamTargetRules(SimplyStreamTargetRules Inner)
		{
			this.Inner = Inner;
		}

		/// <summary>
		/// Accessors for fields on the inner TargetRules instance
		/// </summary>

		#region Read-only accessor properties
#if !__MonoCS__
#pragma warning disable CS1591
#endif

		public string libExt
		{
			get { return Inner.libExt; }
		}

		public string objExt
		{
			get { return Inner.objExt; }
		}

		public bool enableSIMD
		{
			get { return Inner.enableSIMD; }
		}

		public string h5LibPath
		{
			get { return Inner.h5LibPath; }
		}

#if !__MonoCS__
#pragma warning restore CS1591
#endif
		#endregion
	}


	partial class TargetRules
	{
		/// <summary>
		/// DPX-specific target settings.
		/// </summary>
		internal SimplyStreamTargetRules SimplyStreamPlatform = new();
	}

	partial class ReadOnlyTargetRules
	{
		private ReadOnlySimplyStreamTargetRules? _SimplyStreamPlatform = null;

		/// <summary>
		/// SimplyStream-specific target settings
		/// </summary>
		public ReadOnlySimplyStreamTargetRules SimplyStreamPlatform
		{
			get
			{
				_SimplyStreamPlatform ??= new ReadOnlySimplyStreamTargetRules(Inner.SimplyStreamPlatform);
				return _SimplyStreamPlatform;
			}
		}

		/// <summary>
		/// Return an optimization suffix for all standard SimplyStream libs based on Target settings
		/// </summary>
		public static string SimplyStreamOptimizationSuffix
		{
			get
			{
				return "_O3";
			}
		}
	}
	class SimplyStreamArchitectureConfig : UnrealArchitectureConfig
	{
		public SimplyStreamArchitectureConfig(UnrealTargetPlatform Platform)
			: base(UnrealArchitectureMode.OneTargetPerArchitecture, new[]
			{
				UnrealArch.wasm,
				UnrealArch.wasm32,
				UnrealArch.wasm64,
			})
		{
		}
		public override UnrealArchitectures ActiveArchitectures(FileReference? ProjectFile, string? TargetName)
		{
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, ProjectFile?.Directory, UnrealTargetPlatform.SimplyStream);
			string MemoryModeName = "Memory64_Emulated";
			Ini.GetString("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings", "EmscriptenMemoryMode", out MemoryModeName);
			SimplyStreamToolChain.MemoryMode MemoryMode = SimplyStreamToolChain.ParseMemoryMode(MemoryModeName);
			UnrealArch Architecture = MemoryMode switch
			{
				SimplyStreamToolChain.MemoryMode.Memory32 => UnrealArch.wasm32,
				SimplyStreamToolChain.MemoryMode.Memory64 => UnrealArch.wasm64,
				SimplyStreamToolChain.MemoryMode.Memory64_Emulated => UnrealArch.wasm,
				_ => throw new BuildException($"Unsupported Emscripten memory mode: {MemoryMode}"),
			};
			return new UnrealArchitectures(Architecture);
		}

		public override bool RequiresArchitectureFilenames(UnrealArchitectures Architectures)
		{
			return Architectures.bIsMultiArch || !Architectures.Contains(UnrealArch.wasm);
		}
	}

	class SimplyStreamPlatform : UEBuildPlatform
	{
		/// <summary>
		/// Architecture to build for.
		/// </summary>
		public static readonly UnrealArch DefaultHostArchitecture = UnrealArch.wasm;

		UEBuildPlatformSDK SDK;

/// <summary>
		/// Constructor
		/// </summary>
		public SimplyStreamPlatform(SimplyStreamPlatformSDK InSDK, ILogger Logger)
			: this(UnrealTargetPlatform.SimplyStream, InSDK, Logger)
		{
			SDK = InSDK;
		}

		public SimplyStreamPlatform(UnrealTargetPlatform UnrealTarget, SimplyStreamPlatformSDK InSDK, ILogger Logger)
			: base(UnrealTarget, InSDK, new SimplyStreamArchitectureConfig(UnrealTarget), Logger)
		{
			SDK = InSDK;
		}

		public override void ResetTarget(TargetRules Target)
		{
			ValidateTarget(Target);
		}

		public override void ValidateTarget(TargetRules Target)
		{
			Target.OptimizationLevel = OptimizationMode.Size;
			Target.bForceDisableAutomationTests = true; //Bug
			Target.bCompileFreeType = true;
			Target.bCompileICU = true;
			Target.bUsePCHFiles = true; // STOCK-EMSDK UT4: re-enable PCH so non-IWYU game code gets SharedPCH.Engine
			Target.bDeployAfterCompile = true;
			Target.MinGameModuleSourceFilesForUnityBuild = 0;
			Target.NumIncludedBytesPerUnityCPP = int.MaxValue;
			Target.bDisableModuleNumIncludedBytesPerUnityCPPOverride = true;
			Target.bUseUnityBuild = true;
			Target.bUseAdaptiveUnityBuild = false;
			Target.LinkType = TargetLinkType.Monolithic;
			//Target.LinkType = TargetLinkType.Modular;
			Target.bBuildDeveloperTools = false;
			Target.bUsesCEF3 = false;
			Target.bCompileCEF3 = false;
			Target.bUseLoggingInShipping = false;
			// Installed engine modules and project modules must agree on DO_CHECK-dependent layouts.
			// SimplyStream Shipping follows Unreal's default and keeps checks out of the wasm payload.
			Target.bUseChecksInShipping = false;
			Target.bCompileISPC = false; // wndr-todo: work on ispc compiler to get this
			//Target.bCompileNavmeshClusterLinks = true;
			//Target.bCompileNavmeshSegmentLinks = true;
			Target.bCompileWithAccessibilitySupport = false;
			//Target.bCompileWithPluginSupport = true;
			Target.CppStandard = CppStandardVersion.Latest;
			Target.bWithPushModel = false;
			//Target.bWithServerCode = false;
			Target.bWithLiveCoding = false;
			Target.bUseShippingPhysXLibraries = true;
			Target.bCompileSpeedTree = false;
			if (/*Target.Configuration == UnrealTargetConfiguration.Development || */Target.Configuration == UnrealTargetConfiguration.Test)
			{
				Target.bEnableTrace = true;
			}
			//Target.bAllowHotReload = false;
			//Target.bAllowLTCG = false;
			//Target.StaticAllocator = StaticAllocatorType.Ansi;

			// Consuming the precompiled engine payload (UBT -UsePrecompiled) requires the SHARED build
			// environment so engine-module intermediates resolve to the engine-side UnrealGame tree where
			// the shipped .precompiled manifests live (Upgrade/installed_engine_plan.md B2). Forcing it
			// here means consumer projects need NO Target.cs changes. Global UBT flags do not surface in
			// per-target TargetInfo.Arguments, so check the process command line. BuildCookRun's multi-target
			// invocation embeds each target's options inside one `-Target=...` process argument, so recognize
			// an exact token there as well as a top-level flag. This must run before the plugin-disable gate.
			bool bUsePrecompiled = Environment.GetCommandLineArgs().Any(Argument =>
				Argument.Equals("-UsePrecompiled", StringComparison.OrdinalIgnoreCase)
				|| (Argument.StartsWith("-Target=", StringComparison.OrdinalIgnoreCase)
					&& Argument.Split(' ', '\t').Any(Token =>
						Token.Equals("-UsePrecompiled", StringComparison.OrdinalIgnoreCase))));
			if (Target.Type != TargetType.Program && bUsePrecompiled)
			{
				Target.BuildEnvironment = TargetBuildEnvironment.Shared;
			}

			// The generic UnrealGame producer intentionally disables developer tools, so -AllModules filters
			// Source/Developer and DeveloperTool plugin descriptors before their platform rules are created.
			// Lyra's Development ShooterTestsRuntime module nevertheless links these two engine modules. Add
			// them only to the Development -Precompile producer target; Shipping must not link
			// non-redistributable Developer modules. Normal consumers still link them solely when their
			// own dependency graph asks for them.
			if (Target.bPrecompile && Target.Type == TargetType.Game &&
				Target.Configuration == UnrealTargetConfiguration.Development)
			{
				foreach (string ModuleName in new[] { "CQTest", "CQTestEnhancedInput" })
				{
					if (!Target.ExtraModuleNames.Contains(ModuleName, StringComparer.OrdinalIgnoreCase))
					{
						Target.ExtraModuleNames.Add(ModuleName);
					}
				}
			}

			// Explicit plugin disables on TargetRules are only legal for unique-build-environment targets
			// (UEBuildTarget.ValidateSharedEnvironment rejects them for shared ones). Shared-environment
			// targets — the `UnrealGame -Precompile` producer and precompiled-engine consumers — keep the
			// stock default plugin set; projects that need these off in a shared environment disable them
			// per-project in the .uproject instead.
			//
			// This hook runs TWICE: once from the TargetRules BASE constructor (via ResetTarget, BEFORE a
			// derived constructor can set BuildEnvironment — UnrealGame computes as Unique here) and once
			// post-construction from RulesAssembly. So the Shared branch must actively REMOVE entries the
			// first pass may have added.
			// 2026-06-12: the historical wndr-era blanket list (RemoteSession, HTTPChunkInstaller,
			// UdpMessaging, TcpMessaging, ModelViewViewModel, UIFramework, Harmonics, LearningAgents,
			// CustomMeshComponent, ResonanceAudio) was removed — the `-AllModules -Precompile` producer
			// proved those all compile clean for wasm (Harmonics no longer exists in 5.8). Only plugins
			// whose modules genuinely cannot build for this platform stay disabled here; their modules are
			// additionally excluded from precompile via <Module>_SimplyStream Build.cs overrides.
			string[] DisabledPlugins = new string[]
			{
				"BinkMediaPlayer", // binary-only Bink SDK has no wasm lib + pointer-truncation bug in the module
			};
			TargetBuildEnvironment Env;
			try
			{
				Env = Target.BuildEnvironment;
			}
			catch
			{
				// Unresolved UniqueIfNeeded — could still resolve to Shared, so skip the disables.
				Env = TargetBuildEnvironment.Shared;
			}
			if (Env == TargetBuildEnvironment.Unique)
			{
				Target.DisablePlugins.AddRange(DisabledPlugins.Where(x => !Target.DisablePlugins.Contains(x)));
			}
			else
			{
				Target.DisablePlugins.RemoveAll(x => DisabledPlugins.Contains(x));
			}
		}

		public override bool CanUseXGE()
		{
			return false; // NOTE: setting to true may break CIS builds...
		}

		/// <summary>
		/// Determines if the given name is a build product for a target.
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UE4Editor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the string matches the name of a build product, false otherwise</returns>
		public override bool IsBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes)
		{
			return IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".js")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, SimplyStreamToolChain.libExt);
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The binary extension (ie 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".so";
				case UEBuildBinaryType.Executable:
					return ".html";
				case UEBuildBinaryType.StaticLibrary:
					return SimplyStreamToolChain.libExt;
			}

			return base.GetBinaryExtension(InBinaryType);
		}

		/// <summary>
		/// Get the extensions to use for debug info for the given binary type
		/// </summary>
		/// <param name="InTarget">The target being built</param>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string[]    The debug info extensions (i.e. 'pdb')</returns>
		public override string[] GetDebugInfoExtensions(ReadOnlyTargetRules InTarget, UEBuildBinaryType InBinaryType)
		{
			return Array.Empty<string>();
		}

		/// <summary>
		/// Whether this platform should build a monolithic binary
		/// </summary>
		public override bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
		{
			return true;
		}

		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			Rules.AppendStringToPublicDefinition("PLATFORM_WASM", "0");
			if ((ModuleName == "RHI" || ModuleName == "BuildSettings") && (Target.Platform == Platform || Target.bNeedsExtraShaderFormats))
			{
				AddPlatformStaticShaders(Target, Rules, [
					new StaticShader { Name = "WEBGPU_ES31", Index = 41 },
					new StaticShader { Name = "WEBGPU_SM5", Index = 42 },
				]);
			}

			if (ModuleName == "RHI" && Target.Platform.IsInGroup(UnrealPlatformGroup.Desktop))
			{
				Rules.DynamicallyLoadedModuleNames.Add("WebGPURHI");
			}


			if (!UEBuildPlatform.IsPlatformAvailableForTarget(Platform, Target, bIgnoreSDKCheck: true))
			{
				return;
			}

			if (Target.Platform == UnrealTargetPlatform.Win64 ||
				Target.Platform == UnrealTargetPlatform.Mac ||
				Target.Platform == UnrealTargetPlatform.Linux)
			{
				bool bBuildShaderFormats = Target.bForceBuildShaderFormats;
				if (!Target.bBuildRequiresCookedData)
				{
					if (ModuleName == "Engine")
					{
						if (Target.bBuildDeveloperTools)
						{
							Rules.DynamicallyLoadedModuleNames.Add("SimplyStreamTargetPlatform");
							Rules.DynamicallyLoadedModuleNames.Add("SimplyStreamTargetPlatformSettings");
							Rules.DynamicallyLoadedModuleNames.Add("SimplyStreamTargetPlatformControls");
						}
					}
					else if (ModuleName == "TargetPlatform")
					{
						if (bBuildShaderFormats || Target.bBuildDeveloperTools)
						{
							Rules.DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
							Rules.DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
							Rules.DynamicallyLoadedModuleNames.Add("TextureFormatETC2");
							Rules.DynamicallyLoadedModuleNames.Add("WebGPUShaderFormat");
						}
					}
				}
            }

			if (ModuleName == "UnrealEd")
			{
				Rules.DynamicallyLoadedModuleNames.Add("SimplyStreamPlatformEditor");
			}
		}

		/// <summary>
		/// Modify the rules for a newly created module, in a target that's being built for this platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			// overridable #ifdef's
			Rules.PublicDefinitions.Add("PLATFORM_USE_SHOWFLAGS_ALWAYS_BITFIELD=0"); // i.e. use boolean version
			Rules.PublicDefinitions.Add("ALLOW_LQ_LIGHTMAPS=1");
			Rules.PublicDefinitions.Add("ULANG_PLATFORM_WASM=1");
			Rules.bEnableExceptions = false;

			// Modules that cannot build for wasm are excluded from -AllModules/-Precompile producer builds
			// via platform-extension Build.cs overrides (<Module>_SimplyStream setting
			// PrecompileForTargets=None) — NOT here: the -AllModules filter
			// (UEBuildTarget.AddAllValidModulesToTarget) creates module rules without invoking this hook,
			// so only the rules-class constructor reliably reaches every creation path.

			if (ModuleName == "Core")
			{
				Rules.PublicDependencyModuleNames.Add("zlib");
			}
			else if (ModuleName == "Engine")
			{
				Rules.PrivateDependencyModuleNames.Add("zlib");
				Rules.PrivateDependencyModuleNames.Add("UElibPNG");
				Rules.PublicDependencyModuleNames.Add("UEOgg");
				Rules.PublicDependencyModuleNames.Add("Vorbis");
			}
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="Target">Settings for the target being compiled</param>
		/// <param name="CompileEnvironment">The compile environment for this target</param>
		/// <param name="LinkEnvironment">The link environment for this target</param>
		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			CompileEnvironment.Definitions.Add("PLATFORM_IS_EXTENSION=1");

			// NOTE: removed the UE_UHT_FUNCS_CONSTEXPR=inline const override + the matching UHT
			// codegen edit. That worked around a constexpr reinterpret_cast in ERASE_METHOD_PTR,
			// which does not exist in this UE 5.8 branch — the generated `static constexpr
			// FClassNativeFunction Funcs[]` (entries are `{ UTF8TEXT("x"), &Class::execX }`) is a
			// valid constant expression and compiles under emcc (verified). UHT stays byte-stock.

			// override #def's that are platform extensions friendly
			CompileEnvironment.Definitions.Add("UNICODE");
			CompileEnvironment.Definitions.Add("_UNICODE");
			CompileEnvironment.Definitions.Add("WITH_AUTOMATION_WORKER=0"); // can be enabled
			CompileEnvironment.Definitions.Add("WITH_AUTOMATION_TESTS=0");
			CompileEnvironment.Definitions.Add("WITH_OGGVORBIS=1");
			//CompileEnvironment.Definitions.Add("USE_SCENE_LOCK=0");
			//CompileEnvironment.Definitions.Add("USE_NULL_RHI=0");
			CompileEnvironment.Definitions.Add("WITH_COTF=0");

			if (Target.bPGOOptimize || Target.bPGOProfile)
			{
				Logger.LogInformation("PGO {PgoType} build", Target.bPGOOptimize ? "optimize" : "profile");
				if (Target.bPGOOptimize)
				{
					CompileEnvironment.PGODirectory = DirectoryReference.Combine(Target.ProjectFile?.Directory ?? Unreal.WritableEngineDirectory, "Platforms", "wndr", "Build", "PGO").FullName;
					CompileEnvironment.PGOFilenamePrefix = String.Format("{0}-wndr", Target.Name);

					LinkEnvironment.PGODirectory = CompileEnvironment.PGODirectory;
					LinkEnvironment.PGOFilenamePrefix = CompileEnvironment.PGOFilenamePrefix;

					Logger.LogInformation("PGO Dir: {PgoDir}", CompileEnvironment.PGODirectory);
					Logger.LogInformation("PGO Prefix: {PgoPrefix}", CompileEnvironment.PGOFilenamePrefix);
				}
			}
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>true if debug info should be generated, false if not</returns>
		public override bool ShouldCreateDebugInfo(ReadOnlyTargetRules Target)
		{
			switch (Target.Configuration)
			{
				case UnrealTargetConfiguration.Development:
				case UnrealTargetConfiguration.Shipping:
				case UnrealTargetConfiguration.Test:
					return !Target.bOmitPCDebugInfoInDevelopment;
				case UnrealTargetConfiguration.Debug:
				default:
					// We don't need debug info for Emscripten, and it causes a bunch of issues with linking
					return true;
			};
		}

		/// <summary>
		/// Setup the binaries for this specific platform.
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="ExtraModuleNames"></param>
		public override void AddExtraModules(ReadOnlyTargetRules Target, List<string> ExtraModuleNames)
		{
			if (Target.Type != TargetType.Program)
			{
				ExtraModuleNames.Add("SimplyStreamPlatformFeatures");
				ExtraModuleNames.Add("SimplyStreamAudioMixer");
			}
		}

		public override UEToolChain CreateToolChain(ReadOnlyTargetRules Target)
		{
			ClangToolChainOptions Options = ClangToolChainOptions.None;
			return new SimplyStreamToolChain(Target.ProjectFile, Options, Logger);
		}

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Receipt">Receipt for the target being deployed</param>
		public override void Deploy(TargetReceipt Receipt)
		{
			new UEDeploySimplyStream(Logger).PrepTargetForDeployment(Receipt);
		}
	}

	class UEDeploySimplyStream : UEBuildDeploy
	{
		public UEDeploySimplyStream(ILogger InLogger)
			: base(InLogger)
		{
		}

		public override bool PrepTargetForDeployment(TargetReceipt Receipt)
		{
			return base.PrepTargetForDeployment(Receipt);
		}
	}

	class SimplyStreamPlatformFactory : UEBuildPlatformFactory
	{
		public override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.SimplyStream; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		public override void RegisterBuildPlatforms(ILogger Logger)
		{
			SimplyStreamPlatformSDK SDK = new(Logger);
			SDK.ManageAndValidateSDK();
			UEBuildPlatform.RegisterBuildPlatform(new SimplyStreamPlatform(SDK, Logger), Logger);
		}
	}
}
