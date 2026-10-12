#!/usr/bin/env python3
# HUDDIAG-3: canvas pipelines are sane; check the loose $Globals path. Dump the
# canvas VS/PS reflected bindings, and for the first canvas-PSO parameter sets the
# $Globals BufferIndex -> size + first floats (Transform matrix / replicate weights).
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

patch(DR, 'HUDDIAG-3', [(
'''			DumpLines(VS->WGSLSource, TEXT("VS"), TEXT("fn Main("), TEXT("@location"));''',
'''			for (const FDawnShaderBinding& Bd : VS->Bindings) { printf("WNDR HUDPSO #%u VSBIND g=%u b=%u kind=%d name=%s\\n", _hc, Bd.Group, Bd.Binding, (int)Bd.Kind, TCHAR_TO_UTF8(*Bd.Name)); } // wndr HUDDIAG-3
			if (PS) { for (const FDawnShaderBinding& Bd : PS->Bindings) { printf("WNDR HUDPSO #%u PSBIND g=%u b=%u kind=%d name=%s\\n", _hc, Bd.Group, Bd.Binding, (int)Bd.Kind, TCHAR_TO_UTF8(*Bd.Name)); } }
			DumpLines(VS->WGSLSource, TEXT("VS"), TEXT("fn Main("), TEXT("@location"));''')], '.wndrbak_huddiag3')

patch(CC, 'HUDDIAG-3', [(
'''			WGPUBufferDescriptor BD = {};
			BD.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
			BD.size = CbSize;''',
'''			{ // wndr HUDDIAG-3
				bool bCv = false;
				for (const FDawnShaderBinding& Bd : (Shader->GetFrequency() == SF_Pixel ? (CurrentPSO && CurrentPSO->PixelShader ? CurrentPSO->PixelShader->Bindings : TArray<FDawnShaderBinding>()) : TArray<FDawnShaderBinding>())) { if (Bd.Name == TEXT("InTexture")) { bCv = true; break; } }
				if (!bCv && CurrentPSO && CurrentPSO->PixelShader) { for (const FDawnShaderBinding& Bd : CurrentPSO->PixelShader->Bindings) { if (Bd.Name == TEXT("InTexture")) { bCv = true; break; } } }
				static uint32 _hg = 0;
				if (bCv && _hg++ < 12)
				{
					const float* F = (const float*)Cpu.GetData();
					printf("WNDR HUDGLOBALS #%u freq=%d bufIdx=%u used=%u cb=%u nParams=%d f=[%.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f]\\n", _hg, (int)Shader->GetFrequency(), (uint32)BS.Key, BS.Value, CbSize, InParameters.Num(),
						F[0],F[1],F[2],F[3],F[4],F[5],F[6],F[7],F[8],F[9],F[10],F[11],F[12],F[13],F[14],F[15]);
					for (const FRHIShaderParameter& P : InParameters) { printf("WNDR HUDGLOBALS #%u   param buf=%u base=%u size=%u srcOff=%u\\n", _hg, (uint32)P.BufferIndex, (uint32)P.BaseIndex, (uint32)P.ByteSize, (uint32)P.ByteOffset); }
					fflush(stdout);
				}
			}
			WGPUBufferDescriptor BD = {};
			BD.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
			BD.size = CbSize;''')], '.wndrbak_huddiag3')
