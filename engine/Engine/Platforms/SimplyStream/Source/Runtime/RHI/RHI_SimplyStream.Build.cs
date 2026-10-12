// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;
using EpicGames.Core;

public class RHI_SimplyStream : RHI
{
	public RHI_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.bCompileAgainstEngine)
		{
			if (Target.Type != TargetRules.TargetType.Server)   // Dedicated servers should skip loading everything but NullDrv
			{
				DirectoryReference ProjectDir = Target.ProjectFile == null ? (DirectoryReference)null : Target.ProjectFile.Directory;
				ConfigHierarchy EngineIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, ProjectDir, Target.Platform);
				String RenderMode = null;
				if (EngineIni != null)
				{
					EngineIni.GetString("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings", "RenderMode", out RenderMode);
				}
				if (RenderMode == "WebGPUSM5" || RenderMode == "WebGPUES31" || RenderMode == "WebGPUALL")
				{
					DynamicallyLoadedModuleNames.Add("WebGPURHI");
				}
				else
				{
					DynamicallyLoadedModuleNames.Add("OpenGLDrv");
				}
			}
		}
	}
}
