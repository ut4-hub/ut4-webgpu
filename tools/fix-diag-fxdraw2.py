#!/usr/bin/env python3
# FXDIAG-2: diag filter -> Link projectile materials only; LMSIZE-1: log each distinct
# lightmap texture bound (name/format/size/mips) to check the patchy-lighting theory.
P='/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/'
MP=P+'Renderer/Private/MeshPassProcessor.cpp'; CC=P+'DawnRHI/Private/DawnCommandContext.cpp'; DR=P+'DawnRHI/Private/DawnDynamicRHI.cpp'
def rep(p, o, n):
    s=open(p).read(); assert s.count(o)==1,(p,o[:60],s.count(o)); open(p,'w').write(s.replace(o,n))
rep(MP, '''(N.Contains(TEXT("LinkProjectile")) || N.Contains(TEXT("Fireball")) || N.Contains(TEXT("explosion_base")) || N.Contains(TEXT("Linkgun_Soft")))''',
        '''(N.Contains(TEXT("LinkProjectile")) || N.Contains(TEXT("LinkArcs")) || N.Contains(TEXT("LinkGun_muzzle")) || N.Contains(TEXT("LinkGun_MF"))) /* FXDIAG-2 */''')
rep(DR, '''WndrViewInfoAdd((void*)NewTexture->View, (int32)TexDesc.format, (int32)TexDesc.size.width, (int32)TexDesc.size.height, CreateDesc.DebugName ? CreateDesc.DebugName : TEXT("?"));''',
        '''WndrViewInfoAdd((void*)NewTexture->View, (int32)TexDesc.format, (int32)TexDesc.size.width, (int32)TexDesc.size.height | ((int32)TexDesc.mipLevelCount << 20), CreateDesc.DebugName ? CreateDesc.DebugName : TEXT("?"));''')
rep(CC, '''	GWndrViewInfo.Add(V, FString::Printf(TEXT("%s fmt=%d %dx%d"), Name, Fmt, W, H));''',
        '''	GWndrViewInfo.Add(V, FString::Printf(TEXT("%s fmt=%d %dx%d mips=%d"), Name, Fmt, W, H & 0xFFFFF, H >> 20));''')
rep(CC, '''		if (GWndrDbgBits) // wndr LMTOGGLE-1
		{''', '''		{ // wndr LMSIZE-1
			static TSet<void*> _lms;
			if (_lms.Num() < 40)
				for (const WGPUBindGroupEntry& E : Entries)
				{
					if (!E.textureView || _lms.Contains((void*)E.textureView)) { continue; }
					const FDawnShaderBinding* NB = nullptr; for (const FDawnShaderBinding& B : LG.Value) { if (B.Binding == E.binding) { NB = &B; break; } }
					if (NB && NB->Name.StartsWith(TEXT("LightmapResourceCluster_"), ESearchCase::CaseSensitive))
					{
						_lms.Add((void*)E.textureView);
						printf("WNDR LMSIZE %s : %s\\n", TCHAR_TO_UTF8(*NB->Name), TCHAR_TO_UTF8(*WndrViewDesc((void*)E.textureView))); fflush(stdout);
					}
				}
		}
		if (GWndrDbgBits) // wndr LMTOGGLE-1
		{''')
print('ok')
