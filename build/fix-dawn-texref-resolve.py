#!/usr/bin/env python3
# TEXREF-fix-1: DawnRHI never resolved FRHITextureReference (default engine
# RHICreateTextureReference). Lightmap/shadowmap cluster textures and material
# UB textures are bound as references; static_cast<FDawnTexture*> on a reference
# read ->View past the object -> null/garbage -> black dummy. Resolve at bind time.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')
DR = os.path.join(ROOT, 'DawnDynamicRHI.cpp')

def patch(path, reps):
    s = open(path).read()
    if 'TEXREF-fix-1' in s:
        print('already patched', path); return
    bak = path + '.wndrbak_texref'
    if not os.path.exists(bak): shutil.copy2(path, bak)
    for old, new, cnt in reps:
        n = s.count(old)
        assert n == cnt, (path, old[:80], n, cnt)
        s = s.replace(old, new)
    open(path, 'w').write(s)
    print('patched', path)

HELPER = r'''#include "RHITextureReference.h"
// wndr TEXREF-fix-1: follow FRHITextureReference to the texture it points at.
static uint32 GWndrTexRefN = 0, GWndrTexRefResolved = 0, GWndrTexRefNull = 0;
static inline FRHITexture* DawnResolveTex(FRHITexture* T)
{
	if (!T) { return nullptr; }
	if (FRHITextureReference* R = T->GetTextureReference())
	{
		FRHITexture* Ref = R->GetReferencedTexture();
		++GWndrTexRefN; if (Ref) { ++GWndrTexRefResolved; } else { ++GWndrTexRefNull; }
		if (GWndrTexRefN <= 4 || (GWndrTexRefN % 20000) == 0) { printf("WNDR TEXREF n=%u resolved=%u null=%u\n", GWndrTexRefN, GWndrTexRefResolved, GWndrTexRefNull); fflush(stdout); }
		return Ref;
	}
	return T;
}
'''

patch(CC, [
    ('extern bool DawnLiveHandleCheck(void* H);\n',
     'extern bool DawnLiveHandleCheck(void* H);\n' + HELPER, 1),
    # UB resource table (lightmap cluster, material UB textures)
    ('if (TE.MemberType == (uint8)UBMT_TEXTURE) { Tex = static_cast<FDawnTexture*>((FRHITexture*)TE.Resource.GetReference()); }',
     'if (TE.MemberType == (uint8)UBMT_TEXTURE) { Tex = static_cast<FDawnTexture*>(DawnResolveTex((FRHITexture*)TE.Resource.GetReference())); } // wndr TEXREF-fix-1', 1),
    # LMMISS diag dimension probe
    ('Dim = (int)static_cast<FDawnTexture*>((FRHITexture*)TE.Resource.GetReference())->GetDesc().Dimension;',
     'FRHITexture* _rt = DawnResolveTex((FRHITexture*)TE.Resource.GetReference()); Dim = _rt ? (int)_rt->GetDesc().Dimension : -2;', 1),
    # direct texture parameter
    ('FDawnTexture* Tex = static_cast<FDawnTexture*>(Param.Resource);\n\t\t\tEntry.textureView = Tex->View;',
     'FDawnTexture* Tex = static_cast<FDawnTexture*>(DawnResolveTex(static_cast<FRHITexture*>(Param.Resource))); // wndr TEXREF-fix-1\n\t\t\tif (!Tex) { continue; } // empty slot -> dummy-fill\n\t\t\tEntry.textureView = Tex->View;', 1),
])

patch(DR, [
    ('#include "DataDrivenShaderPlatformInfo.h"\n',
     '#include "DataDrivenShaderPlatformInfo.h"\n#include "RHITextureReference.h" // wndr TEXREF-fix-1\n', 1),
    ('FDawnTexture* Tex = static_cast<FDawnTexture*>(static_cast<FRHITexture*>(Resource));\n\t\tSRV->TextureView',
     'FRHITexture* _rt = static_cast<FRHITexture*>(Resource); if (_rt && _rt->GetTextureReference()) { _rt = _rt->GetTextureReference()->GetReferencedTexture(); } // wndr TEXREF-fix-1\n\t\tFDawnTexture* Tex = static_cast<FDawnTexture*>(_rt);\n\t\tSRV->TextureView', 1),
])
