#!/usr/bin/env python3
# GLOBALSLIVE-1: the transient $Globals uniform buffers built in
# RHISetShaderParameters were never registered as live handles, so the bind-time
# NULLHANDLE check swapped every one for the ZERO dummy UB. Every loose-parameter
# shader ran with zeroed globals: the HUD/canvas PS computes alpha =
# dot(tex, TextureComponentReplicateAlpha) = 0 -> the whole HUD drew invisibly.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')
s = open(CC).read()
if 'GLOBALSLIVE-1' in s:
    print('already patched'); sys.exit(0)
bak = CC + '.wndrbak_globalslive'
if not os.path.exists(bak): shutil.copy2(CC, bak)
reps = [
('''	for (WGPUBuffer UB : PendingTransientUBs) { wgpuBufferRelease(UB); }
	PendingTransientUBs.Reset();
}''',
'''	for (WGPUBuffer UB : PendingTransientUBs) { DawnLiveHandleRemove((void*)UB); wgpuBufferRelease(UB); } // wndr GLOBALSLIVE-1
	PendingTransientUBs.Reset();
}'''),
('''			WGPUBuffer UB = wgpuDeviceCreateBuffer(Owner->GetDevice(), &BD);
			wgpuQueueWriteBuffer(Owner->GetQueue(), UB, 0, Cpu.GetData(), CbSize);
			TransientUBs.Add(UB);''',
'''			WGPUBuffer UB = wgpuDeviceCreateBuffer(Owner->GetDevice(), &BD);
			wgpuQueueWriteBuffer(Owner->GetQueue(), UB, 0, Cpu.GetData(), CbSize);
			DawnLiveHandleAdd((void*)UB); // wndr GLOBALSLIVE-1: else NULLHANDLE swaps it for the zero dummy UB
			TransientUBs.Add(UB);'''),
]
for old, new in reps:
    assert s.count(old) == 1, (old[:70], s.count(old))
    s = s.replace(old, new)
open(CC, 'w').write(s)
print('patched', CC)
