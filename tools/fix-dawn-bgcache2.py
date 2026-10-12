#!/usr/bin/env python3
# BGCACHE-2 + UBPOOL-1 (perf): wgpuDeviceCreateBindGroup was ~16% of the frame.
#  UBPOOL-1: uniform buffers (and per-draw $Globals transients) are recycled through a
#    per-size FIFO pool instead of released; buffers retired during a frame become
#    reusable only after that frame's RHIEndFrame submit. Same allocation sequence each
#    frame -> same WGPUBuffer handles (per-frame View UB etc.) -> bind groups repeat.
#  BGCACHE-2: bind groups cached by exact (layout, entries) key; a dying handle
#    (DawnLiveHandleRemove) evicts every cached group that references it, so a reused
#    pointer value can never hit a stale group. Unused groups evicted after 8 frames.
import sys, shutil, os
R = '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/'
H = R + 'Public/DawnResources.h'; DR = R + 'Private/DawnDynamicRHI.cpp'; CC = R + 'Private/DawnCommandContext.cpp'
def patch(path, reps):
    s = open(path).read()
    if 'BGCACHE-2' in s: print('already patched', path); return
    bak = path + '.wndrbak_bgcache2'
    if not os.path.exists(bak): shutil.copy2(path, bak)
    for old, new in reps:
        assert s.count(old) == 1, (path, old[:80], s.count(old))
        s = s.replace(old, new)
    open(path, 'w').write(s); print('patched', path)

patch(H, [
('void DawnLiveHandleRemove(void* H);\n',
 'void DawnLiveHandleRemove(void* H);\nvoid DawnUBPoolRetire(WGPUBuffer B, uint64 Size); // wndr BGCACHE-2/UBPOOL-1 (DawnDynamicRHI.cpp)\n'),
('		if (Buffer) { DawnUBRegistryRemove((void*)Buffer); DawnLiveHandleRemove((void*)Buffer); wgpuBufferRelease(Buffer); }',
 '		if (Buffer) { DawnUBRegistryRemove((void*)Buffer); DawnUBPoolRetire(Buffer, 0); } // wndr BGCACHE-2/UBPOOL-1: recycle (stays live)'),
])

patch(DR, [
('void DawnLiveHandleRemove(void* H) { if (H) { FScopeLock L(&GDawnRegLock); GDawnLiveHandles.Remove(H); } }',
 '''extern void DawnBGCacheInvalidate(void* H); // wndr BGCACHE-2 (DawnCommandContext.cpp)
void DawnLiveHandleRemove(void* H) { if (H) { { FScopeLock L(&GDawnRegLock); GDawnLiveHandles.Remove(H); } DawnBGCacheInvalidate(H); } }

// wndr BGCACHE-2/UBPOOL-1: per-size FIFO pool of uniform buffers. Retired buffers may
// still be referenced by this frame's unsubmitted encoder, so they only become reusable
// at DawnUBPoolEndFrame (after RHIEndFrame's submit). Pooled buffers stay registered live.
struct FWndrUBFreeList { TArray<WGPUBuffer> Items; int32 Head = 0; };
static FCriticalSection GWndrUBPoolLock;
static TMap<uint64, FWndrUBFreeList> GWndrUBFree;
static TArray<TPair<uint64, WGPUBuffer>> GWndrUBRetired;
WGPUBuffer DawnUBPoolGet(uint64 Size)
{
	FScopeLock L(&GWndrUBPoolLock);
	if (FWndrUBFreeList* F = GWndrUBFree.Find(Size)) { if (F->Head < F->Items.Num()) { return F->Items[F->Head++]; } }
	return nullptr;
}
void DawnUBPoolRetire(WGPUBuffer B, uint64 Size)
{
	if (!B) { return; }
	if (Size == 0) { Size = wgpuBufferGetSize(B); }
	FScopeLock L(&GWndrUBPoolLock);
	GWndrUBRetired.Emplace(Size, B);
}
void DawnUBPoolEndFrame()
{
	TArray<WGPUBuffer> Excess;
	{
		FScopeLock L(&GWndrUBPoolLock);
		for (TPair<uint64, FWndrUBFreeList>& KV : GWndrUBFree)
		{
			FWndrUBFreeList& F = KV.Value;
			if (F.Head > 0) { F.Items.RemoveAt(0, F.Head, EAllowShrinking::No); F.Head = 0; }
		}
		for (const TPair<uint64, WGPUBuffer>& P : GWndrUBRetired) { GWndrUBFree.FindOrAdd(P.Key).Items.Add(P.Value); }
		GWndrUBRetired.Reset();
		for (TPair<uint64, FWndrUBFreeList>& KV : GWndrUBFree)
		{
			FWndrUBFreeList& F = KV.Value;
			const int32 Cap = 8192;
			if (F.Items.Num() > Cap) { const int32 N = F.Items.Num() - Cap; Excess.Append(F.Items.GetData(), N); F.Items.RemoveAt(0, N, EAllowShrinking::No); }
		}
	}
	for (WGPUBuffer B : Excess) { DawnLiveHandleRemove((void*)B); wgpuBufferRelease(B); }
}'''),
('	UB->Buffer = wgpuDeviceCreateBuffer(Device, &BufDesc);\n	checkf(UB->Buffer, TEXT("DawnRHI: uniform buffer creation failed"));',
 '	UB->Buffer = DawnUBPoolGet(Size); // wndr BGCACHE-2/UBPOOL-1\n	if (!UB->Buffer) { UB->Buffer = wgpuDeviceCreateBuffer(Device, &BufDesc); }\n	checkf(UB->Buffer, TEXT("DawnRHI: uniform buffer creation failed"));'),
('	if (CommandContext.IsValid()) { CommandContext->WndrReleaseFrameHeldUBs(); }\n#endif\n#if PLATFORM_WASM\n	// Controlled end-of-frame delete drain (RHIResources.cpp, friend of FRHIResource -> can call the\n	// private Gather/Delete statics). Fence held during deletes so the executor cascade can\'t re-enter.\n	{ extern RHI_API void WndrDrainPendingDeletesEndFrame(); WndrDrainPendingDeletesEndFrame(); }\n#endif',
 '	if (CommandContext.IsValid()) { CommandContext->WndrReleaseFrameHeldUBs(); }\n#endif\n#if PLATFORM_WASM\n	// Controlled end-of-frame delete drain (RHIResources.cpp, friend of FRHIResource -> can call the\n	// private Gather/Delete statics). Fence held during deletes so the executor cascade can\'t re-enter.\n	{ extern RHI_API void WndrDrainPendingDeletesEndFrame(); WndrDrainPendingDeletesEndFrame(); }\n	{ extern void DawnBGCacheEndFrame(); DawnUBPoolEndFrame(); DawnBGCacheEndFrame(); } // wndr BGCACHE-2 (after submit + deletes)\n#endif'),
])

patch(CC, [
('extern void DawnLiveHandleAdd(void* H);   // LIVEHANDLE registry (DawnDynamicRHI.cpp)\n',
 '''extern void DawnLiveHandleAdd(void* H);   // LIVEHANDLE registry (DawnDynamicRHI.cpp)
extern WGPUBuffer DawnUBPoolGet(uint64 Size); // wndr BGCACHE-2/UBPOOL-1
extern void DawnUBPoolRetire(WGPUBuffer B, uint64 Size);
// wndr BGCACHE-2: exact-key bind group cache (see fix-dawn-bgcache2.py).
struct FWndrBGSlot { TArray<uint64> Words; WGPUBindGroup BG = nullptr; uint32 LastFrame = 0; };
static FCriticalSection GWndrBGLock;
static TMap<uint64, FWndrBGSlot> GWndrBGCache;
static TMap<void*, TArray<uint64>> GWndrBGByHandle;
static uint32 GWndrBGFrame = 0;
static uint64 GWndrBGHits = 0, GWndrBGMisses = 0, GWndrBGEvicts = 0;
static void WndrBGDropLocked(uint64 Hash)
{
	FWndrBGSlot* S = GWndrBGCache.Find(Hash);
	if (!S) { return; }
	for (int32 i = 1; i + 5 < S->Words.Num(); i += 6)
	{
		const uint64 Hs[3] = { S->Words[i + 1], S->Words[i + 4], S->Words[i + 5] };
		for (uint64 Hd : Hs) { if (Hd) { if (TArray<uint64>* L = GWndrBGByHandle.Find((void*)Hd)) { L->RemoveSwap(Hash, EAllowShrinking::No); if (L->Num() == 0) { GWndrBGByHandle.Remove((void*)Hd); } } } }
	}
	if (S->BG) { wgpuBindGroupRelease(S->BG); }
	GWndrBGCache.Remove(Hash);
	++GWndrBGEvicts;
}
void DawnBGCacheInvalidate(void* H)
{
	if (!H) { return; }
	FScopeLock L(&GWndrBGLock);
	TArray<uint64>* Hs = GWndrBGByHandle.Find(H);
	if (!Hs) { return; }
	TArray<uint64> Copy = MoveTemp(*Hs);
	GWndrBGByHandle.Remove(H);
	for (uint64 Hash : Copy) { WndrBGDropLocked(Hash); }
}
void DawnBGCacheEndFrame()
{
	FScopeLock L(&GWndrBGLock);
	++GWndrBGFrame;
	if ((GWndrBGFrame % 30) == 0)
	{
		TArray<uint64> Old;
		for (const TPair<uint64, FWndrBGSlot>& KV : GWndrBGCache) { if (KV.Value.LastFrame + 8 < GWndrBGFrame) { Old.Add(KV.Key); } }
		for (uint64 Hash : Old) { WndrBGDropLocked(Hash); }
	}
	if ((GWndrBGFrame % 300) == 0)
	{
		printf("WNDR BGCACHE f=%u size=%d hits=%llu misses=%llu evicts=%llu\\n", GWndrBGFrame, GWndrBGCache.Num(), (unsigned long long)GWndrBGHits, (unsigned long long)GWndrBGMisses, (unsigned long long)GWndrBGEvicts);
		fflush(stdout);
	}
}
// Returns the group to bind; bOwned=true -> caller must release it (uncached collision path).
static WGPUBindGroup WndrBGCacheGet(WGPUDevice Dev, const WGPUBindGroupDescriptor& D, bool& bOwned)
{
	bOwned = false;
	TArray<uint64, TInlineAllocator<160>> W;
	W.Add((uint64)D.layout);
	for (size_t i = 0; i < D.entryCount; ++i)
	{
		const WGPUBindGroupEntry& E = D.entries[i];
		W.Add((uint64)E.binding); W.Add((uint64)E.buffer); W.Add((uint64)E.offset); W.Add((uint64)E.size); W.Add((uint64)E.sampler); W.Add((uint64)E.textureView);
	}
	const uint64 Hash = CityHash64((const char*)W.GetData(), W.Num() * sizeof(uint64));
	FScopeLock L(&GWndrBGLock);
	if (FWndrBGSlot* S = GWndrBGCache.Find(Hash))
	{
		if (S->Words.Num() == W.Num() && FMemory::Memcmp(S->Words.GetData(), W.GetData(), W.Num() * sizeof(uint64)) == 0)
		{
			S->LastFrame = GWndrBGFrame; ++GWndrBGHits;
			return S->BG;
		}
		++GWndrBGMisses; bOwned = true;
		return wgpuDeviceCreateBindGroup(Dev, &D); // hash collision: do not cache
	}
	++GWndrBGMisses;
	WGPUBindGroup BG = wgpuDeviceCreateBindGroup(Dev, &D);
	if (!BG) { return nullptr; }
	FWndrBGSlot& S = GWndrBGCache.Add(Hash);
	S.Words.Append(W.GetData(), W.Num()); S.BG = BG; S.LastFrame = GWndrBGFrame;
	for (size_t i = 0; i < D.entryCount; ++i)
	{
		const WGPUBindGroupEntry& E = D.entries[i];
		void* Hd[3] = { (void*)E.buffer, (void*)E.sampler, (void*)E.textureView };
		for (void* P : Hd) { if (P) { GWndrBGByHandle.FindOrAdd(P).Add(Hash); } }
	}
	return BG;
}
'''),
('		WGPUBindGroup BindGroup = wgpuDeviceCreateBindGroup(Owner->GetDevice(), &BgDesc);',
 '		bool bBGOwned = false; // wndr BGCACHE-2\n		WGPUBindGroup BindGroup = WndrBGCacheGet(Owner->GetDevice(), BgDesc, bBGOwned);'),
('		if (BindGroup) { wgpuRenderPassEncoderSetBindGroup(ActiveRenderPass, Group, BindGroup, 0, nullptr); wgpuBindGroupRelease(BindGroup); }',
 '		if (BindGroup) { wgpuRenderPassEncoderSetBindGroup(ActiveRenderPass, Group, BindGroup, 0, nullptr); if (bBGOwned) { wgpuBindGroupRelease(BindGroup); } } // wndr BGCACHE-2'),
('	for (WGPUBuffer UB : PendingTransientUBs) { DawnLiveHandleRemove((void*)UB); wgpuBufferRelease(UB); } // wndr GLOBALSLIVE-1',
 '	for (WGPUBuffer UB : PendingTransientUBs) { DawnUBPoolRetire(UB, 0); } // wndr GLOBALSLIVE-1 + BGCACHE-2: recycle (stays live)'),
('			WGPUBufferDescriptor BD = {};\n			BD.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;\n			BD.size = CbSize;\n			WGPUBuffer UB = wgpuDeviceCreateBuffer(Owner->GetDevice(), &BD);',
 '			WGPUBufferDescriptor BD = {};\n			BD.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst; // wndr BGCACHE-2: same usage as pooled UBs\n			BD.size = CbSize;\n			WGPUBuffer UB = DawnUBPoolGet(CbSize); // wndr BGCACHE-2/UBPOOL-1\n			if (!UB) { UB = wgpuDeviceCreateBuffer(Owner->GetDevice(), &BD); }'),
])
