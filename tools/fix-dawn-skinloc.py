#!/usr/bin/env python3
# SKINLOC-1: DXC numbers VS inputs densely in HLSL DECLARATION order and the
# ATTRIBUTEn semantics are gone in the WGSL. The RHI placed every attribute at
# shaderLocation = UE AttributeIndex, which only works when declaration order is
# ascending (LocalVF). GpuSkinVertexFactory.ush declares 0,1,2,3,14,4,15,5,(6),
# (9,10),13 -> shader loc4 = BlendIndicesExtra(14), loc5 = BlendWeights(4),
# loc6 = BlendWeightsExtra(15), loc7 = TexCoords0(5). Feeding texcoords as bone
# weights wrecked every skinned mesh (characters = blocks, no 1st-person weapon).
# Remap skin-VF attributes to the shader's dense locations in declaration order.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
DR = os.path.join(ROOT, 'DawnDynamicRHI.cpp')
s = open(DR).read()
if 'SKINLOC-1' in s:
    print('already patched'); sys.exit(0)
bak = DR + '.wndrbak_skinloc'
if not os.path.exists(bak): shutil.copy2(DR, bak)
old = '''	// VTXPADFIX (frame-3 stall): Dawn requires every attribute location the VS'''
new = '''	// wndr SKINLOC-1: GPU-skin VF inputs are densely renumbered in HLSL declaration order.
	if (VertexDecl && VS && !VS->WGSLSource.IsEmpty() && VS->WGSLSource.Contains(TEXT("GPUSkinVFBase")))
	{
		TMap<uint32, WGPUVertexFormat> SI;
		ParseWgslVertexInputs(VS->WGSLSource, SI);
		TArray<uint32> Locs; SI.GenerateKeyArray(Locs); Locs.Sort();
		TSet<uint32> Present;
		for (uint32 S = 0; S < MaxVertexStreams; ++S) { for (const WGPUVertexAttribute& A : StreamAttributes[S]) { Present.Add((uint32)A.shaderLocation); } }
		auto IsUintFmt = [](WGPUVertexFormat F) { return F == WGPUVertexFormat_Uint32 || F == WGPUVertexFormat_Uint32x2 || F == WGPUVertexFormat_Uint32x3 || F == WGPUVertexFormat_Uint32x4; };
		auto Comps = [](WGPUVertexFormat F) -> int
		{
			switch (F)
			{
			case WGPUVertexFormat_Float32x4: case WGPUVertexFormat_Uint32x4: case WGPUVertexFormat_Sint32x4: return 4;
			case WGPUVertexFormat_Float32x3: case WGPUVertexFormat_Uint32x3: case WGPUVertexFormat_Sint32x3: return 3;
			case WGPUVertexFormat_Float32x2: case WGPUVertexFormat_Uint32x2: case WGPUVertexFormat_Sint32x2: return 2;
			default: return 1;
			}
		};
		static const uint32 Order[] = { 0, 1, 2, 3, 14, 4, 15, 5, 6, 9, 10, 13 }; // GpuSkinVertexFactory.ush FVertexFactoryInput
		TMap<uint32, uint32> AttrToLoc;
		int32 Li = 0;
		for (uint32 A : Order)
		{
			if (Li >= Locs.Num()) { break; }
			if (!Present.Contains(A)) { continue; }
			const WGPUVertexFormat Want = SI.FindRef(Locs[Li]);
			const bool bAttrUint = (A == 3 || A == 14);
			if (IsUintFmt(Want) != bAttrUint)
			{
				if (A == 14 || A == 15) { continue; } // optional extra influences stripped from this variant
			}
			if (A == 15 && Comps(Want) != 4) { continue; }
			AttrToLoc.Add(A, Locs[Li++]);
		}
		for (uint32 S = 0; S < MaxVertexStreams; ++S)
		{
			TArray<WGPUVertexAttribute>& Attrs = StreamAttributes[S];
			for (int32 i = Attrs.Num() - 1; i >= 0; --i)
			{
				if (const uint32* L = AttrToLoc.Find((uint32)Attrs[i].shaderLocation)) { Attrs[i].shaderLocation = *L; }
				else { Attrs.RemoveAt(i); }
			}
		}
		static uint32 _sl = 0;
		if (_sl++ < 4)
		{
			printf("WNDR SKINLOC #%u shaderLocs=%d map:", _sl, Locs.Num());
			for (const TPair<uint32, uint32>& P : AttrToLoc) { printf(" A%u->L%u", P.Key, P.Value); }
			printf("\\n"); fflush(stdout);
		}
	}
	// VTXPADFIX (frame-3 stall): Dawn requires every attribute location the VS'''
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
open(DR, 'w').write(s)
print('patched', DR)
