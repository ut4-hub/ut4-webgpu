#!/usr/bin/env python3
# SKINWGSL-dump-1: print the GPUSkin VS WGSL lines declaring @location inputs and
# the vertex-input struct fields, to map shader locations to UE attribute slots.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
DR = os.path.join(ROOT, 'DawnDynamicRHI.cpp')
s = open(DR).read()
if 'SKINWGSL-dump-1' in s:
    print('already patched'); sys.exit(0)
bak = DR + '.wndrbak_skinwgsl'
if not os.path.exists(bak): shutil.copy2(DR, bak)
old = '''			printf("DAWNBG GPUSKINLAYOUT #%u slots=%u mask=0x%x\\n", _gsk, NumStreamSlots, VertexStreamMask);
'''
new = '''			printf("DAWNBG GPUSKINLAYOUT #%u slots=%u mask=0x%x\\n", _gsk, NumStreamSlots, VertexStreamMask);
			if (_gsk <= 2) // wndr SKINWGSL-dump-1
			{
				const FString& Src = VS->WGSLSource; int32 St = 0, Nl = 0;
				while (St < Src.Len() && Nl < 40)
				{
					int32 En = Src.Find(TEXT("\\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, St); if (En < 0) { En = Src.Len(); }
					const FString Ln = Src.Mid(St, En - St);
					if (Ln.Contains(TEXT("@location")) || Ln.Contains(TEXT("in_var_ATTRIBUTE")) || Ln.Contains(TEXT("ATTRIBUTE"))) { printf("WNDR SKINWGSL #%u %s\\n", _gsk, TCHAR_TO_UTF8(*Ln.TrimStartAndEnd().Left(200))); ++Nl; }
					St = En + 1;
				}
				fflush(stdout);
			}
'''
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
open(DR, 'w').write(s)
print('patched', DR)
