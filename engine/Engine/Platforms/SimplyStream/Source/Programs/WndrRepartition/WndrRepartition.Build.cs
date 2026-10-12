// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WndrRepartition : ModuleRules
{
	public WndrRepartition(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");

		PrivateDependencyModuleNames.Add("Core");              // FIoStoreReader, FIoChunkEncoding, FIoChunkId, FIoHash
		PrivateDependencyModuleNames.Add("IoStoreOnDemandCore"); // FOnDemandTocWriter (Internal/IO/Serialization/OnDemandContainerToc.h)
		PrivateDependencyModuleNames.Add("Json");
		PrivateDependencyModuleNames.Add("Projects");

		// These headers are Public but pull in Internal headers (IoChunkEncoding.h -> Core/Internal/IO/IoOffsetLength.h;
		// the on-demand TOC writer lives in OnDemandCore/Internal). A Program module isn't granted a dependency's Internal
		// include paths automatically, so add them explicitly — same as Engine/Source/Programs/IoStoreDependencyViewer.
		PrivateIncludePaths.Add("Runtime/Core/Internal");
		PrivateIncludePaths.Add("Runtime/Experimental/IoStore/OnDemandCore/Internal");
	}
}
