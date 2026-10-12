#!/usr/bin/env python3
# LMPAIR-1 (patchy/blocky world lighting): LightmapResourceCluster_* textures were taken from
# whichever pending UB matched first (incl. STICKYUB leftovers from earlier draws) before the
# PLB->cluster association. The shader's lightmap UV scale/bias comes from the PLB bound to
# THIS draw, so the texture must be that PLB's cluster: resolve it first. Disable at runtime
# with Module._wndr_dbg(4) for A/B. Also counts pairing hits for the log.
CC='/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private/DawnCommandContext.cpp'
s=open(CC).read()
if 'LMPAIR-1' in s: print('already'); raise SystemExit
o='''					WNDR_STAGE(24);
					return (E.textureView != nullptr) || (E.sampler != nullptr) || (E.buffer != nullptr);
				};
'''
n=o+'''				if ((GWndrDbgBits & 4) == 0 && B.Name.StartsWith(TEXT("LightmapResourceCluster_"), ESearchCase::CaseSensitive)) // wndr LMPAIR-1
				{
					extern FDawnUniformBuffer* DawnUBRegistryFind(void* WgpuBuf);
					FDawnUniformBuffer* PLBUB = nullptr;
					for (const TPair<uint32, TArray<FDawnShaderBinding>>& G2 : LayoutByGroup)
					{
						for (const FDawnShaderBinding& B2 : G2.Value)
						{
							if (B2.Kind != EDawnShaderBindingKind::UniformBuffer || !B2.Name.Equals(TEXT("PrecomputedLightingBuffer"), ESearchCase::CaseSensitive)) { continue; }
							const TMap<uint32, WGPUBindGroupEntry>* P2 = PendingBindEntries.Find(G2.Key);
							const WGPUBindGroupEntry* PE2 = P2 ? P2->Find(B2.Binding) : nullptr;
							if (PE2 && PE2->buffer) { PLBUB = DawnUBRegistryFind((void*)PE2->buffer); }
							if (PLBUB) { break; }
						}
						if (PLBUB) { break; }
					}
					static uint32 _lpHit = 0, _lpMiss = 0;
					FRHIUniformBuffer* Cl = PLBUB ? WndrFindLMCluster(PLBUB) : nullptr;
					if (Cl && TryUBTable(Cl)) { bResFilled = true; ++_lpHit; } else { ++_lpMiss; }
					if (((_lpHit + _lpMiss) % 20000) == 1) { printf("WNDR LMPAIR hit=%u miss=%u (plb=%d)\\n", _lpHit, _lpMiss, PLBUB ? 1 : 0); fflush(stdout); }
				}
'''
assert s.count(o)==1; s=s.replace(o,n)
o2='''				// Newest first: with several materials cycling on one PSO, this
				// draw's own UBs are the most recently recorded.
				if (!bResFilled)'''
assert s.count(o2)==1
open(CC,'w').write(s); print('patched')
