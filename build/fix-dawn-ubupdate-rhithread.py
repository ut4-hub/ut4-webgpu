#!/usr/bin/env python3
# UBRACE-fix-1: RHIUpdateUniformBuffer ran on the render thread and did
# ResTable.Reset() (releasing the old texture/buffer refs) while the RHI thread
# was reading the same ResTable in FlushPendingBindGroups -> double release
# (WNDR REFUNDERFLOW), use-after-free, then "table index"/"memory access out of
# bounds" in FRHIResource::DeleteResources. Defer the update onto the RHI
# thread like the other RHIs do, with a copy of the contents.
# Also: DUMMYWHITE - re-enable the neutral grey fill for RGBA8 placeholder
# textures so an unbound slot no longer paints a surface black.
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

patch(DR, 'UBRACE-fix-1', [(
'''	FDawnUniformBuffer* UB = static_cast<FDawnUniformBuffer*>(UniformBufferRHI);
	if (UB && UB->Buffer && Contents)
	{
		const uint32 Size = UB->GetLayout().ConstantBufferSize;''',
'''	FDawnUniformBuffer* UB = static_cast<FDawnUniformBuffer*>(UniformBufferRHI);
	// wndr UBRACE-fix-1: run the update (GPU write + ResTable swap) on the RHI
	// thread, in command order, so it never races the bind-time ResTable reads.
	if (UB && UB->Buffer && Contents && !RHICmdList.IsBottomOfPipe())
	{
		const FRHIUniformBufferLayout& L = UB->GetLayout();
		uint32 CopySize = L.ConstantBufferSize;
		for (const FRHIUniformBufferResource& R : L.Resources) { CopySize = FMath::Max<uint32>(CopySize, (uint32)R.MemberOffset + (uint32)sizeof(void*)); }
		void* Copy = RHICmdList.Alloc(CopySize, 16);
		FMemory::Memcpy(Copy, Contents, CopySize);
		TRefCountPtr<FRHIUniformBuffer> Keep(UniformBufferRHI);
		RHICmdList.EnqueueLambda(TEXT("DawnUpdateUniformBuffer"), [this, Keep, Copy](FRHICommandListBase& Cmd)
		{
			RHIUpdateUniformBuffer(Cmd, Keep.GetReference(), Copy);
		});
		return;
	}
	if (UB && UB->Buffer && Contents)
	{
		const uint32 Size = UB->GetLayout().ConstantBufferSize;''')], '.wndrbak_ubrace')

patch(CC, 'DUMMYWHITE', [(
'''	if (false && Tex && Queue && Format == WGPUTextureFormat_RGBA8Unorm)''',
'''	if (/* wndr DUMMYWHITE: neutral grey, not black */ Tex && Queue && Format == WGPUTextureFormat_RGBA8Unorm)''')], '.wndrbak_dummywhite')
