// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Diagnostics;
using EpicGames.Core;

public class HTMLPakAutomation
{
	ProjectParams Params;

	DeploymentContext SC;

	string PakOrderFileLocation;

	Dictionary<string, object> DependencyJson = new Dictionary<string, object>();

	public static bool CanCreateMapPaks(ProjectParams Param)
	{
		return true;
	}

	public HTMLPakAutomation(ProjectParams InParam, DeploymentContext InSC)
	{
		Params = InParam;
		SC = InSC;

		var PakOrderFileLocationBase = CommandUtils.CombinePaths(SC.ProjectRoot.FullName, "Build", SC.FinalCookPlatform, "FileOpenOrder");
		PakOrderFileLocation = CommandUtils.CombinePaths(PakOrderFileLocationBase, "GameOpenOrder.log");
		if (!CommandUtils.FileExists_NoExceptions(PakOrderFileLocation))
		{
			// Use a fall back, it doesn't matter if this file exists or not. GameOpenOrder.log is preferred however
			PakOrderFileLocation = CommandUtils.CombinePaths(PakOrderFileLocationBase, "EditorOpenOrder.log");
		}

		string PakPath = Path.Combine(new string[] { Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "SimplyStream", SC.ShortProjectName });
		if (Directory.Exists(PakPath))
		{
			Directory.Delete(PakPath, true);
		}

		// wndr 5.8: the SimplyStream cook does not emit MapDependencyGraph.json, and this read's result was never
		// used (dead code carried over from the 5.7 port). It threw DirectoryNotFoundException during staging, so
		// it has been removed.
	}

	/// <summary>
	/// Create Emscripten Data package with Necessary Packages.
	///
	/// </summary>
	public void CreateEmscriptenDataPackage(string PackagePath, string FinalDataLocation)
	{

	}
	public static string GetRelativePath(string fullPath, string basePath)
	{
		// Require trailing backslash for path
		if (!basePath.EndsWith("\\"))
			basePath += "\\";

		Uri baseUri = new Uri(basePath);
		Uri fullUri = new Uri(fullPath);

		Uri relativeUri = baseUri.MakeRelativeUri(fullUri);

		// Uri's use forward slashes so convert back to backward slashes
		return relativeUri.ToString().Replace("/", "\\");

	}
	/// <summary>
	/// Create a Pak from Engine Contents.
	/// </summary>
	public void CreateEnginePak()
	{
		string StagedDir = Path.Combine(Params.BaseStageDirectory, SC.CookSourcePlatform.GetCookPlatform(Params.DedicatedServer, Params.Client));

		string ConfigPath_engine = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Config", "donot_preload.txt");
		HashSet<string> hash = new HashSet<string>();
		string[] lines = new string[0];
		if (File.Exists(ConfigPath_engine))
		{
			string temp = File.ReadAllText(ConfigPath_engine);
			lines = temp.Split(
				new[] { "\r\n", "\r", "\n" },
				StringSplitOptions.None
			);
			hash = new HashSet<string>(lines);
		}

		string[] files = Directory.GetFiles(Path.Combine(StagedDir, "Engine"), "*", SearchOption.AllDirectories);

		Dictionary<string, string> UnrealPakResponseFile = new Dictionary<string, string>();

		//RunUnrealPak(UnrealPakResponseFile, PakPath + "\\EnginePak.pak", null, "", true);
	}


	/// <summary>
	/// Create a pak from a minimum set of game files
	/// </summary>
	public void CreateGamePak()
	{
		// create a pak of game config and first level files.

		string StagedDir = Path.Combine(Params.BaseStageDirectory, SC.CookSourcePlatform.GetCookPlatform(Params.DedicatedServer, Params.Client));

		string PakPath = Path.Combine(new string[] { Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "SimplyStream", Params.ShortProjectName, "Content", "Paks" });
		if (!Directory.Exists(PakPath))
		{
			Directory.CreateDirectory(PakPath);
		}

		List<string> AllFiles = new List<string>();
		try
		{
			string[] SlateFiles = Directory.GetFiles(Path.Combine(StagedDir, Params.ShortProjectName, "Content", "Slate"), "*", SearchOption.AllDirectories);
			AllFiles.AddRange(SlateFiles);
		}
		catch (Exception /*Ex*/)
		{
			// not found.
		}

		try
		{
			string[] Resources = Directory.GetFiles(Path.Combine(StagedDir, Params.ShortProjectName, "Content", "Resources"), "*", SearchOption.AllDirectories);
			AllFiles.AddRange(Resources);
		}
		catch (Exception /*Ex*/)
		{
			// not found.
		}

		// wndr IAS (path-pak): the project Config is packed into the cooked .pak (NonUFS) and is no longer staged loose
		// under <PN>/Config (the deploy-side loose-staging that used to create it was removed in favour of mounting the
		// .pak). This vestigial gather (RunUnrealPak below is commented out) must tolerate a missing dir, exactly like the
		// Slate/Resources gathers above — otherwise GetFiles throws an unhandled DirectoryNotFoundException and aborts Package.
		try
		{
			string[] ConfigFiles = Directory.GetFiles(Path.Combine(StagedDir, Params.ShortProjectName, "Config"), "*", SearchOption.AllDirectories);
			AllFiles.AddRange(ConfigFiles);
		}
		catch (Exception /*Ex*/)
		{
			// not found.
		}

		try
		{
			string[] TopLevelFiles = Directory.GetFiles(Path.Combine(StagedDir, Params.ShortProjectName), "*", SearchOption.TopDirectoryOnly);
			AllFiles.AddRange(TopLevelFiles);
		}
		catch (Exception /*Ex*/)
		{
			// not found.
		}

		//RunUnrealPak(UnrealPakResponseFile, PakPath + "\\" + Params.ShortProjectName + ".pak", null, "", true);

	}

	/// <summary>
	/// Create ContentDirectory Pak.
	/// </summary>
	public void CreateContentDirectoryPak()
	{
		object[] Value = (object[])DependencyJson["ContentDirectoryAssets"];
		List<string> ContentDirectory = Array.ConvertAll<object, string>(Value, Convert.ToString).ToList();
		CreatePak("ContentDirectoryAssets", ContentDirectory);
	}


	/// <summary>
	/// Create Maps Paks based on JSON file created during Cooking. (-GenerateDependenciesForMaps)
	/// </summary>
	public void CreateMapPak()
	{
		// read in the json file.

		object[] Value = (object[])DependencyJson["ContentDirectoryAssets"];
		List<string> ContentDirectory = Array.ConvertAll<object, string>(Value, Convert.ToString).ToList();
		string ConfigPath_maps = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Config", "preload_maps.txt");
		string temp = CommandUtils.ReadAllText(ConfigPath_maps);
		string[] lines = temp.Split(
			new[] { "\r\n", "\r", "\n" },
			StringSplitOptions.None
		);
		foreach (string Pak in DependencyJson.Keys)
		{
			bool skip = true;
			foreach (string line in lines)
			{
				if (Path.GetFileName(Pak) == line)
				{
					skip = false;
					break;
				}
			}
			if (skip) continue;
			//if (Pak != "/Game/Maps/login_map" && Pak != "/Game/Maps/ClassSelectScreen" && Pak != "/Game/Maps/LoadingScreen") continue;
			object[] ObjectList = (object[])DependencyJson[Pak];
			List<string> Assets = Array.ConvertAll<object, string>(ObjectList, Convert.ToString).ToList();

			// add itself.
			Assets.Add(Pak);

			if (Pak != "ContentDirectoryAssets")
			{
				// remove content directory assets.
				Assets = Assets.Except(ContentDirectory).ToList();
				string[] Tokens = Pak.Split('/');
				CreatePak(Tokens[Tokens.Length - 1], Assets);
			}
		}

	}

	/// <summary>
	/// Create Delta Paks based on Level Transitions.
	/// </summary>
	/// <param name="MapFrom"></param>
	/// <param name="MapTo"></param>
	public void CreateDeltaMapPaks(string MapFrom, string MapTo)
	{
		foreach (string NameKey in DependencyJson.Keys)
		{
			if (NameKey.Contains(MapFrom))
			{
				MapFrom = NameKey;
			}
			if (NameKey.Contains(MapTo))
			{
				MapTo = NameKey;
			}
		}

		List<string> MapFromAssets = Array.ConvertAll<object, string>(DependencyJson[MapFrom] as object[], Convert.ToString).ToList();
		MapFromAssets.Add(MapFrom);
		List<string> MapToAssets = Array.ConvertAll<object, string>(DependencyJson[MapTo] as object[], Convert.ToString).ToList();
		MapToAssets.Add(MapTo);

		object[] Value = (object[])DependencyJson["ContentDirectoryAssets"];
		List<string> ContentDirectory = Array.ConvertAll<object, string>(Value, Convert.ToString).ToList();

		// Delta.
		List<string> DeltaAssets = MapToAssets.Except(MapFromAssets).Except(ContentDirectory).ToList();

		// create a pak file for this delta.
		string[] Tokens_MapFrom = MapFrom.Split('/');
		string[] Tokens_MapTo = MapTo.Split('/');

		string DeltaPakName = Tokens_MapFrom[Tokens_MapFrom.Length - 1] + "_" + Tokens_MapTo[Tokens_MapTo.Length - 1];
		CreatePak(DeltaPakName, DeltaAssets);
	}

	/// <summary>
	/// Writes a pak response file to disk
	/// </summary>
	/// <param name="Filename"></param>
	/// <param name="ResponseFile"></param>
	private static void WritePakResponseFile(string Filename, Dictionary<string, string> ResponseFile, bool Compressed)
	{
		Directory.CreateDirectory(Path.GetDirectoryName(Filename));
		using (var Writer = new StreamWriter(Filename, false, new System.Text.UTF8Encoding(true)))
		{
			foreach (var Entry in ResponseFile)
			{
				string Line = String.Format("\"{0}\" \"{1}\"", Entry.Key, Entry.Value);
				Writer.WriteLine(Line);
			}
		}
	}

	/// <summary>
	/// Create a Pak file
	/// </summary>
	private void RunUnrealPak(Dictionary<string, string> UnrealPakResponseFile, string OutputLocation, string EncryptionKeys, string PlatformOptions, bool Compressed)
	{
		if (UnrealPakResponseFile.Count < 1)
		{
			return;
		}
		string PakName = Path.GetFileNameWithoutExtension(OutputLocation);
		string UnrealPakResponseFileName = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, "PakList_" + PakName + ".txt");
		WritePakResponseFile(UnrealPakResponseFileName, UnrealPakResponseFile, Compressed);

		var UnrealPakExe = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine/Binaries/Win64/UnrealPak.exe");

		string CmdLine = CommandUtils.MakePathSafeToUseWithCommandLine(OutputLocation) + " -create=" + CommandUtils.MakePathSafeToUseWithCommandLine(UnrealPakResponseFileName);
		if (!String.IsNullOrEmpty(EncryptionKeys))
		{
			CmdLine += " -sign=" + CommandUtils.MakePathSafeToUseWithCommandLine(EncryptionKeys);
		}
		CmdLine += " -order=" + CommandUtils.MakePathSafeToUseWithCommandLine(PakOrderFileLocation);
		if (GlobalCommandLine.UTF8Output)
		{
			CmdLine += " -UTF8Output";
		}
		CmdLine += PlatformOptions;
		CommandUtils.RunAndLog(CommandUtils.CmdEnv, UnrealPakExe, CmdLine, Options: CommandUtils.ERunOptions.Default | CommandUtils.ERunOptions.UTF8Output);
	}

	/// <summary>
	/// Create a Pak.
	/// </summary>
	/// <param name="MapFrom"></param>
	/// <param name="MapTo"></param>
	public void CreatePak(string Name, List<string> Assets)
	{
		string CookedDir = Path.Combine(Params.BaseStageDirectory, SC.CookSourcePlatform.GetCookPlatform(Params.DedicatedServer, Params.Client));

		Assets.Add(Name);
		List<string> Files = new List<string>();
		foreach (string Asset in Assets)
		{
			string GameDir = "/Game/";
			bool found = false;
			if (Asset.StartsWith(GameDir))
			{
				string PathOnDiskasset = CommandUtils.CombinePaths(CookedDir, Params.ShortProjectName, "Content", Asset.Remove(0, GameDir.Length) + ".uasset");
				string PathOnDiskmap = CommandUtils.CombinePaths(CookedDir, Params.ShortProjectName, "Content", Asset.Remove(0, GameDir.Length) + ".umap");
				string PathOnDiskexp = CommandUtils.CombinePaths(CookedDir, Params.ShortProjectName, "Content", Asset.Remove(0, GameDir.Length) + ".uexp");
				string PathOnDisksbc = CommandUtils.CombinePaths(CookedDir, Params.ShortProjectName, "Content", Asset.Remove(0, GameDir.Length) + ".ushaderbytecode");
				string PathOnDiskfont = CommandUtils.CombinePaths(CookedDir, Params.ShortProjectName, "Content", Asset.Remove(0, GameDir.Length) + ".ufont");
				if (File.Exists(PathOnDiskmap))
				{
					found = true;
					Files.Add(PathOnDiskmap);
				}
				if (File.Exists(PathOnDiskexp))
				{
					found = true;
					Files.Add(PathOnDiskexp);
				}
				if (File.Exists(PathOnDiskasset))
				{
					found = true;
					Files.Add(PathOnDiskasset);
				}
				if (File.Exists(PathOnDisksbc))
				{
					found = true;
					Files.Add(PathOnDisksbc);
				}
				if (File.Exists(PathOnDiskfont))
				{
					found = true;
					Files.Add(PathOnDiskfont);
				}
			}
			else
			{
				string PathOnDiskasset = CommandUtils.CombinePaths(CookedDir, "Content", Asset + ".uasset");
				string PathOnDiskmap = CommandUtils.CombinePaths(CookedDir, "Content", Asset + ".umap");
				string PathOnDiskexp = CommandUtils.CombinePaths(CookedDir, "Content", Asset + ".uexp");
				string PathOnDisksbc = CommandUtils.CombinePaths(CookedDir, "Content", Asset + ".ushaderbytecode");
				string PathOnDiskfont = CommandUtils.CombinePaths(CookedDir, "Content", Asset + ".ufont");
				if (File.Exists(PathOnDiskmap))
				{
					found = true;
					Files.Add(PathOnDiskmap);
				}
				if (File.Exists(PathOnDiskexp))
				{
					found = true;
					Files.Add(PathOnDiskexp);
				}
				if (File.Exists(PathOnDiskasset))
				{
					found = true;
					Files.Add(PathOnDiskasset);
				}
				if (File.Exists(PathOnDisksbc))
				{
					found = true;
					Files.Add(PathOnDisksbc);
				}
				if (File.Exists(PathOnDiskfont))
				{
					found = true;
					Files.Add(PathOnDiskfont);
				}
			}
			if (!found)
				System.Console.WriteLine(Asset + " not found, skipping !!");
		}

		// we have all the files.
		Dictionary<string, string> UnrealPakResponseFile = new Dictionary<string, string>();
		String folder = Name + '\\';
		var new_folder = Path.Combine(CookedDir, folder);
		System.IO.Directory.CreateDirectory(new_folder);
		// create response file structure.
		foreach (string file in Files)
		{
			//Log.TraceInformation("PackageHas:" + file);
			string MountPoint = file.Remove(0, CookedDir.Length + 1);
			//UnrealPakResponseFile[file] ="../../../" + MountPoint;
			var new_file = Path.Combine(new_folder, MountPoint);
			System.IO.Directory.CreateDirectory(Path.GetDirectoryName(new_file));
			//Log.TraceInformation("PackageTransform:" + file + " to " + new_file);
			System.IO.File.Copy(file, new_file, true);
		}

		//RunUnrealPak(UnrealPakResponseFile, PakPath + "\\"+ Name + ".pak", null, "", true);
	}

}
