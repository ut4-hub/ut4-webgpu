#!/usr/bin/env python3
# HUDDIAG-4: draw ORDER on the backbuffer per frame: index of first/last canvas
# draw vs the last non-canvas draw (a full-screen tonemap/copy after the HUD
# would paint over it), plus the first canvas draw's vertex count/instances.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')
s = open(CC).read()
if 'HUDDIAG-4' in s:
    print('already patched'); sys.exit(0)
bak = CC + '.wndrbak_huddiag4'
if not os.path.exists(bak): shutil.copy2(CC, bak)
reps = [
('uint32 GDawnFrameCanvasBB = 0, GDawnFrameCanvasInt = 0; // wndr HUDDIAG-1\n',
 'uint32 GDawnFrameCanvasBB = 0, GDawnFrameCanvasInt = 0; // wndr HUDDIAG-1\n'
 'uint32 GWndrBBSeq = 0, GWndrCanvasFirst = 0, GWndrCanvasLast = 0, GWndrOtherLast = 0, GWndrOtherLastPass = 0, GWndrCanvasLastPass = 0, GWndrBBPassSeq = 0; // wndr HUDDIAG-4\n'),
('''	if (_fr <= 60 || (_fr % 300) == 0) { printf("WNDR HUDDIAG f=%u canvasDraws(bb=%u int=%u)\\n", _fr, GDawnFrameCanvasBB, GDawnFrameCanvasInt); fflush(stdout); }''',
'''	if (_fr <= 60 || (_fr % 300) == 0) { printf("WNDR HUDDIAG f=%u canvasDraws(bb=%u int=%u) bbDraws=%u canvasIdx=%u..%u (pass %u) lastOtherIdx=%u (pass %u) bbPasses=%u\\n", _fr, GDawnFrameCanvasBB, GDawnFrameCanvasInt, GWndrBBSeq, GWndrCanvasFirst, GWndrCanvasLast, GWndrCanvasLastPass, GWndrOtherLast, GWndrOtherLastPass, GWndrBBPassSeq); fflush(stdout); }
	GWndrBBSeq = GWndrCanvasFirst = GWndrCanvasLast = GWndrOtherLast = GWndrOtherLastPass = GWndrCanvasLastPass = GWndrBBPassSeq = 0;'''),
]
for old, new in reps:
    assert s.count(old) == 1, (old[:70], s.count(old))
    s = s.replace(old, new)
old = '''	if (GWndrCanvasIs) { if (bCurRTPresentable && bCurPassHasColor) { ++GDawnFrameCanvasBB; } else { ++GDawnFrameCanvasInt; }
'''
new = '''	if (bCurRTPresentable && bCurPassHasColor) { ++GWndrBBSeq; if (GWndrCanvasIs) { if (!GWndrCanvasFirst) { GWndrCanvasFirst = GWndrBBSeq; } GWndrCanvasLast = GWndrBBSeq; GWndrCanvasLastPass = GWndrBBPassSeq; } else { GWndrOtherLast = GWndrBBSeq; GWndrOtherLastPass = GWndrBBPassSeq; } } // wndr HUDDIAG-4
	if (GWndrCanvasIs) { if (bCurRTPresentable && bCurPassHasColor) { ++GDawnFrameCanvasBB; } else { ++GDawnFrameCanvasInt; }
'''
assert s.count(old) == 2, s.count(old)
s = s.replace(old, new)
old2 = '''	bCurRTPresentable = ColorTex && EnumHasAnyFlags(ColorTex->GetDesc().Flags, ETextureCreateFlags::Presentable);
'''
assert s.count(old2) == 1, s.count(old2)
s = s.replace(old2, old2 + '	if (bCurRTPresentable) { ++GWndrBBPassSeq; } // wndr HUDDIAG-4\n')
open(CC, 'w').write(s)
print('patched', CC)
