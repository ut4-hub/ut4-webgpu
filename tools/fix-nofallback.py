#!/usr/bin/env python3
# NOFALLBACK-1 ("bricks" at characters' feet): M_Robust_BlobShadow (BaseUTCharacter blob shadow),
# M_UDamage_Overlay, ShieldBelt, ... have cooked shader maps that fail to load on this platform
# ("Invalid shader map ID ... will use default material") -> drawn with the default WorldGrid
# material. On wasm, skip the draw instead of falling back. Logs the skipped materials.
import os, shutil
R='/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/Renderer/Private/'
OLD='''		MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);'''
NEW='''		MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
#if PLATFORM_WASM
		{ // wndr NOFALLBACK-1: never draw the default-material fallback (grid "bricks")
			static uint32 _nf = 0;
			if (_nf < 40 && MeshBatch.MaterialRenderProxy) { ++_nf; printf("WNDR NOFALLBACK skip mat=%s\\n", TCHAR_TO_UTF8(*MeshBatch.MaterialRenderProxy->GetFriendlyName())); fflush(stdout); }
			break;
		}
#endif'''
for f in ('MobileBasePass.cpp','DepthRendering.cpp','CustomDepthRendering.cpp'):
    p=R+f; s=open(p).read()
    if 'NOFALLBACK-1' in s: print('already',f); continue
    n=s.count(OLD); print(f, 'occurrences', n)
    bak=p+'.wndrbak_nofallback'
    if not os.path.exists(bak): shutil.copy2(p,bak)
    open(p,'w').write(s.replace(OLD,NEW))
