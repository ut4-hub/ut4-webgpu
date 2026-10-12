// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;


public class IoStoreHttpClient_SimplyStream : IoStoreHttpClient
{
	public IoStoreHttpClient_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		// wndr: 5.8 removed the `bSupportsOpenSSL` virtual from the base. SimplyStream/wasm has no
		// OpenSSL, so drop the dependency + define the base adds unconditionally (preserves intent).
		PrivateDependencyModuleNames.Remove("OpenSSL");
		PrivateDefinitions.Remove("IAS_HTTP_HAS_OPENSSL=1");
		PrivateDefinitions.Add("IAS_HTTP_HAS_OPENSSL=0");
	}
}
