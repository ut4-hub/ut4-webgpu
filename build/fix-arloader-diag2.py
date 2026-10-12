#!/usr/bin/env python3
# wndr AR-fix-2e: step-by-step diag inside OpenReadStateFileArchive to see which
# step fails on wasm: OpenRead null? Size<=0? Read false?
import io, sys

F = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/AssetRegistry/Private/ARLoader.cpp"
s = io.open(F, encoding="utf-8").read()

old = """#if PLATFORM_WASM
	// wndr AR-fix-2d: absolute MEMFS paths must go through raw-fopen. A wrapped top
	// file (CachedReadFile/IAS PathPak) can open-but-misread -> FailedToLoad.
	TUniquePtr<IFileHandle> FileHandle(IPlatformFile::GetPlatformPhysical().OpenRead(*FilePathSZ));
#else"""

new = """#if PLATFORM_WASM
	// wndr AR-fix-2d: absolute MEMFS paths must go through raw-fopen. A wrapped top
	// file (CachedReadFile/IAS PathPak) can open-but-misread -> FailedToLoad.
	IFileHandle* RawHandle = IPlatformFile::GetPlatformPhysical().OpenRead(*FilePathSZ);
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[wndr AR-fix-2e] OpenRead('%ls') handle=%s\\n"),
		FilePathSZ.GetData(), RawHandle ? TEXT("VALID") : TEXT("NULL"));
	TUniquePtr<IFileHandle> FileHandle(RawHandle);
#else"""

if old not in s:
    print("anchor not found", file=sys.stderr)
    sys.exit(1)

s = s.replace(old, new, 1)

old2 = """	if (FileSize <= 0)
	{
		return TUniquePtr<FArchive>();
	}
	if (BufferSize == 0 || FileSize <= static_cast<int64>(BufferSize))"""
new2 = """	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[wndr AR-fix-2e] handle Size=%lld\\n"), (long long)FileSize);
	if (FileSize <= 0)
	{
		return TUniquePtr<FArchive>();
	}
	if (BufferSize == 0 || FileSize <= static_cast<int64>(BufferSize))"""

if old2 not in s:
    print("anchor2 not found", file=sys.stderr)
    sys.exit(1)

s = s.replace(old2, new2, 1)

old3 = """		if (!FileHandle->Read(Data, FileSize))
		{
			return TUniquePtr<FArchive>();
		}"""
new3 = """		const bool bReadOk = FileHandle->Read(Data, FileSize);
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[wndr AR-fix-2e] Read(%lld) ok=%d\\n"), (long long)FileSize, (int)bReadOk);
		if (!bReadOk)
		{
			return TUniquePtr<FArchive>();
		}"""

if old3 not in s:
    print("anchor3 not found", file=sys.stderr)
    sys.exit(1)

s = s.replace(old3, new3, 1)

io.open(F, "w", encoding="utf-8").write(s)
print("patched AR-fix-2e diagnostics")
