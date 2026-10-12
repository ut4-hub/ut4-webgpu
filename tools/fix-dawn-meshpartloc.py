#!/usr/bin/env python3
# MESHPARTLOC-1 (invisible Link Gun plasma: M_LinkProjectile_Core/Coma are Cascade MESH particles).
# MeshParticleVertexFactory.ush declares Position, TangentX, TangentZ, VertexColor,
# TexCoords0..N-1 (N = material's vertex texcoords, 0..4), Transform1..3, SubUVParams,
# SubUVLerpAndRelTime, [DynamicParameter], ParticleColor, ParticleVelocity. DXC numbers them
# densely in that order, but the decl feeds them at ATTRIBUTE8..15 -> the per-particle
# transform landed on texcoord/garbage locations -> every mesh particle collapsed/offscreen.
# Remap: Transform1 = first vec4<f32> input at location >= 4 (texcoords are vec2), the rest
# follow; DynamicParameter present if a location >= T1+7 exists, else if the decl's A13
# stream has a real stride.
import os, shutil
DR = '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private/DawnDynamicRHI.cpp'
s = open(DR).read()
if 'MESHPARTLOC-1' in s:
    print('already patched'); raise SystemExit
bak = DR + '.wndrbak_meshpartloc'
if not os.path.exists(bak): shutil.copy2(DR, bak)
reps = [
('''static void ParseWgslVertexInputs(const FString& Wgsl, TMap<uint32, WGPUVertexFormat>& Out)
{''',
'''static void ParseWgslVertexInputs(const FString& Wgsl, TMap<uint32, WGPUVertexFormat>& Out, TMap<uint32, FString>* OutTypes = nullptr) // wndr MESHPARTLOC-1: OutTypes
{'''),
('''		Out.Add(Loc, BaseFmt(M.Mid(Colon + 1)));''',
'''		Out.Add(Loc, BaseFmt(M.Mid(Colon + 1)));
		if (OutTypes) { OutTypes->Add(Loc, M.Mid(Colon + 1).TrimStartAndEnd()); } // wndr MESHPARTLOC-1'''),
('''	// wndr SKIPDIAG-1 / INSTDIAG-1: per-instance-stream PSOs: VS inputs vs decl''',
'''	// wndr MESHPARTLOC-1: Cascade mesh-particle VS inputs (see fix-dawn-meshpartloc.py)
	if (VertexDecl && VS && !bWndrSkinVS && !bWndrSpriteVS && !VS->WGSLSource.IsEmpty() && VS->WGSLSource.Contains(TEXT("MeshParticleVF")))
	{
		bool bHasT1 = false; int32 DynStride = 0;
		for (const FVertexElement& Elem : VertexDecl->Elements) { if (Elem.AttributeIndex == 8) { bHasT1 = true; } if (Elem.AttributeIndex == 13) { DynStride = Elem.Stride; } }
		TMap<uint32, WGPUVertexFormat> SI; TMap<uint32, FString> ST;
		ParseWgslVertexInputs(VS->WGSLSource, SI, &ST);
		int32 T1 = -1, MaxLoc = -1;
		TArray<uint32> Locs; ST.GenerateKeyArray(Locs); Locs.Sort();
		for (uint32 L : Locs)
		{
			MaxLoc = FMath::Max<int32>(MaxLoc, (int32)L);
			const FString& Ty = ST[L];
			if (T1 < 0 && L >= 4 && (Ty.Contains(TEXT("vec4<f32>")) || Ty.Contains(TEXT("vec4f")))) { T1 = (int32)L; }
		}
		if (bHasT1 && T1 >= 4)
		{
			const int32 NTC = T1 - 4;
			const bool bDyn = (MaxLoc >= T1 + 7) || (DynStride > 0);
			TMap<uint32, uint32> AttrToLoc;
			for (uint32 A = 0; A < 4; ++A) { AttrToLoc.Add(A, A); }
			for (int32 k = 0; k < NTC; ++k) { AttrToLoc.Add(4 + k, 4 + k); }
			AttrToLoc.Add(8, T1); AttrToLoc.Add(9, T1 + 1); AttrToLoc.Add(10, T1 + 2); AttrToLoc.Add(11, T1 + 3); AttrToLoc.Add(12, T1 + 4);
			if (bDyn) { AttrToLoc.Add(13, T1 + 5); AttrToLoc.Add(14, T1 + 6); AttrToLoc.Add(15, T1 + 7); }
			else { AttrToLoc.Add(14, T1 + 5); AttrToLoc.Add(15, T1 + 6); }
			for (uint32 S = 0; S < MaxVertexStreams; ++S)
			{
				TArray<WGPUVertexAttribute>& Attrs = StreamAttributes[S];
				for (int32 i = Attrs.Num() - 1; i >= 0; --i)
				{
					if (const uint32* L = AttrToLoc.Find((uint32)Attrs[i].shaderLocation)) { Attrs[i].shaderLocation = *L; }
					else { Attrs.RemoveAt(i); }
				}
			}
			static uint32 _mp = 0;
			if (_mp++ < 12)
			{
				printf("WNDR MESHPARTLOC #%u T1=%d maxLoc=%d nTC=%d dyn=%d dynStride=%d inputs:", _mp, T1, MaxLoc, NTC, bDyn ? 1 : 0, DynStride);
				for (uint32 L : Locs) { printf(" L%u=%s", L, TCHAR_TO_UTF8(*ST[L])); }
				printf("\\n"); fflush(stdout);
			}
		}
	}
	// wndr SKIPDIAG-1 / INSTDIAG-1: per-instance-stream PSOs: VS inputs vs decl'''),
]
for o, n in reps:
    assert s.count(o) == 1, (o[:60], s.count(o))
    s = s.replace(o, n)
open(DR, 'w').write(s)
print('patched')
