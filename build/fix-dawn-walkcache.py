#!/usr/bin/env python3
# WALKCACHE-1 (perf): FlushPendingBindGroups' resolve walk (~45% of the frame) is skipped
# when its inputs repeat. Key = PSO serial + pending entries + pending UBs (serial,version)
# + static UBs (serial,version) + lightmap-assoc version. Value = per-group BGCACHE-2 hashes.
# A hit re-binds the cached groups; if any is gone (a referenced handle died -> evicted)
# the full walk runs. Also UBNAMECACHE-1: derived UB shader name computed once per UB.
import sys, shutil, os
R = '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/'
H = R + 'Public/DawnResources.h'; DR = R + 'Private/DawnDynamicRHI.cpp'; CC = R + 'Private/DawnCommandContext.cpp'
def patch(path, reps):
    s = open(path).read()
    if 'WALKCACHE-1' in s: print('already patched', path); return
    bak = path + '.wndrbak_walkcache'
    if not os.path.exists(bak): shutil.copy2(path, bak)
    for old, new in reps:
        assert s.count(old) == 1, (path, old[:80], s.count(old))
        s = s.replace(old, new)
    open(path, 'w').write(s); print('patched', path)

patch(H, [
('	FString ShaderName; // WGSL var name of this UB ("Material", "GPUSkinVFBase", ...)\n',
 '	FString ShaderName; // WGSL var name of this UB ("Material", "GPUSkinVFBase", ...)\n'
 '	uint64 WndrSerial = 0;      // wndr WALKCACHE-1: unique id (pointer values get reused)\n'
 '	uint32 WndrResVersion = 0;  // wndr WALKCACHE-1: bumped on every resource-table capture\n'
 '	FString WndrDerivedName, WndrDerivedNameBuf; bool bWndrDerivedNameSet = false; // wndr UBNAMECACHE-1\n'),
('	mutable bool bWndrLayoutBuilt = false;\n',
 '	mutable bool bWndrLayoutBuilt = false;\n	mutable uint64 WndrSerial = 0; // wndr WALKCACHE-1\n'),
])

patch(DR, [
('static void DawnCaptureUBResourceTable(FDawnUniformBuffer* UB, const void* Contents)\n{\n',
 'static void DawnCaptureUBResourceTable(FDawnUniformBuffer* UB, const void* Contents)\n{\n	if (UB) { ++UB->WndrResVersion; } // wndr WALKCACHE-1\n'),
])

patch(CC, [
# LM assoc version
('	if (Slot.GetReference() != (FRHIUniformBuffer*)ClusterUB) { Slot = (FRHIUniformBuffer*)ClusterUB; }',
 '	if (Slot.GetReference() != (FRHIUniformBuffer*)ClusterUB) { Slot = (FRHIUniformBuffer*)ClusterUB; extern uint32 GWndrLMAssocVersion; ++GWndrLMAssocVersion; } // wndr WALKCACHE-1'),
# BG cache get returns hash
('static WGPUBindGroup WndrBGCacheGet(WGPUDevice Dev, const WGPUBindGroupDescriptor& D, bool& bOwned)\n{\n	bOwned = false;',
 'static WGPUBindGroup WndrBGCacheGet(WGPUDevice Dev, const WGPUBindGroupDescriptor& D, bool& bOwned, uint64* OutHash = nullptr)\n{\n	bOwned = false;'),
('	const uint64 Hash = CityHash64((const char*)W.GetData(), W.Num() * sizeof(uint64));\n	FScopeLock L(&GWndrBGLock);',
 '	const uint64 Hash = CityHash64((const char*)W.GetData(), W.Num() * sizeof(uint64));\n	if (OutHash) { *OutHash = Hash; }\n	FScopeLock L(&GWndrBGLock);'),
# walk cache helpers, placed right before FlushPendingBindGroups
('bool FDawnCommandContext::FlushPendingBindGroups()\n{\n',
 '''// wndr WALKCACHE-1
uint32 GWndrLMAssocVersion = 0;
static std::atomic<uint64> GWndrSerialGen{0};
static TMap<uint64, TArray<TPair<uint32, uint64>>> GWndrWalkCache; // input hash -> (group, BG hash)
static uint64 GWndrWalkHits = 0, GWndrWalkMisses = 0;
static inline uint64 WndrSerialOf(const FDawnUniformBuffer* U) { if (!U) { return 0; } FDawnUniformBuffer* M = const_cast<FDawnUniformBuffer*>(U); if (!M->WndrSerial) { M->WndrSerial = ++GWndrSerialGen; } return M->WndrSerial; }
bool FDawnCommandContext::FlushPendingBindGroups()
{
'''),
# key build + hit path after the early-return check
('''	if (!CurrentPSO || !CurrentPSO->Pipeline || !ActiveRenderPass || CurrentPSO->BindGroupLayouts.Num() == 0) { return true; }
	bool bAllGroupsComplete = true;''',
'''	if (!CurrentPSO || !CurrentPSO->Pipeline || !ActiveRenderPass || CurrentPSO->BindGroupLayouts.Num() == 0) { return true; }
	bool bAllGroupsComplete = true;
	// wndr WALKCACHE-1: input key
	uint64 WndrWalkKey = 0;
	TArray<TPair<uint32, uint64>, TInlineAllocator<4>> WndrWalkGroups;
	bool bWndrWalkCacheable = true;
	{
		if (!CurrentPSO->WndrSerial) { CurrentPSO->WndrSerial = ++GWndrSerialGen; }
		TArray<uint64, TInlineAllocator<256>> K;
		K.Add(CurrentPSO->WndrSerial); K.Add(GWndrLMAssocVersion);
		for (const TPair<uint32, TMap<uint32, WGPUBindGroupEntry>>& G : PendingBindEntries)
		{
			K.Add(0xA0000000ull | G.Key);
			for (const TPair<uint32, WGPUBindGroupEntry>& P : G.Value)
			{
				const WGPUBindGroupEntry& E = P.Value;
				K.Add(E.binding); K.Add((uint64)E.buffer); K.Add(E.offset); K.Add(E.size); K.Add((uint64)E.sampler); K.Add((uint64)E.textureView);
			}
		}
		K.Add(0xB0000000ull);
		for (const TRefCountPtr<FRHIUniformBuffer>& R : PendingUBRefs) { const FDawnUniformBuffer* U = static_cast<const FDawnUniformBuffer*>(R.GetReference()); K.Add(WndrSerialOf(U)); K.Add(U ? U->WndrResVersion : 0); }
		K.Add(0xC0000000ull);
		for (const TPair<FString, FRHIUniformBuffer*>& P : StaticUBsByName) { const FDawnUniformBuffer* U = static_cast<const FDawnUniformBuffer*>(P.Value); K.Add(WndrSerialOf(U)); K.Add(U ? U->WndrResVersion : 0); }
		WndrWalkKey = CityHash64((const char*)K.GetData(), K.Num() * sizeof(uint64));
		if (const TArray<TPair<uint32, uint64>>* Hit = GWndrWalkCache.Find(WndrWalkKey))
		{
			FScopeLock BL(&GWndrBGLock);
			bool bAll = true;
			TArray<WGPUBindGroup, TInlineAllocator<4>> BGs;
			for (const TPair<uint32, uint64>& GH : *Hit)
			{
				FWndrBGSlot* S = GWndrBGCache.Find(GH.Value);
				if (!S || !S->BG) { bAll = false; break; }
				S->LastFrame = GWndrBGFrame; BGs.Add(S->BG);
			}
			if (bAll)
			{
				for (int32 i = 0; i < Hit->Num(); ++i) { wgpuRenderPassEncoderSetBindGroup(ActiveRenderPass, (*Hit)[i].Key, BGs[i], 0, nullptr); }
				++GWndrWalkHits; ++GWndrBGHits;
				if ((GWndrWalkHits % 200000) == 1) { printf("WNDR WALKCACHE hits=%llu misses=%llu size=%d\\n", (unsigned long long)GWndrWalkHits, (unsigned long long)GWndrWalkMisses, GWndrWalkCache.Num()); fflush(stdout); }
				return true;
			}
			GWndrWalkCache.Remove(WndrWalkKey);
		}
		++GWndrWalkMisses;
	}'''),
# record BG hashes
('		bool bBGOwned = false; // wndr BGCACHE-2\n		WGPUBindGroup BindGroup = WndrBGCacheGet(Owner->GetDevice(), BgDesc, bBGOwned);',
 '		bool bBGOwned = false; // wndr BGCACHE-2\n		uint64 WndrBGHash = 0;\n		WGPUBindGroup BindGroup = WndrBGCacheGet(Owner->GetDevice(), BgDesc, bBGOwned, &WndrBGHash);\n		if (BindGroup && !bBGOwned) { WndrWalkGroups.Emplace(Group, WndrBGHash); } else { bWndrWalkCacheable = false; } // wndr WALKCACHE-1'),
('''		if (BindGroup) { wgpuRenderPassEncoderSetBindGroup(ActiveRenderPass, Group, BindGroup, 0, nullptr); if (bBGOwned) { wgpuBindGroupRelease(BindGroup); } } // wndr BGCACHE-2
	}
	return bAllGroupsComplete;''',
'''		if (BindGroup) { wgpuRenderPassEncoderSetBindGroup(ActiveRenderPass, Group, BindGroup, 0, nullptr); if (bBGOwned) { wgpuBindGroupRelease(BindGroup); } } // wndr BGCACHE-2
	}
	if (bAllGroupsComplete && bWndrWalkCacheable && WndrWalkGroups.Num() > 0) // wndr WALKCACHE-1
	{
		if (GWndrWalkCache.Num() > 200000) { GWndrWalkCache.Reset(); }
		GWndrWalkCache.Add(WndrWalkKey, TArray<TPair<uint32, uint64>>(WndrWalkGroups.GetData(), WndrWalkGroups.Num()));
	}
	return bAllGroupsComplete;'''),
# UBNAMECACHE-1
('				const FString UBN = DawnDeriveShaderUBName(UB->GetLayout().GetDebugName());\n				if (!UBN.IsEmpty())',
 '				if (!UB->bWndrDerivedNameSet) { UB->WndrDerivedName = DawnDeriveShaderUBName(UB->GetLayout().GetDebugName()); UB->WndrDerivedNameBuf = UB->WndrDerivedName + TEXT("Buffer"); UB->bWndrDerivedNameSet = true; } // wndr UBNAMECACHE-1\n				const FString& UBN = UB->WndrDerivedName;\n				if (!UBN.IsEmpty())'),
('B.Name.Equals(UBN + TEXT("Buffer"), ESearchCase::CaseSensitive))) { continue; } // wndr LM-fix-1: PrecomputedLightingBuffer',
 'B.Name.Equals(UB->WndrDerivedNameBuf, ESearchCase::CaseSensitive))) { continue; } // wndr LM-fix-1: PrecomputedLightingBuffer (UBNAMECACHE-1)'),
])
