#!/usr/bin/env python3
# STICKYUB-1 (texture flicker fix): UE's mesh-draw state cache only re-sends a
# uniform buffer when it differs from the previous draw's (same PSO). The
# 2026-08-20 perf change emptied PendingUBRefs after every draw, so a draw that
# shares e.g. its LightmapResourceCluster UB with the previous draw resolved its
# lightmap textures from a fallback (dummy / another object's) -> per-frame
# flicker as draw order changes. Keep the last UB per (stage, slot) until the
# PSO changes, and seed each draw's walk list from it.
import sys, shutil, os
ROOT = '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI'
H = ROOT + '/Public/DawnCommandContext.h'
C = ROOT + '/Private/DawnCommandContext.cpp'
def patch(path, reps, tag):
    s = open(path).read()
    if 'STICKYUB-1' in s: print('already patched', path); return
    bak = path + '.wndrbak_stickyub'
    if not os.path.exists(bak): shutil.copy2(path, bak)
    for old, new, cnt in reps:
        assert s.count(old) == cnt, (path, old[:70], s.count(old))
        s = s.replace(old, new)
    open(path, 'w').write(s); print('patched', path)

patch(H, [(
'	TArray<TRefCountPtr<FRHIUniformBuffer>> PendingUBRefs;\n',
'	TArray<TRefCountPtr<FRHIUniformBuffer>> PendingUBRefs;\n'
'	TMap<uint32, TRefCountPtr<FRHIUniformBuffer>> StickyUBBySlot; // wndr STICKYUB-1: (frequency<<16|slot) -> last delivered UB, reset on PSO change\n'
'	int32 PendingUBStickyBase = 0; // wndr STICKYUB-1: PendingUBRefs[0..base) are re-seeded sticky entries\n', 1)], 'h')

seed_old = 'WndrFrameHeldUBs.Append(PendingUBRefs); PendingUBRefs.Reset();'
seed_new = ('for (int32 _si = PendingUBStickyBase; _si < PendingUBRefs.Num(); ++_si) { WndrFrameHeldUBs.Add(PendingUBRefs[_si]); } '
            'PendingUBRefs.Reset(); for (const TPair<uint32, TRefCountPtr<FRHIUniformBuffer>>& _sp : StickyUBBySlot) { PendingUBRefs.Add(_sp.Value); } '
            'PendingUBStickyBase = PendingUBRefs.Num(); /* wndr STICKYUB-1 */')
patch(C, [
('	PendingUBRefs.Reset(); // UBRESTABLE\n',
 '	PendingUBRefs.Reset(); // UBRESTABLE\n	StickyUBBySlot.Reset(); PendingUBStickyBase = 0; // wndr STICKYUB-1: UE resets its shader-binding state cache on PSO change\n', 1),
(seed_old, seed_new, 2),
('			if (PendingUBRefs.Num() == 0 || PendingUBRefs.Last().GetReference() != UB) { PendingUBRefs.Add(UB); }\n',
 '			if (PendingUBRefs.Num() == 0 || PendingUBRefs.Last().GetReference() != UB) { PendingUBRefs.Add(UB); }\n'
 '			if (Shader) { StickyUBBySlot.Add(((uint32)Shader->GetFrequency() << 16) | (uint32)Param.Index, TRefCountPtr<FRHIUniformBuffer>(UB)); } // wndr STICKYUB-1\n', 1),
], 'c')
