#!/usr/bin/env python3
# HUDDIAG-7: canvas PS alpha = dot(tex, $Globals.TextureComponentReplicateAlpha)
# (binding 102). If that entry is gone at draw time, a zero dummy UB -> alpha 0.
# Print the pending bindings of group 0 for the first canvas-PSO flushes.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')
s = open(CC).read()
if 'HUDDIAG-7' in s:
    print('already patched'); sys.exit(0)
bak = CC + '.wndrbak_huddiag7'
if not os.path.exists(bak): shutil.copy2(CC, bak)
old = '''		const TMap<uint32, WGPUBindGroupEntry>* Pending = PendingBindEntries.Find(Group);
'''
new = '''		const TMap<uint32, WGPUBindGroupEntry>* Pending = PendingBindEntries.Find(Group);
		if (CurrentPSO->PixelShader) // wndr HUDDIAG-7
		{
			bool bCv = false; for (const FDawnShaderBinding& Bd : CurrentPSO->PixelShader->Bindings) { if (Bd.Name == TEXT("InTexture")) { bCv = true; break; } }
			static uint32 _hp = 0;
			if (bCv && _hp++ < 30)
			{
				printf("WNDR HUDPEND #%u grp=%u pending:", _hp, Group);
				if (Pending) { for (const TPair<uint32, WGPUBindGroupEntry>& P : *Pending) { printf(" %u(buf=%p sz=%llu tv=%p smp=%p)", P.Key, (void*)P.Value.buffer, (unsigned long long)P.Value.size, (void*)P.Value.textureView, (void*)P.Value.sampler); } }
				printf(" | want:"); for (const FDawnShaderBinding& B : LG.Value) { printf(" %u:%s", B.Binding, TCHAR_TO_UTF8(*B.Name)); }
				printf("\\n"); fflush(stdout);
			}
		}
'''
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
open(CC, 'w').write(s)
print('patched', CC)
