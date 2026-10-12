#!/usr/bin/env python3
# RHISTAGE-probe-1: mark where the RHI thread is inside FlushPendingBindGroups
# (bind-loop spikes of 18-95s with no extra work = a blocking wait). Exported
# getters let the main thread read the stage + a progress counter during a stall.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')
s = open(CC).read()
if 'RHISTAGE-probe-1' in s:
    print('already patched'); sys.exit(0)
bak = CC + '.wndrbak_rhistage'
if not os.path.exists(bak): shutil.copy2(CC, bak)
reps = [
('// 1x1 dummy texture VIEW matching the reflected view dimension + sample type\n',
'''// wndr RHISTAGE-probe-1: where is the RHI thread inside FlushPendingBindGroups?
#include <atomic>
static std::atomic<uint32> GWndrRhiStage{0};
static std::atomic<uint32> GWndrRhiStageMax{0};
static std::atomic<double> GWndrRhiBindCalls{0.0};
#define WNDR_STAGE(n) GWndrRhiStage.store((uint32)(n), std::memory_order_relaxed)
#if PLATFORM_WASM
extern "C" EMSCRIPTEN_KEEPALIVE uint32 wndr_rhi_stage() { return GWndrRhiStage.load(std::memory_order_relaxed); }
extern "C" EMSCRIPTEN_KEEPALIVE double wndr_rhi_bind_calls() { return GWndrRhiBindCalls.load(std::memory_order_relaxed); }
#endif
// 1x1 dummy texture VIEW matching the reflected view dimension + sample type
'''),
('''	FScopeLock Lock(&GWndrLMAssocLock);
	const TRefCountPtr<FRHIUniformBuffer>* F = GWndrLMAssoc.Find((void*)PLB);''',
'''	WNDR_STAGE(30);
	FScopeLock Lock(&GWndrLMAssocLock);
	WNDR_STAGE(31);
	const TRefCountPtr<FRHIUniformBuffer>* F = GWndrLMAssoc.Find((void*)PLB);'''),
('''_wndrBindTimer;
#endif
''',
'''_wndrBindTimer;
#endif
	WNDR_STAGE(10); GWndrRhiBindCalls.store(GWndrRhiBindCalls.load(std::memory_order_relaxed) + 1.0, std::memory_order_relaxed);
	struct FWndrStageExit { ~FWndrStageExit() { WNDR_STAGE(0); } } _wndrStageExit; // wndr RHISTAGE-probe-1
'''),
('''					FScopeLock _ResTableLock(&GDawnResTableLock); // wndr RESTABLE-lock-1
					if (DUB->ShaderName.IsEmpty()''',
'''					WNDR_STAGE(20);
					FScopeLock _ResTableLock(&GDawnResTableLock); // wndr RESTABLE-lock-1
					WNDR_STAGE(21);
					if (DUB->ShaderName.IsEmpty()'''),
('''					extern FDawnUniformBuffer* DawnUBRegistryFind(void* WgpuBuf);
''',
'''					extern FDawnUniformBuffer* DawnUBRegistryFind(void* WgpuBuf);
					WNDR_STAGE(22);
'''),
('''					for (const TPair<FString, FRHIUniformBuffer*>& P : StaticUBsByName)
					{
						if (TryUBTable(P.Value)) { bResFilled = true; break; }''',
'''					WNDR_STAGE(25);
					for (const TPair<FString, FRHIUniformBuffer*>& P : StaticUBsByName)
					{
						if (TryUBTable(P.Value)) { bResFilled = true; break; }'''),
('''	static TMap<uint64, WGPUTextureView> Cache; // nullptr cached too (negative result)
''',
'''	WNDR_STAGE(40);
	static TMap<uint64, WGPUTextureView> Cache; // nullptr cached too (negative result)
'''),
('''	WGPUTexture Tex = wgpuDeviceCreateTexture(Device, &TD);
	WGPUTextureView View = nullptr;''',
'''	WNDR_STAGE(41);
	WGPUTexture Tex = wgpuDeviceCreateTexture(Device, &TD);
	WNDR_STAGE(42);
	WGPUTextureView View = nullptr;'''),
('''		{
			bool bEntriesOk = true;
			for (WGPUBindGroupEntry& E : Entries)''',
'''		{
			WNDR_STAGE(50);
			bool bEntriesOk = true;
			for (WGPUBindGroupEntry& E : Entries)'''),
('''		WGPUBindGroup BindGroup = wgpuDeviceCreateBindGroup(Owner->GetDevice(), &BgDesc);
''',
'''		WNDR_STAGE(60);
		WGPUBindGroup BindGroup = wgpuDeviceCreateBindGroup(Owner->GetDevice(), &BgDesc);
		WNDR_STAGE(61);
'''),
]
for old, new in reps:
    assert s.count(old) == 1, (old[:70], s.count(old))
    s = s.replace(old, new)
open(CC, 'w').write(s)
print('patched', CC)
