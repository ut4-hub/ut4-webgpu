#!/usr/bin/env python3
# SLOWUB-trace-1: stage 21 (TryUBTable after the lock) eats ~1s per bind during
# stalls while one worker runs memmove/memset flat out. Print the sizes involved
# whenever one TryUBTable call takes > 50 ms.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')
s = open(CC).read()
if 'SLOWUB-trace-1' in s:
    print('already patched'); sys.exit(0)
bak = CC + '.wndrbak_slowub'
if not os.path.exists(bak): shutil.copy2(CC, bak)
old = '''					WNDR_STAGE(21);
'''
new = '''					WNDR_STAGE(21);
#if PLATFORM_WASM
					struct FWndrSlowUB { const FDawnUniformBuffer* D; const FDawnShaderBinding& Bn; int32 NP; int32 NS; double T0;
						~FWndrSlowUB() { const double Ms = emscripten_get_now() - T0; static uint32 _n = 0;
							if (Ms > 50.0 && _n++ < 60) { printf("WNDR SLOWUB ms=%.1f ub=%p snLen=%d resTbl=%d resTblMax=%d bNameLen=%d pendUBs=%d staticUBs=%d layoutName=%s\\n",
								Ms, (const void*)D, D->ShaderName.Len(), D->ResTable.Num(), D->ResTable.Max(), Bn.Name.Len(), NP, NS, TCHAR_TO_UTF8(*D->GetLayout().GetDebugName())); fflush(stdout); } } }
						_wndrSlowUB{ DUB, B, PendingUBRefs.Num(), StaticUBsByName.Num(), emscripten_get_now() }; // wndr SLOWUB-trace-1
#endif
'''
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
open(CC, 'w').write(s)
print('patched', CC)
