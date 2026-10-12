#!/usr/bin/env python3
# STATICUB-hold-1: StaticUBsByName kept RAW FRHIUniformBuffer* forever. Once the
# engine freed a static UB, every bind walked the dangling pointer in TryUBTable:
# `DUB->ShaderName + "_"` copied a garbage-sized string (≈1 s of memmove/memset per
# draw = the 18-95 s "walk" frames) or trapped (boot41: memcpy size=1.76 GB -> OOB).
# Hold a counted ref for every map entry. Also REGLOCK-1: the UB registry and the
# live-handle set are touched from render, RHI and deleting threads with no lock.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI'
H  = os.path.join(ROOT, 'Public', 'DawnCommandContext.h')
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

patch(H, 'STATICUB-hold-1', [(
'	TMap<FString, FRHIUniformBuffer*> StaticUBsByName;\n',
'	TMap<FString, FRHIUniformBuffer*> StaticUBsByName;\n'
'	// wndr STATICUB-hold-1: counted refs keeping every StaticUBsByName pointer alive (same keys).\n'
'	TMap<FString, TRefCountPtr<FRHIUniformBuffer>> StaticUBHold;\n')], '.wndrbak_staticub')

patch(CC, 'STATICUB-hold-1', [
('''			StaticUBsByName.Add(UB->GetLayout().GetDebugName(), UB);
			{ const FString DKey = DawnDeriveShaderUBName(UB->GetLayout().GetDebugName()); if (!DKey.IsEmpty()) StaticUBsByName.Add(DKey, UB); }''',
'''			StaticUBsByName.Add(UB->GetLayout().GetDebugName(), UB); StaticUBHold.Add(UB->GetLayout().GetDebugName(), UB); // wndr STATICUB-hold-1
			{ const FString DKey = DawnDeriveShaderUBName(UB->GetLayout().GetDebugName()); if (!DKey.IsEmpty()) { StaticUBsByName.Add(DKey, UB); StaticUBHold.Add(DKey, UB); } }'''),
('''		StaticUBsByName.Add(UniformBuffer->GetLayout().GetDebugName(), UniformBuffer);
		{ const FString DKey = DawnDeriveShaderUBName(UniformBuffer->GetLayout().GetDebugName()); if (!DKey.IsEmpty()) StaticUBsByName.Add(DKey, UniformBuffer); }''',
'''		StaticUBsByName.Add(UniformBuffer->GetLayout().GetDebugName(), UniformBuffer); StaticUBHold.Add(UniformBuffer->GetLayout().GetDebugName(), UniformBuffer); // wndr STATICUB-hold-1
		{ const FString DKey = DawnDeriveShaderUBName(UniformBuffer->GetLayout().GetDebugName()); if (!DKey.IsEmpty()) { StaticUBsByName.Add(DKey, UniformBuffer); StaticUBHold.Add(DKey, UniformBuffer); } }'''),
], '.wndrbak_staticub')

patch(DR, 'REGLOCK-1', [
('''static TSet<void*> GDawnLiveHandles;
void DawnLiveHandleAdd(void* H) { if (H && GDawnLiveHandles.Num() < 1000000) { GDawnLiveHandles.Add(H); } }
void DawnLiveHandleRemove(void* H) { if (H) { GDawnLiveHandles.Remove(H); } }
bool DawnLiveHandleCheck(void* H) { return H && GDawnLiveHandles.Contains(H); }''',
'''static TSet<void*> GDawnLiveHandles;
static FCriticalSection GDawnRegLock; // wndr REGLOCK-1: live-handle set + UB registry are multi-threaded
void DawnLiveHandleAdd(void* H) { if (H) { FScopeLock L(&GDawnRegLock); if (GDawnLiveHandles.Num() < 1000000) { GDawnLiveHandles.Add(H); } } }
void DawnLiveHandleRemove(void* H) { if (H) { FScopeLock L(&GDawnRegLock); GDawnLiveHandles.Remove(H); } }
bool DawnLiveHandleCheck(void* H) { if (!H) { return false; } FScopeLock L(&GDawnRegLock); return GDawnLiveHandles.Contains(H); }'''),
('''void DawnUBRegistryRemove(void* WgpuBuf) { if (WgpuBuf) { GDawnUBByBuffer.Remove(WgpuBuf); } }
FDawnUniformBuffer* DawnUBRegistryFind(void* WgpuBuf)
{
	FDawnUniformBuffer** Found = WgpuBuf ? GDawnUBByBuffer.Find(WgpuBuf) : nullptr;''',
'''void DawnUBRegistryRemove(void* WgpuBuf) { if (WgpuBuf) { FScopeLock L(&GDawnRegLock); GDawnUBByBuffer.Remove(WgpuBuf); } }
FDawnUniformBuffer* DawnUBRegistryFind(void* WgpuBuf)
{
	FScopeLock L(&GDawnRegLock); // wndr REGLOCK-1
	FDawnUniformBuffer** Found = WgpuBuf ? GDawnUBByBuffer.Find(WgpuBuf) : nullptr;'''),
('''static void DawnUBRegistryAdd(FDawnUniformBuffer* UB) { if (UB && UB->Buffer) { GDawnUBByBuffer.Add((void*)UB->Buffer, UB); DawnLiveHandleAdd((void*)UB->Buffer); } }''',
'''static void DawnUBRegistryAdd(FDawnUniformBuffer* UB) { if (UB && UB->Buffer) { { FScopeLock L(&GDawnRegLock); GDawnUBByBuffer.Add((void*)UB->Buffer, UB); } DawnLiveHandleAdd((void*)UB->Buffer); } }'''),
], '.wndrbak_reglock')
