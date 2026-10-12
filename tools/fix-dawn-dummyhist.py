#!/usr/bin/env python3
# DUMMYHIST-1 (diagnostic, flicker hunt): histogram of texture bindings that fall to
# the dummy, printed every 3000 dummy fills; LightmapResourceCluster misses also
# record whether a PrecomputedLighting UB was pending and whether the PLB->cluster
# association existed.
import sys, shutil, os
P = '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private/DawnCommandContext.cpp'
s = open(P).read()
if 'DUMMYHIST-1' in s:
    print('already patched'); sys.exit(0)
bak = P + '.wndrbak_dummyhist'
if not os.path.exists(bak): shutil.copy2(P, bak)
old = '''				if (bFilled)
				{
					static uint32 _df = 0;
					WndrLMCount(B.Name, 3);'''
new = '''				if (bFilled && B.Kind != EDawnShaderBindingKind::Sampler && B.Kind != EDawnShaderBindingKind::UniformBuffer) // wndr DUMMYHIST-1
				{
					static FCriticalSection HL; static TMap<FString, uint32> Hist; static uint32 HN = 0;
					FString Key = B.Name;
					if (B.Name.StartsWith(TEXT("LightmapResourceCluster_"), ESearchCase::CaseSensitive))
					{
						int32 NPLB = 0, NAssoc = 0;
						for (const TRefCountPtr<FRHIUniformBuffer>& R : PendingUBRefs)
						{
							FDawnUniformBuffer* PD = static_cast<FDawnUniformBuffer*>(R.GetReference());
							if (PD && PD->ShaderName == TEXT("PrecomputedLighting")) { ++NPLB; if (WndrFindLMCluster(PD)) { ++NAssoc; } }
						}
						Key += FString::Printf(TEXT(" plb=%d assoc=%d"), NPLB, NAssoc);
					}
					FScopeLock L(&HL);
					++Hist.FindOrAdd(Key);
					if ((++HN % 3000) == 0)
					{
						Hist.ValueSort([](uint32 A, uint32 B2) { return A > B2; });
						printf("WNDR DUMMYHIST n=%u:", HN);
						int32 k = 0;
						for (const TPair<FString, uint32>& P : Hist) { if (k++ >= 10) break; printf(" [%s]=%u", TCHAR_TO_UTF8(*P.Key), P.Value); }
						printf("\\n"); fflush(stdout);
					}
				}
				if (bFilled)
				{
					static uint32 _df = 0;
					WndrLMCount(B.Name, 3);'''
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
open(P, 'w').write(s)
print('patched OK')
