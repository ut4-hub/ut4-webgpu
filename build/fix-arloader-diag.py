#!/usr/bin/env python3
# wndr AR-fix-2c: add failure-side diagnostics to the MEMFS probe so we can see
# WHY the absolute probe misses, and add a raw-stat (POSIX) probe as a second try.
import io, sys

F = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/AssetRegistry/Private/ARLoader.cpp"
s = io.open(F, encoding="utf-8").read()

old = """#if PLATFORM_WASM
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
#endif"""

new = """#if PLATFORM_WASM
	{
		IPlatformFile& Physical = IPlatformFile::GetPlatformPhysical();
		const TCHAR*const WasmPaths[] = {
			TEXT("/UnrealTournament/AssetRegistry.bin"),
			TEXT("/UnrealTournament/EditorClientAssetRegistry.bin"),
		};
		for (const TCHAR*const WasmPath : WasmPaths)
		{
			const bool bViaIFile = Physical.FileExists(WasmPath);
			struct stat St; const int RawRc = stat(TCHAR_TO_UTF8(WasmPath), &St);
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[wndr AR-fix-2] probe '%ls' IFile=%d rawstat=%d size=%ld\\n"),
				WasmPath, (int)bViaIFile, RawRc, (long)(RawRc == 0 ? St.st_size : -1L));
			if (bViaIFile || RawRc == 0)
			{
				UE_LOGF(LogAssetRegistry, Display, "wndr AR-fix-2: premade AR found via MEMFS path '%ls' (IFile=%d raw=%d)", WasmPath, (int)bViaIFile, RawRc);
				ARPath = WasmPath;
				return true;
			}
		}
	}
#endif"""

if old not in s:
    print("anchor not found", file=sys.stderr)
    sys.exit(1)

s = s.replace(old, new, 1)

# make sure sys/stat.h is included
if "#include <sys/stat.h>" not in s:
    s = s.replace('#include "HAL/FileManager.h"', '#include "HAL/FileManager.h"\n#include <sys/stat.h>', 1)

io.open(F, "w", encoding="utf-8").write(s)
print("patched AR-fix-2c diagnostics")
