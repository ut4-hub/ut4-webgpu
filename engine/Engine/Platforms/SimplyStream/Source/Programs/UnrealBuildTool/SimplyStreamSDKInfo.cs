// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using System.Text;
using UnrealBuildTool;
using System.Diagnostics;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using EpicGames.Core;
using UnrealBuildBase;

#nullable disable


namespace UnrealBuildTool {
	/// <summary>
	///
	/// </summary>
	public class SimplyStreamSDKInfo {
		static string SDKVersion = "5.0.7";
		static string _DebugUUID = Guid.NewGuid().ToString();

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		static public string DebugUUID
		{
			get
			{
				return _DebugUUID;
			}
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		static public DirectoryReference EMSDK_ROOT
		{
			get
			{
				// Use the in-tree emsdk submodule on all platforms. Mac used to point at
				// /Applications/emsdk, but that picked up an unpatched upstream install
				// (missing our WebGPU/wasm64/ASAN patches) and the wrong clang version.
				return DirectoryReference.Combine(Unreal.EngineDirectory, "Platforms", "SimplyStream", "emsdk");
			}
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		static public string EMSCRIPTEN_ROOT
		{
			get
			{
				return Path.Combine(EMSDK_ROOT.ToString(), "upstream", "emscripten");
			}
		}


		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		static public FileReference emcc
		{
			get
			{
				return FileReference.Combine(EMSDK_ROOT, "upstream", "emscripten", "emcc.py");
			}
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		static public FileReference emcc2
		{
			get
			{
				return FileReference.Combine(EMSDK_ROOT, "upstream", "emscripten", "emcc2.py");
			}
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		static public FileReference python
		{
			get
			{
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					return FileReference.Combine(EMSDK_ROOT, "python", "3.9.2_64bit", "bin", "python3.9");
				}
				return FileReference.Combine(EMSDK_ROOT, "python", "3.9.2-nuget_64bit", "python.exe");
			}
		}


		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static string SimplyStreamIntermediatory
		{
			get
			{
				string SimplyStreamIntermediatoryPath = FileReference.Combine(Unreal.EngineDirectory, "Intermediate", "Build", "SimplyStream").FullName;
				if (!Directory.Exists(SimplyStreamIntermediatoryPath))
				{
					Directory.CreateDirectory(SimplyStreamIntermediatoryPath);
				}
				return SimplyStreamIntermediatoryPath;
			}
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static string PLATFORM_USER_HOME
		{
			get
			{
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
				{
					return "USERPROFILE";
				}
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					return "HOME";
				}
				return "error_unknown_platform";
			}
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static string EmscriptenVersion()
		{
			return SDKVersion;
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static string EmscriptenPackager()
		{
			return Path.Combine(EMSCRIPTEN_ROOT, "tools", "file_packager.py");
		}

		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static string EmscriptenCompiler()
		{
			return "\"" + Path.Combine(EMSCRIPTEN_ROOT, "emcc") + "\"";
		}


		/// <summary>
		///
		/// </summary>
		/// <returns></returns>
		public static bool IsSDKInstalled()
		{
			return true;
		}

	}
}
