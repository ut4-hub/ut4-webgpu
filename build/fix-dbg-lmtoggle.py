#!/usr/bin/env python3
# LMTOGGLE-1 (diag): runtime switches from JS, Module._wndr_dbg(bits):
#   bit0: LightmapResourceCluster_* textures -> neutral grey dummy
#   bit1: PrecomputedLightingBuffer -> flat ambient buffer
# Lets us A/B the patchy dark/bright world lighting without rebuilding.
import os, shutil
CC = '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private/DawnCommandContext.cpp'
s = open(CC).read()
if 'LMTOGGLE-1' in s:
    print('already patched'); raise SystemExit
bak = CC + '.wndrbak_lmtoggle'
if not os.path.exists(bak): shutil.copy2(CC, bak)
reps = [
('''static WGPUBuffer GetDawnAmbientLightmapBuffer(WGPUDevice Device, WGPUQueue Queue)''',
'''static int32 GWndrDbgBits = 0; // wndr LMTOGGLE-1
extern "C" EMSCRIPTEN_KEEPALIVE int wndr_dbg(int Bits) { GWndrDbgBits = Bits; printf("WNDR DBGBITS %d\\n", Bits); fflush(stdout); return Bits; }
static WGPUBuffer GetDawnAmbientLightmapBuffer(WGPUDevice Device, WGPUQueue Queue)'''),
('''		K.Add(CurrentPSO->WndrSerial); K.Add(GWndrLMAssocVersion);''',
'''		K.Add(CurrentPSO->WndrSerial); K.Add(GWndrLMAssocVersion); K.Add((uint64)GWndrDbgBits); // LMTOGGLE-1'''),
('''		if (GWndrDiagDraw && GWndrFxBudget < 60) // wndr FXDIAG-1
		{''',
'''		if (GWndrDbgBits) // wndr LMTOGGLE-1
		{
			for (WGPUBindGroupEntry& E : Entries)
			{
				const FDawnShaderBinding* NB = nullptr; for (const FDawnShaderBinding& B : LG.Value) { if (B.Binding == E.binding) { NB = &B; break; } }
				if (!NB) { continue; }
				if ((GWndrDbgBits & 1) && E.textureView && NB->Name.StartsWith(TEXT("LightmapResourceCluster_LightMapTexture"), ESearchCase::CaseSensitive))
				{ E.textureView = GetDawnDummyTextureView(Owner->GetDevice(), NB->TexViewDim, NB->TexSampleType, Owner->GetQueue()); }
				if ((GWndrDbgBits & 2) && E.buffer && NB->Name.Equals(TEXT("PrecomputedLightingBuffer"), ESearchCase::CaseSensitive))
				{ E.buffer = GetDawnAmbientLightmapBuffer(Owner->GetDevice(), Owner->GetQueue()); E.offset = 0; E.size = 240; }
			}
		}
		if (GWndrDiagDraw && GWndrFxBudget < 60) // wndr FXDIAG-1
		{'''),
]
for o, n in reps:
    assert s.count(o) == 1, (o[:60], s.count(o))
    s = s.replace(o, n)
open(CC, 'w').write(s)
print('patched')
