#!/usr/bin/env python3
# BGCACHE-1: every draw built a fresh WGPUBindGroup (~3200/frame, ~53 ms/frame).
# Cache bind groups by (layout, entries). Every entry handle passed the
# live-handle check, so DawnLiveHandleRemove (called when a buffer/view/sampler is
# released) evicts every cached group that references it -> a recycled handle
# address can never hit a stale group. PSO bind-group layouts evict the same way.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI'
H  = os.path.join(ROOT, 'Public', 'DawnResources.h')
CC = os.path.join(ROOT, 'Private', 'DawnCommandContext.cpp')
DR = os.path.join(ROOT, 'Private', 'DawnDynamicRHI.cpp')

def patch(path, mark, reps, bak_suffix):
    s = open(path).read()
    if mark in s:
        print('already patched', path); return
    bak = path + bak_suffix
    if not os.path.exists(bak): shutil.copy2(path, bak)
    for old, new in reps:
        assert s.count(old) == 1, (path, old[:80], s.count(old))
        s = s.replace(old, new)
    open(path, 'w').write(s)
    print('patched', path)

patch(DR, 'BGCACHE-1', [
('#include <stdio.h> // DAWNBG diagnostics (printf survives log stripping; UE_LOG does not)\n',
 '#include <stdio.h> // DAWNBG diagnostics (printf survives log stripping; UE_LOG does not)\n#include "Hash/CityHash.h" // wndr BGCACHE-1\n'),
('''void DawnLiveHandleRemove(void* H) { if (H) { FScopeLock L(&GDawnRegLock); GDawnLiveHandles.Remove(H); } }''',
'''// wndr BGCACHE-1: bind-group cache keyed by (layout, entries); evicted per released handle.
struct FDawnBGCacheEntry { WGPUBindGroup BG = nullptr; TArray<uint64> Key; };
static TMap<uint64, FDawnBGCacheEntry> GDawnBGCache;
static TMultiMap<void*, uint64> GDawnBGByHandle;
static uint64 GDawnBGHits = 0, GDawnBGMisses = 0, GDawnBGEvicts = 0, GDawnBGFlushes = 0;
static void DawnBGCacheEvictLocked(void* H)
{
	TArray<uint64> Hashes;
	GDawnBGByHandle.MultiFind(H, Hashes);
	for (uint64 Hh : Hashes)
	{
		if (FDawnBGCacheEntry* E = GDawnBGCache.Find(Hh)) { wgpuBindGroupRelease(E->BG); GDawnBGCache.Remove(Hh); ++GDawnBGEvicts; }
	}
	GDawnBGByHandle.Remove(H);
}
WGPUBindGroup DawnBGCacheGetOrCreate(WGPUDevice Device, WGPUBindGroupLayout Layout, const WGPUBindGroupEntry* Entries, int32 Num)
{
	TArray<uint64, TInlineAllocator<64>> K;
	K.Add((uint64)(UPTRINT)Layout);
	for (int32 i = 0; i < Num; ++i)
	{
		const WGPUBindGroupEntry& E = Entries[i];
		K.Add((uint64)E.binding); K.Add((uint64)(UPTRINT)E.buffer); K.Add((uint64)E.offset); K.Add((uint64)E.size);
		K.Add((uint64)(UPTRINT)E.sampler); K.Add((uint64)(UPTRINT)E.textureView);
	}
	const uint64 Hh = CityHash64((const char*)K.GetData(), K.Num() * sizeof(uint64));
	FScopeLock L(&GDawnRegLock);
	if (FDawnBGCacheEntry* Found = GDawnBGCache.Find(Hh))
	{
		if (Found->Key.Num() == K.Num() && FMemory::Memcmp(Found->Key.GetData(), K.GetData(), K.Num() * sizeof(uint64)) == 0) { ++GDawnBGHits; return Found->BG; }
		wgpuBindGroupRelease(Found->BG); GDawnBGCache.Remove(Hh); // hash collision: replace
	}
	if (GDawnBGCache.Num() >= 65536)
	{
		for (TPair<uint64, FDawnBGCacheEntry>& P : GDawnBGCache) { wgpuBindGroupRelease(P.Value.BG); }
		GDawnBGCache.Reset(); GDawnBGByHandle.Reset(); ++GDawnBGFlushes;
	}
	WGPUBindGroupDescriptor D = {};
	D.layout = Layout; D.entryCount = (size_t)Num; D.entries = Entries;
	WGPUBindGroup BG = wgpuDeviceCreateBindGroup(Device, &D);
	if (!BG) { return nullptr; }
	++GDawnBGMisses;
	FDawnBGCacheEntry& NE = GDawnBGCache.Add(Hh);
	NE.BG = BG; NE.Key.Append(K.GetData(), K.Num());
	GDawnBGByHandle.Add((void*)Layout, Hh);
	for (int32 i = 0; i < Num; ++i)
	{
		void* Hd = Entries[i].sampler ? (void*)Entries[i].sampler : (Entries[i].textureView ? (void*)Entries[i].textureView : (void*)Entries[i].buffer);
		if (Hd) { GDawnBGByHandle.Add(Hd, Hh); }
	}
	if (((GDawnBGHits + GDawnBGMisses) % 200000) < 1) { printf("WNDR BGCACHE hits=%llu misses=%llu evicts=%llu flushes=%llu live=%d\\n", (unsigned long long)GDawnBGHits, (unsigned long long)GDawnBGMisses, (unsigned long long)GDawnBGEvicts, (unsigned long long)GDawnBGFlushes, GDawnBGCache.Num()); fflush(stdout); }
	return BG;
}
void DawnLiveHandleRemove(void* H) { if (H) { FScopeLock L(&GDawnRegLock); GDawnLiveHandles.Remove(H); DawnBGCacheEvictLocked(H); } }'''),
], '.wndrbak_bgcache')

patch(CC, 'BGCACHE-1', [
('''		WGPUBindGroup BindGroup = wgpuDeviceCreateBindGroup(Owner->GetDevice(), &BgDesc);''',
'''		extern WGPUBindGroup DawnBGCacheGetOrCreate(WGPUDevice, WGPUBindGroupLayout, const WGPUBindGroupEntry*, int32);
		WGPUBindGroup BindGroup = DawnBGCacheGetOrCreate(Owner->GetDevice(), BgDesc.layout, Entries.GetData(), Entries.Num()); // wndr BGCACHE-1 (cache owns the ref)'''),
('''		if (BindGroup) { wgpuRenderPassEncoderSetBindGroup(ActiveRenderPass, Group, BindGroup, 0, nullptr); wgpuBindGroupRelease(BindGroup); }''',
'''		if (BindGroup) { wgpuRenderPassEncoderSetBindGroup(ActiveRenderPass, Group, BindGroup, 0, nullptr); }'''),
], '.wndrbak_bgcache')

patch(H, 'BGCACHE-1', [
('''		if (PipelineLayout) { wgpuPipelineLayoutRelease(PipelineLayout); }
		for (WGPUBindGroupLayout Layout : BindGroupLayouts) { if (Layout) { wgpuBindGroupLayoutRelease(Layout); } }
	}
};''',
'''		if (PipelineLayout) { wgpuPipelineLayoutRelease(PipelineLayout); }
		for (WGPUBindGroupLayout Layout : BindGroupLayouts) { if (Layout) { DawnLiveHandleRemove((void*)Layout); /* wndr BGCACHE-1: evict cached groups */ wgpuBindGroupLayoutRelease(Layout); } }
	}
};'''),
], '.wndrbak_bgcache')
