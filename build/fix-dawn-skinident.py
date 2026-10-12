#!/usr/bin/env python3
# SKINIDENT-1 (diagnostic A/B): when Module.wndrSkinIdent is set (index.html
# ?skinident=1), GPU-skin VSs read an identity bone matrix instead of the bone
# buffer -> meshes draw in bind pose. Also dumps the skin vertex layout.
import sys, shutil, os
P = '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private/DawnDynamicRHI.cpp'
s = open(P).read()
if 'SKINIDENT-1' in s:
    print('already patched'); sys.exit(0)
bak = P + '.wndrbak_skinident'
if not os.path.exists(bak): shutil.copy2(P, bak)
old = '''	Shader->ShaderModule = CompileWGSL(Device, CreateShaderDesc, TEXT("DawnRHI.VertexShader"));
	checkf(Shader->ShaderModule, TEXT("DawnRHI: vertex shader WGSL compile failed"));'''
new = '''	// wndr SKINIDENT-1
	TArray<uint8> _SkinIdentUtf8;
	{
		static const TCHAR* Tok = TEXT("GPUSkinVFBase_Common_BoneMatrices.tint_symbol[");
		static int32 bIdent = -1;
		if (bIdent < 0) { bIdent = EM_ASM_INT({ return (Module.wndrSkinIdent | 0); }); printf("WNDR SKINIDENT mode=%d\\n", bIdent); fflush(stdout); }
		FString& W = Shader->WGSLSource;
		if (bIdent && W.Contains(Tok, ESearchCase::CaseSensitive))
		{
			const int32 TokLen = FCString::Strlen(Tok);
			FString Out; Out.Reserve(W.Len() + 512);
			int32 i = 0, NRep = 0;
			while (true)
			{
				const int32 Idx = W.Find(Tok, ESearchCase::CaseSensitive, ESearchDir::FromStart, i);
				if (Idx == INDEX_NONE) { Out += W.Mid(i); break; }
				Out += W.Mid(i, Idx - i); Out += TEXT("wndrBone(");
				int32 j = Idx + TokLen, Depth = 1;
				for (; j < W.Len(); ++j) { if (W[j] == TEXT('[')) ++Depth; else if (W[j] == TEXT(']')) { if (--Depth == 0) break; } }
				Out += W.Mid(Idx + TokLen, j - (Idx + TokLen)); Out += TEXT(")");
				i = j + 1; ++NRep;
			}
			const int32 FnAt = Out.Find(TEXT("\\nfn "), ESearchCase::CaseSensitive);
			const FString Helper = TEXT("\\nfn wndrBone(i : u32) -> vec4<f32> {\\n  let k = i % 3u;\\n  let z = GPUSkinVFBase_Common_BoneMatrices.tint_symbol[0u] * 0.0f;\\n  if (k == 0u) { return z + vec4<f32>(1.0f, 0.0f, 0.0f, 0.0f); }\\n  if (k == 1u) { return z + vec4<f32>(0.0f, 1.0f, 0.0f, 0.0f); }\\n  return z + vec4<f32>(0.0f, 0.0f, 1.0f, 0.0f);\\n}\\n");
			if (FnAt != INDEX_NONE) { Out.InsertAt(FnAt, Helper); }
			W = Out;
			FTCHARToUTF8 U(*W); _SkinIdentUtf8.Append(reinterpret_cast<const uint8*>(U.Get()), U.Length());
			static uint32 _si = 0; if (_si++ < 4) { printf("WNDR SKINIDENT rewrote VS reps=%d len=%d\\n", NRep, W.Len()); fflush(stdout); }
		}
	}
	Shader->ShaderModule = CompileWGSL(Device, CreateShaderDesc, TEXT("DawnRHI.VertexShader"), false, _SkinIdentUtf8.Num() ? &_SkinIdentUtf8 : nullptr);
	checkf(Shader->ShaderModule, TEXT("DawnRHI: vertex shader WGSL compile failed"));'''
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
old2 = '''			for (const TPair<uint32, uint32>& P : AttrToLoc) { printf(" A%u->L%u", P.Key, P.Value); }
			printf("\\n"); fflush(stdout);'''
new2 = '''			for (const TPair<uint32, uint32>& P : AttrToLoc) { printf(" A%u->L%u", P.Key, P.Value); }
			printf("\\n");
			for (uint32 S = 0; S < MaxVertexStreams; ++S)
			{
				if (StreamAttributes[S].Num() == 0) { continue; }
				printf("WNDR SKINLAYOUT #%u strm=%u stride=%llu inst=%d:", _sl, S, (unsigned long long)StreamStrides[S], (int)StreamInstanced[S]);
				for (const WGPUVertexAttribute& A : StreamAttributes[S]) { printf(" [L%u fmt=0x%x off=%llu]", (uint32)A.shaderLocation, (uint32)A.format, (unsigned long long)A.offset); }
				printf("\\n");
			}
			fflush(stdout);'''
assert s.count(old2) == 1, s.count(old2)
s = s.replace(old2, new2)
if '#include <emscripten.h>' not in s and '#include <emscripten/emscripten.h>' not in s:
    s = s.replace('#include "RHITextureReference.h" // wndr TEXREF-fix-1\n', '#include "RHITextureReference.h" // wndr TEXREF-fix-1\n#include <emscripten.h> // wndr SKINIDENT-1\n', 1)
open(P, 'w').write(s)
print('patched OK')
