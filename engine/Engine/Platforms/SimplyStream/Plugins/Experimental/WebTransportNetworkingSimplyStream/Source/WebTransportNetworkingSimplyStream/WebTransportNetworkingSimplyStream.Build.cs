// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

// SimplyStream WebTransport net driver. Self-contained (no engine deltas), built ONLY for SimplyStream (the
// .uplugin PlatformAllowList gates it). Client bridges to the browser global WebTransport via the
// webtransport.js shim (added to the link below); dedicated server bridges to a Node @fails-components
// WebTransport (HTTP/3) server. Uses datagrams (UDP-like) — the natural fit for UE's net layer.
public class WebTransportNetworkingSimplyStream : ModuleRules
{
	public WebTransportNetworkingSimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		// Build PER-TARGET (client + server), not into the precompiled-engine payload — so a plain
		// `strata cook` picks up changes with no engine-release bump and a -UsePrecompiled client cook
		// builds it locally.
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

		// Add the client WebTransport JS shim to the link. A .js in the link Libraries is auto-appended as
		// `--js-library` by SimplyStreamToolChain.cs, and registered as a link prerequisite so a shim-only
		// edit re-links. (Emscripten has no native WebTransport C API — the client must bridge through JS.)
		string ShimPath = Path.Combine(ModuleDirectory, "Private", "webtransport.js");
		PublicAdditionalLibraries.Add(ShimPath);
	}
}
