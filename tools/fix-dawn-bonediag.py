#!/usr/bin/env python3
# BONEDIAG-1: count BoneMatrices bindings resolved from the GPUSkinVFBase table vs
# falling to the dummy; dump that UB's captured table names on the first miss.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')
s = open(CC).read()
if 'BONEDIAG-1' in s:
    print('already patched'); sys.exit(0)
bak = CC + '.wndrbak_bonediag'
if not os.path.exists(bak): shutil.copy2(CC, bak)
old = '''				WNDR_STAGE(28);
				if (bResFilled)
'''
new = '''				WNDR_STAGE(28);
				if (B.Name.Contains(TEXT("BoneMatrices"))) // wndr BONEDIAG-1
				{
					static uint32 _bOk = 0, _bMiss = 0, _bDump = 0;
					if (bResFilled) { ++_bOk; } else { ++_bMiss; }
					if (((_bOk + _bMiss) % 500) == 1) { printf("WNDR BONE name=%s resolved=%u dummy=%u pendUBs=%d\\n", TCHAR_TO_UTF8(*B.Name), _bOk, _bMiss, PendingUBRefs.Num()); fflush(stdout); }
					if (!bResFilled && _bDump++ < 3)
					{
						for (const TRefCountPtr<FRHIUniformBuffer>& R : PendingUBRefs)
						{
							const FDawnUniformBuffer* D = static_cast<const FDawnUniformBuffer*>(R.GetReference());
							printf("WNDR BONE   ub sn=%s layout=%s tbl=%d:", TCHAR_TO_UTF8(*D->ShaderName), TCHAR_TO_UTF8(*D->GetLayout().GetDebugName()), D->ResTable.Num());
							for (const FDawnUniformBuffer::FTableEntry& TE : D->ResTable) { printf(" {%s t=%d res=%d}", TCHAR_TO_UTF8(*TE.WgslName), (int)TE.MemberType, TE.Resource ? 1 : 0); }
							printf("\\n");
						}
						fflush(stdout);
					}
				}
				if (bResFilled)
'''
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
open(CC, 'w').write(s)
print('patched', CC)
