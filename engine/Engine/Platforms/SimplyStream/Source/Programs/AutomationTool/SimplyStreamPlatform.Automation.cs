// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

//#define XXX_USE_FASTCOMP // temp: until upstream packages successfully

using System;
using System.IO;
using System.Net.Http;
using System.Text;
using AutomationTool;
using UnrealBuildTool;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using System.Diagnostics;
using EpicGames.Core;
using UnrealBuildBase;

public class SimplyStreamPlatform : Platform
{
	// ini configurations
	static bool enableMultithreading = true;
	static bool bMultithreading_UseOffscreenCanvas = true;
	static bool bUseFixedTimeStep = false;
	static string ClientID = "";
	static string ProjectID = "";
	static bool buildForNode = false;

	public SimplyStreamPlatform()
		: base(UnrealTargetPlatform.SimplyStream)
	{
	}
	public static void CopyEntireDirectory(DirectoryInfo source, DirectoryInfo target, bool overwiteFiles = true)
	{
		if (!source.Exists) return;
		if (!target.Exists) target.Create();

		Parallel.ForEach(source.GetDirectories(), (sourceChildDirectory) =>
			CopyEntireDirectory(sourceChildDirectory, new DirectoryInfo(Path.Combine(target.FullName, sourceChildDirectory.Name))));

		Parallel.ForEach(source.GetFiles(), sourceFile =>
			sourceFile.CopyTo(Path.Combine(target.FullName, sourceFile.Name), overwiteFiles));
	}
	public override void Deploy(ProjectParams Params, DeploymentContext SC)
	{
	}
	static string GetProjectConfiguration(ProjectParams Params)
	{
		return (Params.DedicatedServer ? Params.ServerConfigsToBuild[0] : Params.ClientConfigsToBuild[0]).ToString();
	}
	static string ResolveSimplyStreamCliPath(string LocalRoot)
	{
		string HostDirectory;
		string CliName = "simplystream-cli";
		if (OperatingSystem.IsWindows())
		{
			HostDirectory = "Win64";
			CliName += ".exe";
		}
		else if (OperatingSystem.IsMacOS())
		{
			HostDirectory = "Mac";
		}
		else
		{
			HostDirectory = "Linux";
		}

		string PlatformRoot = CombinePaths(LocalRoot, "Engine/Platforms/SimplyStream");
		string BundledPath = CombinePaths(
			PlatformRoot,
			"Binaries/ThirdParty/SimplyStreamCLI",
			HostDirectory,
			CliName);
		if (File.Exists(BundledPath))
		{
			return BundledPath;
		}

		// Strata and source developers historically overlay the host tool at the platform root.
		// Keep that path as a compatibility fallback, but SDK releases must carry the bundled path.
		string ProvisionedPath = CombinePaths(PlatformRoot, CliName);
		if (File.Exists(ProvisionedPath))
		{
			return ProvisionedPath;
		}

		throw new AutomationException(
			"SimplyStream deploy tool is missing. Expected the SDK-bundled host tool at {0}, " +
			"or a source-build provisioned tool at {1}.",
			BundledPath,
			ProvisionedPath);
	}
	static uint ReadWasmVarUInt32(Stream Stream)
	{
		uint Value = 0;
		for (int Shift = 0; Shift <= 28; Shift += 7)
		{
			int Byte = Stream.ReadByte();
			if (Byte < 0)
			{
				throw new AutomationException("Unexpected end of wasm while reading a section length");
			}
			if (Shift == 28 && (Byte & 0xf0) != 0)
			{
				throw new AutomationException("Invalid wasm varuint32 section length");
			}
			Value |= (uint)(Byte & 0x7f) << Shift;
			if ((Byte & 0x80) == 0)
			{
				return Value;
			}
		}
		throw new AutomationException("Invalid wasm varuint32 section length");
	}
	static void ReadWasmBytes(Stream Stream, byte[] Buffer)
	{
		int Offset = 0;
		while (Offset < Buffer.Length)
		{
			int Count = Stream.Read(Buffer, Offset, Buffer.Length - Offset);
			if (Count == 0)
			{
				throw new AutomationException("Unexpected end of wasm");
			}
			Offset += Count;
		}
	}
	static void ValidateNamesOnServer(string WasmPath)
	{
		string SymbolMapPath = Path.ChangeExtension(WasmPath, ".js.symbols");
		string[] EmscriptenSymbolMapCandidates =
		{
			Path.ChangeExtension(WasmPath, ".html.symbols"),
			Path.ChangeExtension(WasmPath, ".mjs.symbols"),
		};
		string EmscriptenSymbolMapPath = null;
		foreach (string Candidate in EmscriptenSymbolMapCandidates)
		{
			if (!File.Exists(Candidate))
			{
				continue;
			}
			if (EmscriptenSymbolMapPath != null)
			{
				throw new AutomationException(
					"Multiple Emscripten Shipping symbol maps found for {0}: {1}, {2}",
					WasmPath, EmscriptenSymbolMapPath, Candidate);
			}
			EmscriptenSymbolMapPath = Candidate;
		}
		if (EmscriptenSymbolMapPath != null)
		{
			File.Move(EmscriptenSymbolMapPath, SymbolMapPath, true);
			Console.WriteLine(
				"names-on-server: normalized Emscripten symbol map {0} -> {1}",
				Path.GetFileName(EmscriptenSymbolMapPath), Path.GetFileName(SymbolMapPath));
		}
		if (!File.Exists(SymbolMapPath) || new FileInfo(SymbolMapPath).Length == 0)
		{
			throw new AutomationException("Shipping symbol map missing or empty: {0}", SymbolMapPath);
		}

		using FileStream Stream = File.OpenRead(WasmPath);
		byte[] Header = new byte[8];
		ReadWasmBytes(Stream, Header);
		byte[] ExpectedHeader = { 0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00 };
		for (int Index = 0; Index < ExpectedHeader.Length; ++Index)
		{
			if (Header[Index] != ExpectedHeader[Index])
			{
				throw new AutomationException("Invalid wasm header in Shipping binary: {0}", WasmPath);
			}
		}

		while (Stream.Position < Stream.Length)
		{
			int SectionId = Stream.ReadByte();
			if (SectionId < 0)
			{
				break;
			}
			uint SectionSize = ReadWasmVarUInt32(Stream);
			long SectionEnd = Stream.Position + (long)SectionSize;
			if (SectionEnd > Stream.Length)
			{
				throw new AutomationException("Invalid wasm section length in Shipping binary: {0}", WasmPath);
			}

			if (SectionId == 0)
			{
				if (SectionSize == 0)
				{
					throw new AutomationException("Invalid empty wasm custom section in Shipping binary: {0}", WasmPath);
				}
				uint NameLength = ReadWasmVarUInt32(Stream);
				if (NameLength > 1024 || Stream.Position + (long)NameLength > SectionEnd)
				{
					throw new AutomationException("Invalid wasm custom section name in Shipping binary: {0}", WasmPath);
				}
				byte[] NameBytes = new byte[(int)NameLength];
				ReadWasmBytes(Stream, NameBytes);
				string SectionName = Encoding.UTF8.GetString(NameBytes);
				if (SectionName == "name" || SectionName == "producers" || SectionName.StartsWith(".debug_", StringComparison.Ordinal))
				{
					throw new AutomationException(
						"Shipping wasm still contains forbidden custom section '{0}': {1}", SectionName, WasmPath);
				}
			}
			Stream.Position = SectionEnd;
		}

		Console.WriteLine(
			"names-on-server: verified name-free Shipping wasm ({0}); symbol map ready for private upload ({1})",
			Path.GetFileName(WasmPath), Path.GetFileName(SymbolMapPath));
	}
	static void ValidateIasExtentRuntime(string WasmPath)
	{
		string GluePath = Path.ChangeExtension(WasmPath, ".js");
		if (!File.Exists(GluePath))
		{
			throw new AutomationException(
				"IAS extents-v1 requires the target JavaScript glue next to the wasm, but {0} is missing.",
				GluePath);
		}
		string GlueSource = File.ReadAllText(GluePath);
		if (!GlueSource.Contains("-WndrIasExtentBytes=", System.StringComparison.Ordinal))
		{
			throw new AutomationException(
				"IAS extents-v1 publication requires a matching SimplyStream runtime, but {0} does not pass -WndrIasExtentBytes. " +
				"Run `strata ship` or rebuild the scoped 5.8 platform payload before cooking; refusing to publish extents that this wasm would request as legacy /chunks objects.",
				GluePath);
		}
		Console.WriteLine("IAS extents-v1 runtime gate passed: {0}", Path.GetFileName(GluePath));
	}
	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		string ProjectConfiguration = GetProjectConfiguration(Params);

		// ----------------------------------------
		// ini configurations
		ConfigHierarchy ConfigCache = UnrealBuildTool.ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Params.RawProjectPath), UnrealTargetPlatform.SimplyStream);

		ConfigCache.GetBool("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings", "EnableMultithreading", out enableMultithreading);
		ConfigCache.GetBool("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings", "OffscreenCanvas", out bMultithreading_UseOffscreenCanvas);
		ConfigCache.GetBool("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings", "UseFixedTimeStep", out bUseFixedTimeStep);
		ConfigCache.GetString("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings", "ClientID", out ClientID);
		ConfigCache.GetString("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings", "ProjectID", out ProjectID);

		string Hybrid = Environment.GetEnvironmentVariable("WNDR_BUILD_HYBRID")!;
		if (!string.IsNullOrEmpty(Hybrid) && Hybrid == "1")
		{
			// Do something special
			Console.WriteLine("WNDR_BUILD_HYBRID is set to 1, enabling hybrid build mode.");
			buildForNode = true;
		}

		// ----------------------------------------
		// package directory
		string PackagePath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "SimplyStream");
		if (!Directory.Exists(PackagePath))
		{
			Directory.CreateDirectory(PackagePath);
		}
		string _ProjectNameExtra = ProjectConfiguration != "Development" ? "-SimplyStream-" + ProjectConfiguration : "";
		string _ProjectFullpath = Params.GetProjectExeForPlatform(UnrealTargetPlatform.SimplyStream).ToString();
		string targetSuffix = Params.DedicatedServer ? "Server" : "";
		string _ProjectFilename = Params.ShortProjectName + targetSuffix + _ProjectNameExtra;
		string SrcUE4GameBasename = Path.Combine(Path.GetDirectoryName(_ProjectFullpath), _ProjectFilename);
		string UE4GameBasename = Path.Combine(PackagePath, _ProjectFilename);
		string ProjectBasename = Path.Combine(PackagePath, Params.ShortProjectName + _ProjectNameExtra);
		string ShellFile = Path.Combine(PackagePath, Params.ShortProjectName + _ProjectNameExtra + ".html");

		// ----------------------------------------
		// packaging
		if ( !Params.IsCodeBasedProject )
		{
			Console.WriteLine("SrcUE4GameBasename {0}", SrcUE4GameBasename + ".wasm");
			Console.WriteLine("UE4GameBasename {0}", UE4GameBasename + ".wasm");
			File.Copy(SrcUE4GameBasename + ".html", UE4GameBasename + ".html", true);
			File.Copy(SrcUE4GameBasename + ".wasm", UE4GameBasename + ".wasm", true);
			File.Copy(SrcUE4GameBasename + ".js",   UE4GameBasename + ".js",   true);

			File.SetAttributes(UE4GameBasename + ".wasm", FileAttributes.Normal);
			File.SetAttributes(UE4GameBasename + ".js" ,  FileAttributes.Normal);
			if (buildForNode)
			{
				File.Copy(SrcUE4GameBasename + ".mjs", UE4GameBasename + ".mjs", true);
				File.SetAttributes(UE4GameBasename + ".mjs", FileAttributes.Normal);
			}

			if (File.Exists(SrcUE4GameBasename + ".js.symbols"))
			{
				File.Copy(      SrcUE4GameBasename + ".js.symbols", UE4GameBasename + ".js.symbols", true);
				File.SetAttributes(UE4GameBasename + ".js.symbols", FileAttributes.Normal);
			}

			if (enableMultithreading)
			{
				File.Copy(      SrcUE4GameBasename + ".worker.js", UE4GameBasename + ".worker.js", true);
				File.SetAttributes(UE4GameBasename + ".worker.js", FileAttributes.Normal);
			}
			else
			{
				File.Delete(UE4GameBasename + ".worker.js");
			}
		}

		string EmPackagerPath = SimplyStreamSDKInfo.EmscriptenPackager();
		string TemplateFilesPath = CombinePaths(CmdEnv.LocalRoot, "Engine/Platforms/SimplyStream/Build/TemplateFiles");
		string SimplystreamCliPath = ResolveSimplyStreamCliPath(CmdEnv.LocalRoot);
		string BuildPath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Build", "SimplyStream");
		string TemplateFile = CombinePaths(BuildPath, "project_template.html");
		string BinariesPath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "SimplyStream");
		string ProjectName = Params.ShortProjectName;
		string CookedDir1 = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Saved", "Cooked");
		string CookedDir2 = Path.Combine(CookedDir1, SC.CookSourcePlatform.GetCookPlatform(Params.DedicatedServer, Params.Client));
		string depman = Path.Combine(CookedDir2, Params.ShortProjectName, "depmap.bin");
		string filetypes = Path.Combine(CookedDir2, Params.ShortProjectName, "FileTypes.json");
		var StagedDir = Path.Combine(Params.BaseStageDirectory, GetCookPlatform(Params.DedicatedServer, Params.Client));
		string ConfigPath_static = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Config", "prefetch.txt");
		string ConfigPath_static_out = Path.Combine(StagedDir, "prefetch.txt");
		if (File.Exists(ConfigPath_static))
			CopyFile(ConfigPath_static, ConfigPath_static_out);

		//this is been deprecated, set these via the CLI
		string[] lines2 = { "0", "0", ProjectConfiguration, Params.ShortProjectName };
		System.IO.File.WriteAllLines(Path.Combine(StagedDir, "build_info.txt"), lines2);

		// wndr 5.8: aggregate the per-shader WebGPU resource records the cooker emitted (see
		// RecordWebGPUShaderRequirement in WebGPUShaderCompiler.cpp) into a per-build requirements manifest
		// { "perFeatureLevel": { "sm5": { "limits": { "maxSampledTexturesPerShaderStage": N } }, "es31": {..} } }.
		// These are ADVISORY (the launcher floor mode decides if they hard-gate): a device below this still runs — the
		// in-wasm runtime picks a lower tier / skips over-limit pipelines (advisory posture). The number reflects
		// whatever shaders this cook config includes (a shipping cook excludes editor/dev shaders automatically).
		// Shader compilation is multi-process, so records are split across shader_requirements_<pid>.jsonl files;
		// we max them per feature level, write the manifest into the stage, and delete the records so the next
		// cook starts clean. Authoritative on a full cook (the deploy path); empty ⇒ no requirements emitted. The
		// CLI reads the staged webgpu_requirements.json by convention (deploy.cpp) and forwards it on commit.
		try
		{
			// I3: env-overridable records dir (distributed/UBA builds point all workers + this reducer at shared
			// storage); default EngineSaved/WebGPU for local multi-process cooks. Must match WebGPURequirementsDir() in C++.
			string ReqDir = Environment.GetEnvironmentVariable("SS_WEBGPU_REQUIREMENTS_DIR");
			if (string.IsNullOrEmpty(ReqDir)) ReqDir = CombinePaths(CmdEnv.LocalRoot, "Engine", "Saved", "WebGPU");
			string[] LimitKeys = {
				"maxSampledTexturesPerShaderStage",
				"maxSamplersPerShaderStage",
				"maxUniformBuffersPerShaderStage",
				"maxStorageBuffersPerShaderStage",
			};
			var MaxByLevel = new Dictionary<string, Dictionary<string, int>>(); // level -> (limit -> max)
			if (Directory.Exists(ReqDir))
			{
				var RecordFiles = Directory.GetFiles(ReqDir, "shader_requirements_*.jsonl");
				foreach (string RecordFile in RecordFiles)
				{
					foreach (string line in File.ReadLines(RecordFile))
					{
						if (string.IsNullOrWhiteSpace(line)) continue;
						try
						{
							using var doc = System.Text.Json.JsonDocument.Parse(line);
							var root = doc.RootElement;
							if (!root.TryGetProperty("featureLevel", out var lvlEl)) continue;
							string Level = lvlEl.GetString();
							if (Level != "sm5" && Level != "es31") continue;
							if (!MaxByLevel.TryGetValue(Level, out var Limits))
							{
								Limits = new Dictionary<string, int>();
								MaxByLevel[Level] = Limits;
							}
							foreach (string key in LimitKeys)
							{
								if (root.TryGetProperty(key, out var vEl) && vEl.TryGetInt32(out int n))
								{
									if (!Limits.TryGetValue(key, out int Cur) || n > Cur) Limits[key] = n;
								}
							}
						}
						catch { /* skip a malformed record line */ }
					}
				}
				foreach (string RecordFile in RecordFiles)
				{
					try { File.Delete(RecordFile); } catch { /* best-effort cleanup */ }
				}
			}
			if (MaxByLevel.Count > 0)
			{
				// { "perFeatureLevel": { "<level>": { "limits": { <key>: <max>, ... } } } }
				var perFeatureLevel = new Dictionary<string, object>();
				foreach (var kv in MaxByLevel)
				{
					perFeatureLevel[kv.Key] = new Dictionary<string, object> { ["limits"] = kv.Value };
				}
				var manifest = new Dictionary<string, object> { ["perFeatureLevel"] = perFeatureLevel };
				string json = System.Text.Json.JsonSerializer.Serialize(manifest);
				string ManifestPath = Path.Combine(StagedDir, "webgpu_requirements.json");
				File.WriteAllText(ManifestPath, json);
				Console.WriteLine("WebGPU requirements manifest staged: {0}", json);

				// wndr 5.8: surface single-feature-level builds so a missing level is an intentional choice, not a
				// silent surprise. A device that prefers the missing level falls back to the cooked one at runtime
				// (see the clamp in simplystream.js); warn so it's visible at cook time.
				if (MaxByLevel.Count == 1)
				{
					foreach (var only in MaxByLevel.Keys)
					{
						string missing = only == "sm5" ? "es31" : "sm5";
						Console.WriteLine("WebGPU WARNING: this cook recorded shaders for ONLY '{0}' (no '{1}'). Devices that would prefer '{1}' will fall back to '{0}' at runtime — verify this is intentional (check the target's cooked shader formats).", only, missing);
					}
				}
			}
			else
			{
				Console.WriteLine("WebGPU requirements: no cooker records found (manifest omitted; no capability gating).");
			}
		}
		catch (Exception e)
		{
			Console.WriteLine("WebGPU requirements aggregation skipped: {0}", e.Message);
		}

		//all we have to do here now is to create the cache
		{
			//string CmdLine = string.Format("-c fast --serve \"{0}\" --svn \"{0}\" --git \"{0}\" --config \"{0}\" --pname \"{0}\" ", staged_dir, "0", "0", ProjectConfiguration, Params.ShortProjectName);
			var p = Process.Start(
				new ProcessStartInfo("git", "rev-parse --short HEAD")
				{
					CreateNoWindow = true,
					UseShellExecute = false,
					RedirectStandardError = true,
					RedirectStandardOutput = true,
					WorkingDirectory = Environment.CurrentDirectory
				}
			);

			p.WaitForExit();
			string hash = p.StandardOutput.ReadToEnd().TrimEnd();
			string errorInfoIfAny = p.StandardError.ReadToEnd().TrimEnd();
			int type = Params.DedicatedServer == true ? 1 : 0;
			if (buildForNode)
			{
				type = 2;
			}

			string BuildKind = ProjectConfiguration.ToLower(); // "development", "test", or "shipping"
			// wndr 5.8: -t/--texture-format. Previously derived from GetCookPlatform's "_<Flavor>" suffix, but the
			// cook/stage dirs now use the base "SimplyStream" name (the editor cooks there), so there's no suffix to
			// parse. Take the flavor from the platform descriptor instead (DXT/ETC2/ASTC), defaulting to dxt.
			string CookPlatform = GetCookPlatform(Params.DedicatedServer, Params.Client);
			int underscoreIdx = CookPlatform.IndexOf('_');
			string DescFlavor = GetTargetPlatformDescriptor().CookFlavor;
			string TextureFormat = (!string.IsNullOrEmpty(DescFlavor) ? DescFlavor
				: (underscoreIdx >= 0 ? CookPlatform.Substring(underscoreIdx + 1) : "dxt")).ToLower();
			string DeployType = type == 2 ? "--type hybrid" : (type == 1 ? "--type multiplayer" : "--type local");
			string ClientRender = "--client-render webgpu";

			// Pick the wasm matching THIS target. Configured binaries put the configuration after the target name
			// (LyraServer-SimplyStream-Shipping.wasm), so testing only the filename suffix misclassifies every
			// non-Development server and can deploy a coexisting client binary as multiplayer.
			string[] WasmFiles = Directory.GetFiles(BinariesPath, "*.wasm");
			string ServerTargetPrefix = Params.ShortProjectName + "Server";
			System.Predicate<string> IsServerWasm = f => Path.GetFileNameWithoutExtension(f)
				.StartsWith(ServerTargetPrefix, System.StringComparison.OrdinalIgnoreCase);
			string PickedWasm = System.Array.Find(WasmFiles, f => Params.DedicatedServer == IsServerWasm(f));
			if (PickedWasm == null)
			{
				throw new AutomationException(
					"No {0} wasm target found in {1}. Candidates: {2}",
					Params.DedicatedServer ? ServerTargetPrefix : "client",
					BinariesPath,
					string.Join(", ", System.Array.ConvertAll(WasmFiles, f => Path.GetFileName(f))));
			}
			string BinaryName = Path.GetFileNameWithoutExtension(PickedWasm);
			if (ProjectConfiguration.Equals("Shipping", System.StringComparison.OrdinalIgnoreCase))
			{
				ValidateNamesOnServer(PickedWasm);
			}
			if (!Params.DedicatedServer
				&& Environment.GetEnvironmentVariable("WNDR_IAS_STREAM_CONTENT") == "1"
				&& Environment.GetEnvironmentVariable("WNDR_IAS_AUDIT_ONLY") != "1")
			{
				ValidateIasExtentRuntime(PickedWasm);
			}

			if (Params.DedicatedServer)
			{
				if (Environment.GetEnvironmentVariable("WNDR_IAS_STREAM_CONTENT") == "1")
				{
					throw new AutomationException(
						"Dedicated server packaging cannot use WNDR_IAS_STREAM_CONTENT=1 because the Node runtime does not receive browser IAS bootstrap state.");
				}

				if (Environment.GetEnvironmentVariable("WNDR_IAS") == "1")
				{
					string[] ServerPaks = Directory.GetFiles(StagedDir, "*.pak", SearchOption.AllDirectories);
					string[] ServerTocs = Directory.GetFiles(StagedDir, "*.utoc", SearchOption.AllDirectories);
					string[] ServerContainers = Directory.GetFiles(StagedDir, "*.ucas", SearchOption.AllDirectories);
					if (ServerPaks.Length == 0 || ServerTocs.Length == 0 || ServerContainers.Length == 0)
					{
						throw new AutomationException(
							"Dedicated server stage is incomplete under {0}: expected at least one .pak, .utoc, and .ucas before VFS deployment (found pak={1}, utoc={2}, ucas={3}).",
							StagedDir,
							ServerPaks.Length,
							ServerTocs.Length,
							ServerContainers.Length);
					}
					Console.WriteLine(
						"Dedicated server packaged-content gate passed: pak={0}, utoc={1}, ucas={2}",
						ServerPaks.Length,
						ServerTocs.Length,
						ServerContainers.Length);
				}
			}

			string CmdLine = string.Format(
				"deploy \"{0}\" {1} {2} -p \"{3}\" -c \"{4}\" -n \"{5}\" -k {6} -t {7} --binary \"{8}\" --binary-name \"{9}\" --platform wasm",
				StagedDir,            // Staged build directory
				DeployType,           // --type hybrid|local|multiplayer
				ClientRender,         // --client-render webgpu (for local/hybrid)
				ProjectID,            // -p
				ClientID,             // -c
				ProjectName,          // -n
				BuildKind,            // -k (development/test/shipping)
				TextureFormat,        // -t (dxt/etc/astc)
				BinariesPath,         // --binary
				BinaryName  // Strata: cooked TARGET binary matching this deploy (LyraGame client / LyraServer server)
			);
			if (Environment.GetEnvironmentVariable("WNDR_IAS_AUDIT_ONLY") == "1")
			{
				Console.WriteLine("IAS audit-only: staged package is ready; skipping simplystream-cli deploy.");
			}
			else
			{
				Console.WriteLine("simplystream-cli {0}", CmdLine);
				RunAndLog(CmdEnv, SimplystreamCliPath, CmdLine, null, 0, null, ERunOptions.Default | ERunOptions.UTF8Output | ERunOptions.AllowSpew);
			}
		}

		if (!Params.DedicatedServer)
		{
			//LogInformation("h5conf USING_MAPPAKS");
			HTMLPakAutomation PakAutomation = new HTMLPakAutomation(Params, SC);

			// Create Necessary Paks.
			PakAutomation.CreateEnginePak(); //create data file for engine with lz4, then brotli it
			PakAutomation.CreateGamePak();   //create data file for core game with lz4, then brotli it
											 //PakAutomation.CreateContentDirectoryPak(); //we want content directory to be loose files
											 //PakAutomation.CreateMapPak(); //create list of files for map, on client can determine what files we dont have yet and download those async

			// Create All Map Paks which  will be downloaded on the fly.

			// Create Delta Paks if setup.
			List<string> Paks = new List<string>();
			ConfigCache.GetArray("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings", "LevelTransitions", out Paks);

			if (Paks != null)
			{
				foreach (var Pak in Paks)
				{
					var Matched = Regex.Matches(Pak, "\"[^\"]+\"", RegexOptions.IgnoreCase);
					string MapFrom = Path.GetFileNameWithoutExtension(Matched[0].ToString().Replace("\"", ""));
					string MapTo = Path.GetFileNameWithoutExtension(Matched[1].ToString().Replace("\"", ""));
					PakAutomation.CreateDeltaMapPaks(MapFrom, MapTo);
				}
			}
		}

		/*if ( !File.Exists(TemplateFile) )
		{
			// fall back to default UE4 template files
			BuildPath = TemplateFilesPath;
			TemplateFile = CombinePaths(BuildPath, "project_template.html");
		}
		GenerateFileFromTemplate(TemplateFile, ProjectBasename + ".html", Params, ConfigCache);

		TemplateFile = CombinePaths(BuildPath, "project_template.js");
		if ( File.Exists(TemplateFile) )
		{
			GenerateFileFromTemplate(TemplateFile, ProjectBasename + ".UE4.js", Params, ConfigCache);
		}

		TemplateFile = CombinePaths(BuildPath, "project_template.css");
		if ( File.Exists(TemplateFile) )
		{
			GenerateFileFromTemplate(TemplateFile, ProjectBasename + ".css", Params, ConfigCache);
		}*/




		// ----------------------------------------
		// final copies

		// Gather utlity .js files and combine into one file
		string DestinationFile = PackagePath + "/Utility2.js";
		File.Delete(DestinationFile);

		if ( File.Exists(CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/SimplyStreamLaunchHelper.exe")) ) // TODO: platforms-extension: this currently needs to be built manually
		{
			File.Copy(CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/SimplyStreamLaunchHelper.exe"),CombinePaths(PackagePath, "SimplyStreamLaunchHelper.exe"),true);
		}
		PrintRunTime();
	}

	void CompressFile(string Source, string Destination)
	{
		bool DeleteSource = false;

		if(  Source == Destination )
		{
			string CopyOrig = Source + ".Copy";
			File.Copy(Source, CopyOrig);
			Source = CopyOrig;
			DeleteSource = true;
		}

		if (DeleteSource)
		{
			File.Delete(Source);
		}
	}

	protected void GenerateFileFromTemplate(string InTemplateFile, string InOutputFile, ProjectParams Params, ConfigHierarchy ConfigCache)
	{
		bool IsContentOnly = !Params.IsCodeBasedProject;
		string ProjectConfiguration = GetProjectConfiguration(Params);

		string UE4GameName = IsContentOnly ? "UE4Game" : Params.ShortProjectName;
		string ProjectName = Params.ShortProjectName;
		if (ProjectConfiguration != "Development")
		{
			UE4GameName += "-SimplyStream-" + ProjectConfiguration;
			ProjectName += "-SimplyStream-" + ProjectConfiguration;
		}

		string CanvasScaleMode;
		ConfigCache.GetString("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings", "CanvasScalingMode", out CanvasScaleMode);

		StringBuilder outputContents = new StringBuilder();
		using (StreamReader reader = new StreamReader(InTemplateFile))
		{
			string LineStr = null;
			while (reader.Peek() != -1)
			{
				LineStr = reader.ReadLine();
				if (LineStr.Contains("%TIMESTAMP%"))
				{
					string TimeStamp = DateTime.UtcNow.ToString("yyyyMMddHHmm");
					LineStr = LineStr.Replace("%TIMESTAMP%", TimeStamp);
				}

				if (LineStr.Contains("%SHORTNAME%"))
				{
					LineStr = LineStr.Replace("%SHORTNAME%", Params.ShortProjectName);
				}

				if (LineStr.Contains("%UE4GAMENAME%"))
				{
					LineStr = LineStr.Replace("%UE4GAMENAME%", UE4GameName);
				}

				if (LineStr.Contains("%PROJECTNAME%"))
				{
					LineStr = LineStr.Replace("%PROJECTNAME%", ProjectName);
				}

				if (LineStr.Contains("%UE4CMDLINE%"))
				{
					string ArgumentString = "'../../../" + Params.ShortProjectName + "/" + Params.ShortProjectName + ".uproject',";
					ArgumentString += "'-stdout',"; // suppress double printing to console.log
					if (bUseFixedTimeStep)
					{
						ArgumentString += "'-UseFixedTimeStep',";
					}
					LineStr = LineStr.Replace("%UE4CMDLINE%", ArgumentString);
				}

				if (LineStr.Contains("%CANVASSCALEMODE%"))
				{
					string mode = "2 /*ASPECT*/"; // default
					if ( CanvasScaleMode.Equals("stretch", StringComparison.InvariantCultureIgnoreCase))
					{
						mode = "1 /*STRETCH*/";
					}
					else if ( CanvasScaleMode.Equals("fixed", StringComparison.InvariantCultureIgnoreCase))
					{
						mode = "3 /*FIXED*/";
					}
					LineStr = LineStr.Replace("%CANVASSCALEMODE%", mode);
				}

				if (LineStr.Contains("%MULTITHREADED%"))
				{
					LineStr = LineStr.Replace("%MULTITHREADED%", enableMultithreading ? "true" : "false");
				}

				if (LineStr.Contains("%OFFSCREENCANVAS%"))
				{
					LineStr = LineStr.Replace("%OFFSCREENCANVAS%", bMultithreading_UseOffscreenCanvas ? "true" : "false");
				}

				if (LineStr.Contains("%EMSDK_VERSION%"))
				{
					string escpath = SimplyStreamSDKInfo.EMSCRIPTEN_ROOT.Replace("\\", "/");
					LineStr = LineStr.Replace("%EMSDK_VERSION%", (ProjectConfiguration == "Shipping") ? SimplyStreamSDKInfo.EmscriptenVersion() : escpath);
				}

				outputContents.AppendLine(LineStr);
			}
		}

		if (outputContents.Length > 0)
		{
			// Save the file
			try
			{
				Directory.CreateDirectory(Path.GetDirectoryName(InOutputFile));
				File.WriteAllText(InOutputFile, outputContents.ToString(), Encoding.UTF8);
			}
			catch (Exception)
			{
				// Unable to write to the project file.
			}
		}
	}

	protected void GenerateMacCommandFromTemplate(string InTemplateFile, string InOutputFile, string InMonoPath)
	{
		StringBuilder outputContents = new StringBuilder();
		using (StreamReader reader = new StreamReader(InTemplateFile))
		{
			string InMonoPathParent = Path.GetDirectoryName(InMonoPath);
			string LineStr = null;
			while (reader.Peek() != -1)
			{
				LineStr = reader.ReadLine();
				if (LineStr.Contains("${unreal_mono_pkg_path}"))
				{
					LineStr = LineStr.Replace("${unreal_mono_pkg_path}", InMonoPath);
				}
				if (LineStr.Contains("${unreal_mono_pkg_path_base}"))
				{
					LineStr = LineStr.Replace("${unreal_mono_pkg_path_base}", InMonoPathParent);
				}

				outputContents.Append(LineStr + '\n');
			}
		}

		if (outputContents.Length > 0)
		{
			// Save the file. We Copy the template file to keep any permissions set to it.
			try
			{
				Directory.CreateDirectory(Path.GetDirectoryName(InOutputFile));
				if (File.Exists(InOutputFile))
				{
					File.SetAttributes(InOutputFile, File.GetAttributes(InOutputFile) & ~FileAttributes.ReadOnly);
					File.Delete(InOutputFile);
				}
				File.Copy(InTemplateFile, InOutputFile);
				File.SetAttributes(InOutputFile, File.GetAttributes(InOutputFile) & ~FileAttributes.ReadOnly);
//				System.Diagnostics.Process.Start("chmod", "+x " + InOutputFile);
				using (var CmdFile = File.Open(InOutputFile, FileMode.OpenOrCreate | FileMode.Truncate))
				{
					Byte[] BytesToWrite = new UTF8Encoding(true).GetBytes(outputContents.ToString());
					CmdFile.Write(BytesToWrite, 0, BytesToWrite.Length);
				}
			}
			catch (Exception)
			{
				// Unable to write to the project file.
			}
		}
	}

	// --------------------------------------------------------------------------------
	// ArchiveCommand.Automation.cs

	public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
	{
		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}

		// copy to archive directory
		string PackagePath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "SimplyStream");

		// For server builds, append "Server" suffix to match the actual target name
		string targetSuffix = Params.DedicatedServer ? "Server" : "";
		string UE4GameBasename = Params.ShortProjectName + targetSuffix;
		string ProjectBasename = Params.ShortProjectName;
		string ProjectConfiguration = GetProjectConfiguration(Params);
		if (ProjectConfiguration != "Development")
		{
			UE4GameBasename += "-SimplyStream-" + ProjectConfiguration;
			ProjectBasename += "-SimplyStream-" + ProjectConfiguration;
		}

		SC.ArchiveFiles(PackagePath, UE4GameBasename + ".wasm");			// MAIN game code
		SC.ArchiveFiles(PackagePath, UE4GameBasename + (Params.DedicatedServer || buildForNode ? ".mjs" : ".js")); // MAIN js file (emscripten)
		SC.ArchiveFiles(PackagePath, "data");							// utilities
		SC.ArchiveFiles(PackagePath, ProjectBasename + ".data");			// DATA file
		SC.ArchiveFiles(PackagePath, ProjectBasename + ".data.js");			// DATA file js driver (emscripten)
		SC.ArchiveFiles(PackagePath, ProjectBasename + ".UE4.js");			// UE4 js file
		SC.ArchiveFiles(PackagePath, ProjectBasename + ".css");				// UE4 css file
		SC.ArchiveFiles(PackagePath, ProjectBasename + ".html");			// landing page.

		// The project runner consumes .js.symbols from BinariesPath after this deploy allocates a revision,
		// then uploads it to the private symbol store.
		// Never archive the map with public build files.

		if (enableMultithreading)
		{
			SC.ArchiveFiles(PackagePath, UE4GameBasename + ".worker.js");
		}

		if (HTMLPakAutomation.CanCreateMapPaks(Params))
		{
			// find all paks.
			string[] Files = Directory.GetFiles(Path.Combine(PackagePath, Params.ShortProjectName), "*", SearchOption.AllDirectories);
			foreach(string PakFile in Files)
			{
				SC.ArchivedFiles.Add(PakFile, Path.GetFileName(PakFile));
			}
		}
	}

	// --------------------------------------------------------------------------------
	// RunProjectCommand.Automation.cs

	public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		// look for browser
		string BrowserPath = Params.Devices[0].Replace("SimplyStream@", "");

		// open the webpage
		Int32 ServerPort = 8000; // SimplyStreamLaunchHelper default

		var ConfigCache = UnrealBuildTool.ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Params.RawProjectPath), UnrealTargetPlatform.SimplyStream);
		//ConfigCache.GetInt32("/Script/SimplyStreamPlatformEditor.SimplyStreamTargetSettings", "DeployServerPort", out ServerPort); // LaunchOn via Editor or FrontEnd
		string WorkingDirectory = Path.GetDirectoryName(ClientApp);
		string url = Path.GetFileName(ClientApp) +".html";

		// Check SimplyStreamLaunchHelper source for command line args

		var LowerBrowserPath = BrowserPath.ToLower();
		var ProfileDirectory = Path.Combine(Unreal.UserSettingDirectory.FullName, "UE4_SimplyStream", "user");

		string BrowserCommandline = url;

		if (LowerBrowserPath.Contains("chrome"))
		{
			ProfileDirectory = Path.Combine(ProfileDirectory, "chrome");
			// removing [--enable-logging] otherwise, chrome breaks with a bunch of the following errors:
			// > ERROR:process_info.cc(631)] range at 0x7848406c00000000, size 0x1a4 fully unreadable
			// leaving this note here for future reference: UE-45078
			BrowserCommandline  += "  " + String.Format("--user-data-dir=\\\"{0}\\\"   --no-first-run", ProfileDirectory);
		}
		else if (LowerBrowserPath.Contains("firefox"))
		{
			ProfileDirectory = Path.Combine(ProfileDirectory, "firefox");
			BrowserCommandline += "  " +  String.Format("-no-remote -profile \\\"{0}\\\"", ProfileDirectory);
		}
		else if (LowerBrowserPath.Contains("safari"))
		{
			// NOT SUPPORTED: cannot have a separate UE4 profile for safari
			// -- this "can" be done with a different user (e.g. guest) account...
			//    (which is not a turn key solution that can be done within UE4)
			// -- some have tried using symlinks to "mimic" this
			//    https://discussions.apple.com/thread/3327990
			// -- but, none of these are fool proof with an existing/running safari instance

			// -- also, "Safari Extensions JS" has been officially deprecated as of Safari 12
			//    (in favor of using "Safari App Extension")
			//    https://developer.apple.com/documentation/safariextensions

			// this means, Safari "LaunchOn" (UE4 Editor -> Launch -> Safari) will run with your FULL
			// Safari profile -- so, all of your "previously opened" tabs will all also be opened...
		}

		// TODO: test on other platforms to remove this first if() check
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
		{
			if (!Directory.Exists(ProfileDirectory))
			{
				Directory.CreateDirectory(ProfileDirectory);
			}
		}

		string LauncherArguments = string.Format(" -Browser=\"{0}\" + -BrowserCommandLine=\"{1}\" -ServerPort=\"{2}\" -ServerRoot=\"{3}\" ",
				new object[] { BrowserPath, BrowserCommandline, ServerPort, WorkingDirectory });

		var LaunchHelperPath = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/SimplyStreamLaunchHelper.exe");
		IProcessResult BrowserProcess = Run(LaunchHelperPath, LauncherArguments, null, ClientRunFlags | ERunOptions.NoWaitForExit);

		return BrowserProcess;
	}

	public override List<FileReference> GetExecutableNames(DeploymentContext SC)
	{
		List<FileReference> ExecutableNames = new List<FileReference>();
		ExecutableNames.Add(FileReference.Combine(SC.ProjectRoot, "Binaries", "SimplyStream", SC.ShortProjectName));
		return ExecutableNames;
	}

	// --------------------------------------------------------------------------------
	// PackageCommand.Automation.cs

	//public override bool RequiresPackageToDeploy
	//{
	//	get { return true; }
	//}

	// --------------------------------------------------------------------------------
	// CookCommand.Automation.cs

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		// A dedicated-server cook targets the flavorless "SimplyStreamServer" platform (registered in
		// SimplyStreamTargetPlatformControlsModule) so the cooker strips client-only data (audio etc.) and the
		// cooked content matches the LyraServer build (else: SoundWave "Serial size mismatch" at server boot).
		return bDedicatedServer ? "SimplyStreamServer" : "SimplyStream";
	}

	public override string GetCookExtraCommandLine(ProjectParams Params)
	{
		return HTMLPakAutomation.CanCreateMapPaks(Params) ? " -GenerateDependenciesForMaps " : "";
	}

	// --------------------------------------------------------------------------------
	// CopyBuildToStagingDirectory.Automation.cs

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		if (Params.DedicatedServer || Environment.GetEnvironmentVariable("WNDR_IAS") != "1")
		{
			return;
		}

		// Renderer declares this as NonUFS by default. With IoStore, non-package UFS files are
		// packed into the companion .pak, while NonUFS files stay loose and were only reachable
		// through the legacy asset VFS. Nanite opens this file synchronously during PreRHI init,
		// so it must live in the early-mounted PathPak when the legacy manifest is disabled.
		FileReference TessellationTable = FileReference.Combine(SC.EngineRoot, "Content", "Renderer", "TessellationTable.bin");
		DirectoryReference EngineContent = DirectoryReference.Combine(SC.EngineRoot, "Content");
		int PromotedDependencies = 0;
		List<string> UnmigratedEngineContent = new List<string>();
		foreach (StageTarget Target in SC.StageTargets)
		{
			foreach (RuntimeDependency Dependency in Target.Receipt.RuntimeDependencies)
			{
				if (Dependency.Path.Equals(TessellationTable))
				{
					Dependency.Type = StagedFileType.UFS;
					PromotedDependencies++;
				}
				else if (Dependency.Type == StagedFileType.NonUFS && Dependency.Path.IsUnderDirectory(EngineContent))
				{
					UnmigratedEngineContent.Add(Dependency.Path.MakeRelativeTo(SC.EngineRoot));
				}
			}
		}

		if (PromotedDependencies == 0)
		{
			throw new AutomationException("IAS staging expected the Renderer TessellationTable.bin runtime dependency");
		}
		if (UnmigratedEngineContent.Count > 0)
		{
			throw new AutomationException(
				"IAS staging found read-only Engine/Content runtime dependencies still marked NonUFS: {0}. " +
				"Classify each dependency explicitly; files opened through Unreal's filesystem should be UFS/PathPak.",
				String.Join(", ", UnmigratedEngineContent));
		}
		Console.WriteLine("[wndr-IAS] staging Engine/Content/Renderer/TessellationTable.bin in PathPak");
	}

	public override PakType RequiresPak(ProjectParams Params)
	{
		// wndr IAS (Upgrade/iostore_and_ranges_plan.md — M0): under WNDR_IAS=1 force paks so the stage emits
		// IoStore containers (.ucas/.utoc) that `-ApplyIoStoreOnDemand` converts to on-demand. Without the
		// flag the legacy whole-file VFS path is unchanged (PakType.Never). Reversible env gate.
		if (Environment.GetEnvironmentVariable("WNDR_IAS") == "1")
		{
			return PakType.Always;
		}
		return PakType.Never;
	}

	public override string GetPlatformPakCommandLine(ProjectParams Params, DeploymentContext SC)
	{
		return "";
	}

	// --------------------------------------------------------------------------------
	// AutomationUtils/Platform.cs

	public override bool IsSupported { get { return true; } }

	// --------------------------------------------------------------------------------
	// --------------------------------------------------------------------------------

	private static IDictionary<string, string> MimeTypeMapping = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase)
		{
			// the following will default to: "application/octet-stream"
			// .data

			{ ".wasm", "application/wasm" },
			{ ".htaccess", "text/plain"},
			{ ".html", "text/html"},
			{ ".css", "text/css"},
			{ ".js", "application/x-javascript" },
			{ ".symbols", "text/plain"},
			{ ".txt", "text/plain"}
		};
}

public class SimplyStreamPlatformDXT : SimplyStreamPlatform
{
	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		// wndr 5.8: the editor cooks to Saved/Cooked/SimplyStream (base name) — BuildCookRun doesn't apply the
		// flavor suffix to -TargetPlatform. Return the base name so the cook source dir, staged dir, and deploy
		// all read where the content actually is. (-t is taken from GetTargetPlatformDescriptor's flavor = DXT.)
		// This is also the shape of the planned single-build (one "SimplyStream" name, all formats via Multi).
		// Dedicated server → flavorless "SimplyStreamServer" (strips client-only/audio data to match LyraServer).
		return bDedicatedServer ? "SimplyStreamServer" : (bIsClientOnly ? "SimplyStreamClient" : "SimplyStream");
	}

	public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
	{
		return new TargetPlatformDescriptor(TargetPlatformType, "DXT");
	}
}

public class SimplyStreamPlatformETC : SimplyStreamPlatform
{
	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		return bDedicatedServer ? "SimplyStreamServer" : (bIsClientOnly ? "SimplyStream_ETC2Client" : "SimplyStream_ETC2");
	}

	public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
	{
		return new TargetPlatformDescriptor(TargetPlatformType, "ETC2");
	}
}

public class SimplyStreamPlatformASTC : SimplyStreamPlatform
{
	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		return bDedicatedServer ? "SimplyStreamServer" : (bIsClientOnly ? "SimplyStream_ASTCClient" : "SimplyStream_ASTC");
	}
	public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
	{
		return new TargetPlatformDescriptor(TargetPlatformType, "ASTC");
	}
}
