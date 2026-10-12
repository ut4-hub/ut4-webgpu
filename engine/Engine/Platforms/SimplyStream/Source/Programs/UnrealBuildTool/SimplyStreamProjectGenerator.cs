// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
#nullable disable

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	class SimplyStreamProjectGenerator : PlatformProjectGenerator
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Arguments">Command line arguments passed to the project generator</param>
		/// <param name="Logger"></param>
		public SimplyStreamProjectGenerator(CommandLineArguments Arguments, ILogger Logger)
			: base(Logger)
		{
		}

		/// <summary>
		/// Enumerate all the platforms that this generator supports
		/// </summary>
		public override IEnumerable<UnrealTargetPlatform> GetPlatforms()
		{
			yield return UnrealTargetPlatform.SimplyStream;
		}

		/// <inheritdoc/>
		public override bool HasVisualStudioSupport(VSSettings InVSSettings)
		{
			// SimplyStream is not supported in VisualStudio
			return false;
		}
	}
}
