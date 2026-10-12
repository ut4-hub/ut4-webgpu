#!/usr/bin/env python3
# RESTABLE-lock-1: FDawnUniformBuffer::ResTable (TArray<TRefCountPtr>) and ShaderName
# were rebuilt by DawnCaptureUBResourceTable on whatever thread ran the UB update
# (render thread, parallel-translate workers) while the RHI thread iterated them in
# TryUBTable. A torn TArray double-releases its entries -> REFUNDERFLOW at
# ~FDawnUniformBuffer -> use-after-free crash in DeleteResources. Guard both sides
# with one global lock. Also UBRACE-fix-2: the deferred update keeps counted refs to
# the contents' resources until it runs on the RHI thread.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
DR = os.path.join(ROOT, 'DawnDynamicRHI.cpp')
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')

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

patch(DR, 'RESTABLE-lock-1', [
(
'''static void DawnCaptureUBResourceTable(FDawnUniformBuffer* UB, const void* Contents)
{
''',
'''FCriticalSection GDawnResTableLock; // wndr RESTABLE-lock-1 (ResTable/ShaderName writers vs bind-time readers)
static void DawnCaptureUBResourceTable(FDawnUniformBuffer* UB, const void* Contents)
{
	FScopeLock _ResTableLock(&GDawnResTableLock); // wndr RESTABLE-lock-1
'''),
(
'''		TRefCountPtr<FRHIUniformBuffer> Keep(UniformBufferRHI);
		RHICmdList.EnqueueLambda(TEXT("DawnUpdateUniformBuffer"), [this, Keep, Copy](FRHICommandListBase& Cmd)
		{''',
'''		TRefCountPtr<FRHIUniformBuffer> Keep(UniformBufferRHI);
		// wndr UBRACE-fix-2: hold counted refs to the contents' resources until the update runs.
		TArray<TRefCountPtr<FRHIResource>> KeepRes;
		for (const FRHIUniformBufferResource& R : L.Resources)
		{
			FRHIResource* Res = *(FRHIResource* const*)((const uint8*)Contents + R.MemberOffset);
			if (Res && Res->IsValid()) { KeepRes.Emplace(Res); }
		}
		RHICmdList.EnqueueLambda(TEXT("DawnUpdateUniformBuffer"), [this, Keep, Copy, KeepRes = MoveTemp(KeepRes)](FRHICommandListBase& Cmd)
		{'''),
], '.wndrbak_reslock')

patch(CC, 'RESTABLE-lock-1', [
(
'''extern bool DawnLiveHandleCheck(void* H);
''',
'''extern bool DawnLiveHandleCheck(void* H);
extern FCriticalSection GDawnResTableLock; // wndr RESTABLE-lock-1 (DawnDynamicRHI.cpp)
'''),
(
'''					FDawnUniformBuffer* DUB = static_cast<FDawnUniformBuffer*>(UBRHI);
					if (DUB->ShaderName.IsEmpty() || DUB->ResTable.Num() == 0) { return false; }''',
'''					FDawnUniformBuffer* DUB = static_cast<FDawnUniformBuffer*>(UBRHI);
					FScopeLock _ResTableLock(&GDawnResTableLock); // wndr RESTABLE-lock-1
					if (DUB->ShaderName.IsEmpty() || DUB->ResTable.Num() == 0) { return false; }'''),
], '.wndrbak_reslock')
