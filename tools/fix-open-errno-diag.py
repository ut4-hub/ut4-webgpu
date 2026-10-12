#!/usr/bin/env python3
# wndr AR-fix-2f: log errno on the direct-open failure inside PlatformInitialOpenFile
# (wasm branch) to determine why open() fails while stat() succeeds.
import io, sys

F = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/Core/Private/Unix/UnixPlatformFile.cpp"
s = io.open(F, encoding="utf-8").read()

if "AR-fix-2f" in s:
    print("already patched")
    sys.exit(0)

old = """	{
		int32 DirectHandle = open(TCHAR_TO_UTF8(Filename), O_RDONLY | O_CLOEXEC);
		if (FString(Filename).Contains(TEXT(".utoc")) || FString(Filename).Contains(TEXT(".ucas"))) { UE_LOG(LogUnixPlatformFile, Warning, TEXT("[wndr-diag] PlatformInitialOpenFile %s directopen=%d"), Filename, DirectHandle); }
		if (DirectHandle != -1)
		{
			return new FFileHandleUnix(DirectHandle, Filename, false);
		}
	}"""

new = """	{
		int32 DirectHandle = open(TCHAR_TO_UTF8(Filename), O_RDONLY | O_CLOEXEC);
		if (DirectHandle == -1)
		{
			// wndr AR-fix-2f: open failed while stat may succeed — log errno + path.
			UE_LOG(LogUnixPlatformFile, Warning, TEXT("[wndr AR-fix-2f] direct open('%s') FAILED errno=%d"), Filename, errno);
		}
		if (FString(Filename).Contains(TEXT(".utoc")) || FString(Filename).Contains(TEXT(".ucas"))) { UE_LOG(LogUnixPlatformFile, Warning, TEXT("[wndr-diag] PlatformInitialOpenFile %s directopen=%d"), Filename, DirectHandle); }
		if (DirectHandle != -1)
		{
			return new FFileHandleUnix(DirectHandle, Filename, false);
		}
	}"""

if old not in s:
    print("anchor not found", file=sys.stderr)
    sys.exit(1)

s = s.replace(old, new, 1)

# ensure errno.h include
if "#include <errno.h>" not in s:
    s = s.replace("#include <sys/stat.h>", "#include <sys/stat.h>\n#include <errno.h>", 1)

io.open(F, "w", encoding="utf-8").write(s)
print("patched AR-fix-2f errno diag")
