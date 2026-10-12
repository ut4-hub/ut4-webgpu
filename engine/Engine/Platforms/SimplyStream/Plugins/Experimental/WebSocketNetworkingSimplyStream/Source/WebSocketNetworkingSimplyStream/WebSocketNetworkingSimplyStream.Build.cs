// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

// SimplyStream-owned copy of the engine WebSocketNetworking module. Self-contained (does NOT subclass
// the engine rules) so it carries zero engine deltas and is built ONLY for SimplyStream (the .uplugin
// PlatformAllowList gates it). The emscripten/browser path is hard-selected via USE_LIBWEBSOCKET=0, so
// none of the libwebsockets-only deps (libWebSockets/OpenSSL/zlib) are needed — the client uses the
// emscripten BSD-socket path and the dedicated server bridges to the Node 'ws' server.
public class WebSocketNetworkingSimplyStream : ModuleRules
{
	public WebSocketNetworkingSimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		// Build this module PER-TARGET (client + server), not into the precompiled-engine payload. It is
		// small platform net code that we iterate on, and keeping it out of the -AllModules/-Precompile
		// producer means a plain `strata cook` picks up changes with no engine-release bump, and a
		// -UsePrecompiled client cook builds it locally instead of expecting it in the precompiled manifest.
		PrecompileForTargets = PrecompileTargetsType.None;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"NetCore",
				"EngineSettings",
				"ImageCore",
				"Sockets",
				"PacketHandler"
			}
		);

		PublicDefinitions.Add("USE_LIBWEBSOCKET=0");
	}
}
