#!/usr/bin/env python3
# wndr AR-fix-3: on PLATFORM_WASM, read premade AssetRegistry.bin via raw POSIX
# fopen/fread straight into FLargeMemoryReader -- no IPlatformFile layer at all.
import sys, shutil
P = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/AssetRegistry/Private/ARLoader.cpp"
s = open(P).read()
if "AR-fix-3" in s:
    print("already applied"); sys.exit(0)
shutil.copy(P, P + ".wndrbak_arfix3")
anchor = "\tTStringBuilder<512> FilePathSZ(InPlace, FilePath);\n#if PLATFORM_WASM\n"
assert s.count(anchor) == 1, "anchor not found"
raw = r'''	TStringBuilder<512> FilePathSZ(InPlace, FilePath);
#if PLATFORM_WASM
	{
		// wndr AR-fix-3: bypass IPlatformFile entirely (its OpenRead returns NULL on
		// MEMFS paths where raw stat() succeeds). Read whole file with stdio.
		FTCHARToUTF8 Utf8Path(*FilePathSZ);
		errno = 0;
		FILE* Fp = fopen(Utf8Path.Get(), "rb");
		if (!Fp)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[wndr AR-fix-3] fopen('%ls') FAILED errno=%d\n"),
				*FilePathSZ, (int)errno);
		}
		else
		{
			fseeko(Fp, 0, SEEK_END);
			const int64 RawSize = (int64)ftello(Fp);
			fseeko(Fp, 0, SEEK_SET);
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[wndr AR-fix-3] fopen('%ls') ok size=%lld\n"),
				*FilePathSZ, (long long)RawSize);
			if (RawSize > 0)
			{
				uint8* RawData = static_cast<uint8*>(FMemory::Malloc(RawSize));
				int64 Got = 0;
				while (Got < RawSize)
				{
					const size_t N = fread(RawData + Got, 1, (size_t)(RawSize - Got), Fp);
					if (N == 0) { break; }
					Got += (int64)N;
				}
				fclose(Fp);
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[wndr AR-fix-3] fread got=%lld/%lld errno=%d\n"),
					(long long)Got, (long long)RawSize, (int)errno);
				if (Got == RawSize)
				{
					return TUniquePtr<FArchive>(new FLargeMemoryReader(RawData, RawSize, ELargeMemoryReaderFlags::TakeOwnership));
				}
				FMemory::Free(RawData);
			}
			else
			{
				fclose(Fp);
			}
		}
	}
'''
s = s.replace(anchor, raw, 1)
# includes
inc = "#include <stdio.h>\n#include <errno.h>\n"
first = s.find("#include")
s = s[:first] + "#if PLATFORM_WASM\n" + inc + "#endif\n" + s[first:]
open(P, "w").write(s)
print("applied AR-fix-3")
