#!/usr/bin/env python3
# HUDDIAG-6: HUD vertices are correct (white, alpha 1, on-screen) -> the texture
# sample must be alpha 0. Print each canvas InTexture: name, format, size, mips,
# flags; and once, the canvas PS WGSL body (how the sample is used).
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')
s = open(CC).read()
if 'HUDDIAG-6' in s:
    print('already patched'); sys.exit(0)
bak = CC + '.wndrbak_huddiag6'
if not os.path.exists(bak): shutil.copy2(CC, bak)
old = '''			if (DAWNRHI_VERBOSE_DIAG) { static uint32 _tc=0; if(_tc++<24){ const FRHITextureDesc& D=Tex->GetDesc(); UE_LOG(LogDawnRHI, Warning, TEXT("TEXBIND'''
new = '''			if (CurrentPSO && CurrentPSO->PixelShader) // wndr HUDDIAG-6
			{
				const FDawnShaderBinding* HB = nullptr;
				for (const FDawnShaderBinding& Bi : CurrentPSO->PixelShader->Bindings) { if (Bi.Binding == Param.Index && Bi.Name == TEXT("InTexture")) { HB = &Bi; break; } }
				static uint32 _ht = 0;
				if (HB && _ht++ < 40)
				{
					const FRHITextureDesc& D = Tex->GetDesc();
					printf("WNDR HUDTEX #%u name=%s fmt=%d %ux%u mips=%u flags=0x%llx view=%p\\n", _ht, TCHAR_TO_UTF8(*Tex->GetName().ToString()), (int)D.Format, D.Extent.X, D.Extent.Y, (uint32)D.NumMips, (unsigned long long)D.Flags, (void*)Tex->View);
					if (_ht == 1)
					{
						const FString& W = CurrentPSO->PixelShader->WGSLSource;
						int32 Fp = W.Find(TEXT("fn Main"), ESearchCase::CaseSensitive);
						FString Body = W.Mid(FMath::Max(0, Fp - 1400), 2600); Body.ReplaceInline(TEXT("\\n"), TEXT(" | "));
						printf("WNDR HUDPSWGSL %s\\n", TCHAR_TO_UTF8(*Body));
					}
					fflush(stdout);
				}
			}
			if (DAWNRHI_VERBOSE_DIAG) { static uint32 _tc=0; if(_tc++<24){ const FRHITextureDesc& D=Tex->GetDesc(); UE_LOG(LogDawnRHI, Warning, TEXT("TEXBIND'''
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
open(CC, 'w').write(s)
print('patched', CC)
