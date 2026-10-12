#!/usr/bin/env python3
# HUDDIAG-1: is the HUD (UE canvas, SimpleElement* shaders sampling "InTexture")
# drawn at all? Count canvas draws per frame (bb vs intermediate) in FRAMESUM.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')
s = open(CC).read()
if 'HUDDIAG-1' in s:
    print('already patched'); sys.exit(0)
bak = CC + '.wndrbak_huddiag'
if not os.path.exists(bak): shutil.copy2(CC, bak)
reps = [
('uint32 GDawnFrameDrawsBB = 0, GDawnFrameDrawsInt = 0, GDawnFrameSkips = 0;\n',
 'uint32 GDawnFrameDrawsBB = 0, GDawnFrameDrawsInt = 0, GDawnFrameSkips = 0;\n'
 'uint32 GDawnFrameCanvasBB = 0, GDawnFrameCanvasInt = 0; // wndr HUDDIAG-1\n'
 'static const void* GWndrCanvasPSO = nullptr; static bool GWndrCanvasIs = false;\n'
 'struct FDawnGraphicsPipelineState;\n'),
('''	if (_fr <= 40) { printf("DAWNBG PERFPROF''',
'''	if (_fr <= 60 || (_fr % 300) == 0) { printf("WNDR HUDDIAG f=%u canvasDraws(bb=%u int=%u)\\n", _fr, GDawnFrameCanvasBB, GDawnFrameCanvasInt); fflush(stdout); }
	GDawnFrameCanvasBB = GDawnFrameCanvasInt = 0;
	if (_fr <= 40) { printf("DAWNBG PERFPROF'''),
]
for old, new in reps:
    assert s.count(old) == 1, (old[:70], s.count(old))
    s = s.replace(old, new)
cnt = '''	if (bCurRTPresentable && bCurPassHasColor) { ++GDawnFrameDrawsBB; } else { ++GDawnFrameDrawsInt; }
'''
add = '''	if (bCurRTPresentable && bCurPassHasColor) { ++GDawnFrameDrawsBB; } else { ++GDawnFrameDrawsInt; }
	if ((const void*)CurrentPSO != GWndrCanvasPSO) // wndr HUDDIAG-1
	{
		GWndrCanvasPSO = (const void*)CurrentPSO; GWndrCanvasIs = false;
		if (CurrentPSO && CurrentPSO->PixelShader) { for (const FDawnShaderBinding& Bd : CurrentPSO->PixelShader->Bindings) { if (Bd.Name == TEXT("InTexture")) { GWndrCanvasIs = true; break; } } }
	}
	if (GWndrCanvasIs) { if (bCurRTPresentable && bCurPassHasColor) { ++GDawnFrameCanvasBB; } else { ++GDawnFrameCanvasInt; } }
'''
assert s.count(cnt) == 2, s.count(cnt)
s = s.replace(cnt, add)
open(CC, 'w').write(s)
print('patched', CC)
