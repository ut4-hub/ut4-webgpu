#!/usr/bin/env python3
# HUDDIAG-5: decode the first FSimpleElementVertex (relpos f4, tilepos f4, uv f2,
# color f4 @40) the first canvas draws read, from the CPU upload snapshot.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')
s = open(CC).read()
if 'HUDDIAG-5' in s:
    print('already patched'); sys.exit(0)
bak = CC + '.wndrbak_huddiag5'
if not os.path.exists(bak): shutil.copy2(CC, bak)
old = '''		static uint32 _hv = 0; if (_hv++ < 24) {'''
new = '''		{ extern const TArray<uint8>* DawnDiagGetVBSnapshot(void* WgpuBuf); static uint32 _hx = 0; // wndr HUDDIAG-5
		  if (bCurRTPresentable && _hx++ < 16) { FDawnBuffer* B0 = BoundVertexBuffers[0]; const TArray<uint8>* Sn = B0 ? DawnDiagGetVBSnapshot((void*)B0->Buffer) : nullptr; const uint32 Off = BoundVertexBufferOffsets[0];
			if (Sn && Off == 0 && Sn->Num() >= 56) { const float* F = (const float*)Sn->GetData();
				printf("WNDR HUDVTX #%u buf=%p size=%u v0 rel=[%.1f %.1f %.2f %.2f] tile=[%.1f %.1f %.1f %.1f] uv=[%.3f %.3f] col=[%.2f %.2f %.2f %.2f]\\n", _hx, (void*)B0->Buffer, B0->WgpuSize, F[0],F[1],F[2],F[3],F[4],F[5],F[6],F[7],F[8],F[9],F[10],F[11],F[12],F[13]); }
			else { printf("WNDR HUDVTX #%u buf=%p off=%u size=%u snap=%d\\n", _hx, B0 ? (void*)B0->Buffer : nullptr, Off, B0 ? B0->WgpuSize : 0u, Sn ? Sn->Num() : -1); }
			fflush(stdout); } }
		static uint32 _hv = 0; if (_hv++ < 24) {'''
assert s.count(old) == 2, s.count(old)
s = s.replace(old, new)
open(CC, 'w').write(s)
print('patched', CC)
