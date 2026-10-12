#!/usr/bin/env python3
# wndr AR-fix-2 (2026-10-11): the premade AssetRegistry.bin IS in MEMFS
# (/UnrealTournament/AssetRegistry.bin, verified 7981528 bytes via JS FS.stat)
# but FPreloader::TrySetPath's IFileManager::FileExists("../../../UnrealTournament/AssetRegistry.bin")
# returns 0 on the game-thread pthread (path-normalization/BaseDir semantics), so the
# premade AR never loads -> GUID lightmap/shadowmap refs unresolved -> DM-Chill renders dark.
#
# Fix: in TrySetPath (and the Pak variant stays), ALSO probe plain absolute MEMFS
# paths via the PHYSICAL platform file. Absolute /-rooted paths bypass the
# BaseDir/relative-collapse entirely and stat() straight into MEMFS (uproject
# probe proves raw stat works on this thread).
import io, sys

F = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/AssetRegistry/Private/ARLoader.cpp"
s = io.open(F, encoding="utf-8").read()

if "wndr AR-fix-2" in s:
    print("already patched")
    sys.exit(0)

old = """bool FPreloader::TrySetPath()
{
	for (FString& LocalPath : GetPriorityPaths())
	{
		if (IFileManager::Get().FileExists(*LocalPath))
		{
			ARPath = MoveTemp(LocalPath);
			return true;
		}
	}
	return false;
}"""

new = """bool FPreloader::TrySetPath()
{
	// wndr AR-fix-2 (2026-10-11): on wasm the cooked tree mounts at the MEMFS root
	// (/UnrealTournament/...) while ProjectDir()-relative probes
	// ("../../../UnrealTournament/AssetRegistry.bin") fail inside
	// FUnixPlatformFile::FileExists (BaseDir+leading-.. collapse) even though the
	// bytes ARE in MEMFS. Probe the absolute MEMFS paths through the PHYSICAL
	// platform file first; fall back to the stock relative probes.
#if PLATFORM_WASM
	{
		IPlatformFile& Physical = IPlatformFile::GetPlatformPhysical();
		const TCHAR*const WasmPaths[] = {
			TEXT("/UnrealTournament/AssetRegistry.bin"),
			TEXT("/UnrealTournament/EditorClientAssetRegistry.bin"),
		};
		for (const TCHAR*const WasmPath : WasmPaths)
		{
			if (Physical.FileExists(WasmPath))
			{
				UE_LOGF(LogAssetRegistry, Display, "wndr AR-fix-2: premade AR found via MEMFS path '%ls'", WasmPath);
				ARPath = WasmPath;
				return true;
			}
		}
	}
#endif
	for (FString& LocalPath : GetPriorityPaths())
	{
		if (IFileManager::Get().FileExists(*LocalPath))
		{
			ARPath = MoveTemp(LocalPath);
			return true;
		}
	}
	return false;
}"""

if old not in s:
    print("anchor not found", file=sys.stderr)
    sys.exit(1)

s = s.replace(old, new, 1)
io.open(F, "w", encoding="utf-8").write(s)
print("patched ARLoader.cpp TrySetPath with MEMFS absolute probes")
