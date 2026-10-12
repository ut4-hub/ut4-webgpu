#!/usr/bin/env python3
# SKINDIAG-1: characters render as blocks. (a) full GPUSkin VS Main() signature +
# BoneMatrices lines; (b) for the first GPUSkin-PSO flushes, every wanted binding
# and what is pending for it (real handle / missing -> dummy).
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
DR = os.path.join(ROOT, 'DawnDynamicRHI.cpp')
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')

def patch(path, mark, reps, bak_suffix):
    s = open(path).read()
    if mark in s:
        print('already patched', path); return
    bak = path + bak_suffix
    if not os.path.exists(bak): shutil.copy2(path, bak)
    for old, new in reps:
        assert s.count(old) == 1, (path, old[:80], s.count(old))
        s = s.replace(old, new)
    open(path, 'w').write(s)
    print('patched', path)

patch(DR, 'SKINDIAG-1', [(
'''					if (Ln.Contains(TEXT("@location")) || Ln.Contains(TEXT("in_var_ATTRIBUTE")) || Ln.Contains(TEXT("ATTRIBUTE"))) { printf("WNDR SKINWGSL #%u %s\\n", _gsk, TCHAR_TO_UTF8(*Ln.TrimStartAndEnd().Left(200))); ++Nl; }''',
'''					if (Ln.Contains(TEXT("fn Main(")) || Ln.Contains(TEXT("BoneMatri")) || Ln.Contains(TEXT("InputWeight")) || Ln.Contains(TEXT("@binding"))) // wndr SKINDIAG-1
					{ const FString T = Ln.TrimStartAndEnd(); for (int32 Ci = 0; Ci < T.Len() && Ci < 1200; Ci += 300) { printf("WNDR SKINWGSL #%u %s%s\\n", _gsk, Ci ? "  ..." : "", TCHAR_TO_UTF8(*T.Mid(Ci, 300))); } ++Nl; }''')], '.wndrbak_skindiag')

patch(CC, 'SKINDIAG-1', [(
'''		if (CurrentPSO->PixelShader) // wndr HUDDIAG-7''',
'''		if (CurrentPSO->VertexShader && CurrentPSO->VertexShader->WGSLSource.Contains(TEXT("GPUSkinVFBase"))) // wndr SKINDIAG-1
		{
			static uint32 _sk = 0;
			if (_sk++ < 6)
			{
				printf("WNDR SKINPEND #%u grp=%u", _sk, Group);
				for (const FDawnShaderBinding& B : LG.Value)
				{
					const WGPUBindGroupEntry* PE = Pending ? Pending->Find(B.Binding) : nullptr;
					const void* H = PE ? (PE->buffer ? (void*)PE->buffer : (PE->textureView ? (void*)PE->textureView : (void*)PE->sampler)) : nullptr;
					printf(" | %u:%s k=%d %s%s sz=%llu", B.Binding, TCHAR_TO_UTF8(*B.Name), (int)B.Kind, PE ? "PEND" : "none", (PE && !DawnLiveHandleCheck((void*)H)) ? "(NOTLIVE)" : "", PE ? (unsigned long long)PE->size : 0ull);
				}
				printf("\\n"); fflush(stdout);
			}
		}
		if (CurrentPSO->PixelShader) // wndr HUDDIAG-7''')], '.wndrbak_skindiag')
