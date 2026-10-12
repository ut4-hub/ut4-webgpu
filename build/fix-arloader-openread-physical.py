#!/usr/bin/env python3
# wndr AR-fix-2b: OpenReadStateFileArchive should survive a wrapped top platform
# file (IAS PathPak). If the wrapped OpenRead fails, retry with the PHYSICAL file.
import io, sys

F = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/AssetRegistry/Private/ARLoader.cpp"
s = io.open(F, encoding="utf-8").read()

if "AR-fix-2b" in s:
    print("already patched")
    sys.exit(0)

old = """TUniquePtr<FArchive> OpenReadStateFileArchive(FStringView FilePath, uint32 BufferSize)
{
	TStringBuilder<512> FilePathSZ(InPlace, FilePath);
	TUniquePtr<IFileHandle> FileHandle(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*FilePathSZ));
	if (!FileHandle)
	{
		return TUniquePtr<FArchive>();
	}"""

new = """TUniquePtr<FArchive> OpenReadStateFileArchive(FStringView FilePath, uint32 BufferSize)
{
	TStringBuilder<512> FilePathSZ(InPlace, FilePath);
	TUniquePtr<IFileHandle> FileHandle(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*FilePathSZ));
	if (!FileHandle) // wndr AR-fix-2b: wrapped top file (IAS PathPak) may fail absolute MEMFS paths — retry physical
	{
		FileHandle = IPlatformFile::GetPlatformPhysical().OpenRead(*FilePathSZ);
	}
	if (!FileHandle)
	{
		return TUniquePtr<FArchive>();
	}"""

if old not in s:
    print("anchor not found", file=sys.stderr)
    sys.exit(1)

s = s.replace(old, new, 1)
io.open(F, "w", encoding="utf-8").write(s)
print("patched OpenReadStateFileArchive physical fallback")
