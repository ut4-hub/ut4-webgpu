// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;

namespace UnrealBuildTool
{
	partial class SimplyStreamPlatformSDK : UEBuildPlatformSDK
	{
		public override string GetAutoSDKDirectoryForMainVersion()
		{
			return "-26";
		}

		protected override void GetValidVersionRange(out string MinVersion, out string MaxVersion)
		{
			MinVersion = "1.00";
			MaxVersion = "1.00";
		}

		protected override void GetValidSoftwareVersionRange(out string? MinVersion, out string? MaxVersion)
		{
			MinVersion = MaxVersion = null;
		}

		public override string GetPlatformSpecificVersion(string VersionType)
		{
			switch (VersionType.ToLower())
			{
				case "emcc": return "5.0.7";
				case "node": return "28.0.3";
				case "python": return "3.10.2.4988404";
			}

			return "";
		}
	}
}
