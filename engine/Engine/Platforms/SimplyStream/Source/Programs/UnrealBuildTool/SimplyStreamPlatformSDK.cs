// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	partial class SimplyStreamPlatformSDK : UEBuildPlatformSDK
	{
		public SimplyStreamPlatformSDK(ILogger Logger)
			: base(Logger)
		{
		}

		protected override string? GetInstalledSDKVersion()
		{
			// The SDK is the in-tree emsdk (Engine/Platforms/SimplyStream/emsdk). Only report a version
			// when it is actually materialized so a missing SDK fails platform validation with a clear
			// SDK message instead of a later emcc-not-found compile error (on dev machines the emsdk dir
			// can be a symlink into another checkout; in a distributed package it must be real).
			if (FileReference.Exists(SimplyStreamSDKInfo.emcc))
			{
				return "1.0";
			}
			return null;
		}

		public override bool TryConvertVersionToInt(string? StringValue, out ulong OutValue, string? Hint)
		{
			OutValue = 1;
			return true;
		}


		protected override bool PlatformSupportsAutoSDKs()
		{
			return true;
		}

		// prefer auto sdk on android as correct 'manual' sdk detection isn't great at the moment.
		protected override bool PreferAutoSDK()
		{
			return true;
		}

		/// <summary>
		/// checks if the sdk is installed or has been synced
		/// </summary>
		/// <returns></returns>
		protected virtual bool HasAnySDK()
		{
			return true;
		}

		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			return FileReference.Exists(SimplyStreamSDKInfo.emcc) ? SDKStatus.Valid : SDKStatus.Invalid;
		}
	}
}
