#!/usr/bin/env python3
# wndr AR-fix-2d: on wasm, prefer the PHYSICAL platform file for the premade AR
# open (absolute MEMFS paths). A wrapped top file (IAS PathPak / CachedReadFile)
# can open-but-misread -> LoadResult==2. Physical raw-fopen is proven to work.
import io, sys

F = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/AssetRegistry/Private/ARLoader.cpp"
s = io.open(F, encoding="utf-8").read()

if "AR-fix-2d" in s:
    print("already patched")
    sys.exit(0)

old = """	TStringBuilder<512> FilePathSZ(InPlace, FilePath);
	TUniquePtr<IFileHandle> FileHandle(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*FilePathSZ));
	if (!FileHandle) // wndr AR-fix-2b: wrapped top file (IAS PathPak) may fail absolute MEMFS paths — retry physical
	{
		FileHandle = TUniquePtr<IFileHandle>(IPlatformFile::GetPlatformPhysical().OpenRead(*FilePathSZ));
	}"""

new = """	TStringBuilder<512> FilePathSZ(InPlace, FilePath);
#if PLATFORM_WASM
	// wndr AR-fix-2d: absolute MEMFS paths must go through raw-fopen. A wrapped top
	// file (CachedReadFile/IAS PathPak) can open-but-misread -> FailedToLoad.
	TUniquePtr<IFileHandle> FileHandle(IPlatformFile::GetPlatformPhysical().OpenRead(*FilePathSZ));
#else
	TStringBuilder<512> FilePathSZB(InPlace, FilePath);
	TUniquePtr<IFileHandle> FileHandle(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*FilePathSZB));
#endif"""

if old not in s:
    print("anchor not found", file=sys.stderr)
    sys.exit(1)

s = s.replace(old, new, 1)
io.open(F, "w", encoding="utf-8").write(s)
print("patched AR-fix-2d: physical-first open on wasm")
