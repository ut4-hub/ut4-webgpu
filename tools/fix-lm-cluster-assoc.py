#!/usr/bin/env python3
# wndr LM-fix-2: LightmapResourceCluster UB is never delivered to DawnRHI (resource-only UB,
# likely absent from the cooked WebGPU parameter map) -> LightMapTexture always dummy.
# Renderer records PrecomputedLightingBuffer -> LightmapResourceCluster association;
# DawnRHI resolves LightmapResourceCluster_* bindings through the delivered PLB.
import sys, shutil
R = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/Renderer/Private/LightMapRendering.cpp"
D = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private/DawnCommandContext.cpp"

r = open(R).read()
if "LM-fix-2" not in r:
    shutil.copy(R, R + ".wndrbak_lmfix2")
    a = "void SetupLCIUniformBuffers(const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FLightCacheInterface* LCI, FRHIUniformBuffer*& PrecomputedLightingBuffer, FRHIUniformBuffer*& LightmapResourceClusterBuffer, FRHIUniformBuffer*& IndirectLightingCacheBuffer)\n{"
    assert r.count(a) == 1
    r = r.replace(a, '#if PLATFORM_WASM\nextern "C" void WndrDawnAssocLightmapCluster(void* PrecomputedLightingUB, void* ClusterUB); // DawnRHI (LM-fix-2)\n#endif\n' + a, 1)
    b = """	if (!LightmapResourceClusterBuffer)
	{
		LightmapResourceClusterBuffer = GDefaultLightmapResourceClusterUniformBuffer.GetUniformBufferRHI();
	}
"""
    assert r.count(b) == 1
    r = r.replace(b, b + """#if PLATFORM_WASM
	// wndr LM-fix-2: let DawnRHI find this draw's cluster through its (delivered) PLB.
	if (PrecomputedLightingBuffer && PrecomputedLightingBuffer != GEmptyPrecomputedLightingUniformBuffer.GetUniformBufferRHI())
	{
		WndrDawnAssocLightmapCluster(PrecomputedLightingBuffer, LightmapResourceClusterBuffer);
	}
#endif
""", 1)
    open(R, "w").write(r); print("renderer patched")

d = open(D).read()
if "LM-fix-2" not in d:
    shutil.copy(D, D + ".wndrbak_lmfix2")
    helper = r'''
// wndr LM-fix-2: PrecomputedLightingBuffer UB -> its LightmapResourceCluster UB.
static FCriticalSection GWndrLMAssocLock;
static TMap<void*, TRefCountPtr<FRHIUniformBuffer>> GWndrLMAssoc;
extern "C" void WndrDawnAssocLightmapCluster(void* PrecomputedLightingUB, void* ClusterUB)
{
	if (!PrecomputedLightingUB || !ClusterUB) { return; }
	FScopeLock Lock(&GWndrLMAssocLock);
	TRefCountPtr<FRHIUniformBuffer>& Slot = GWndrLMAssoc.FindOrAdd(PrecomputedLightingUB);
	if (Slot.GetReference() != (FRHIUniformBuffer*)ClusterUB) { Slot = (FRHIUniformBuffer*)ClusterUB; }
}
static FRHIUniformBuffer* WndrFindLMCluster(FRHIUniformBuffer* PLB)
{
	FScopeLock Lock(&GWndrLMAssocLock);
	const TRefCountPtr<FRHIUniformBuffer>* F = GWndrLMAssoc.Find((void*)PLB);
	return F ? F->GetReference() : nullptr;
}
'''
    anchor = "bool FDawnCommandContext::FlushPendingBindGroups()"
    assert d.count(anchor) == 1
    d = d.replace(anchor, helper + anchor, 1)
    old = """				if (!bResFilled)
				{
					for (const TPair<FString, FRHIUniformBuffer*>& P : StaticUBsByName)
					{
						if (TryUBTable(P.Value)) { bResFilled = true; break; }
					}
				}"""
    assert d.count(old) == 1, d.count(old)
    new = """				// wndr LM-fix-2: LightmapResourceCluster_* via this draw's delivered PLB.
				if (!bResFilled && B.Name.StartsWith(TEXT("LightmapResourceCluster_"), ESearchCase::CaseSensitive))
				{
					for (int32 UBIdx = PendingUBRefs.Num() - 1; UBIdx >= 0 && !bResFilled; --UBIdx)
					{
						FDawnUniformBuffer* PD = static_cast<FDawnUniformBuffer*>(PendingUBRefs[UBIdx].GetReference());
						if (PD && PD->ShaderName == TEXT("PrecomputedLighting"))
						{
							if (FRHIUniformBuffer* Cl = WndrFindLMCluster(PD)) { if (TryUBTable(Cl)) { bResFilled = true; } }
						}
					}
				}
""" + old
    d = d.replace(old, new, 1)
    open(D, "w").write(d); print("dawn patched")
