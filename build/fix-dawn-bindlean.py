#!/usr/bin/env python3
# BINDLEAN-1 (perf): the per-draw bind walk (FlushPendingBindGroups) spent ~158 ms/frame
# (~49 us/draw x 3200 draws). Cut per-draw work that does not change binding results:
#  - cache the per-group reflected layout on the PSO (was rebuilt as a TMap every draw)
#  - SKINDIAG scanned the whole ~35 KB VS WGSL text (Contains) every draw -> off
#  - HUDDIAG-7 / PRIMSRC / FOGSRC / BONEDIAG / DUMMYHIST / LMCOUNT / MATMISS string work -> off
import sys, shutil, os
ROOT = '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI'
H = ROOT + '/Public/DawnResources.h'
C = ROOT + '/Private/DawnCommandContext.cpp'
def patch(path, reps):
    s = open(path).read()
    if 'BINDLEAN-1' in s: print('already patched', path); return
    bak = path + '.wndrbak_bindlean'
    if not os.path.exists(bak): shutil.copy2(path, bak)
    for old, new in reps:
        assert s.count(old) == 1, (path, old[:80], s.count(old))
        s = s.replace(old, new)
    open(path, 'w').write(s); print('patched', path)

patch(H, [(
'	TArray<WGPUBindGroupLayout> BindGroupLayouts;\n',
'	TArray<WGPUBindGroupLayout> BindGroupLayouts;\n'
'	// wndr BINDLEAN-1: per-group reflected VS+PS layout, built once on first draw.\n'
'	mutable TMap<uint32, TArray<FDawnShaderBinding>> WndrLayoutByGroup;\n'
'	mutable bool bWndrLayoutBuilt = false;\n')])

patch(C, [
# layout cache
('''	TMap<uint32, TArray<FDawnShaderBinding>> LayoutByGroup;
	auto AddLayout = [&](const TArray<FDawnShaderBinding>& Bs)''',
'''	TMap<uint32, TArray<FDawnShaderBinding>>& LayoutByGroup = CurrentPSO->WndrLayoutByGroup; // wndr BINDLEAN-1
	auto AddLayout = [&](const TArray<FDawnShaderBinding>& Bs)'''),
('''	if (CurrentPSO->VertexShader) { AddLayout(CurrentPSO->VertexShader->Bindings); }
	if (CurrentPSO->PixelShader)  { AddLayout(CurrentPSO->PixelShader->Bindings); }
	WNDR_STAGE(12);''',
'''	if (!CurrentPSO->bWndrLayoutBuilt) // wndr BINDLEAN-1
	{
		if (CurrentPSO->VertexShader) { AddLayout(CurrentPSO->VertexShader->Bindings); }
		if (CurrentPSO->PixelShader)  { AddLayout(CurrentPSO->PixelShader->Bindings); }
		CurrentPSO->bWndrLayoutBuilt = true;
	}
	WNDR_STAGE(12);'''),
# diagnostics off
('		if (CurrentPSO->VertexShader && CurrentPSO->VertexShader->WGSLSource.Contains(TEXT("GPUSkinVFBase"))) // wndr SKINDIAG-1',
 '		if (false && CurrentPSO->VertexShader && CurrentPSO->VertexShader->WGSLSource.Contains(TEXT("GPUSkinVFBase"))) // wndr SKINDIAG-1 (off: BINDLEAN-1)'),
('		if (CurrentPSO->PixelShader) // wndr HUDDIAG-7',
 '		if (false && CurrentPSO->PixelShader) // wndr HUDDIAG-7 (off: BINDLEAN-1)'),
('			if (B.Kind == EDawnShaderBindingKind::UniformBuffer && B.Name == TEXT("MobileBasePass"))',
 '			if (GDawnVerboseBindLog && B.Kind == EDawnShaderBindingKind::UniformBuffer && B.Name == TEXT("MobileBasePass")) // BINDLEAN-1'),
('			if (B.Kind == EDawnShaderBindingKind::UniformBuffer && B.Name == TEXT("Primitive"))',
 '			if (GDawnVerboseBindLog && B.Kind == EDawnShaderBindingKind::UniformBuffer && B.Name == TEXT("Primitive")) // BINDLEAN-1'),
('				if (B.Name.Contains(TEXT("BoneMatrices"))) // wndr BONEDIAG-1',
 '				if (false && B.Name.Contains(TEXT("BoneMatrices"))) // wndr BONEDIAG-1 (off: BINDLEAN-1)'),
('				if (!bResFilled && B.Name.StartsWith(TEXT("Material_"), ESearchCase::CaseSensitive))\n				{\n					static uint32 _mm2 = 0; ++_mm2;',
 '				if (GDawnVerboseBindLog && !bResFilled && B.Name.StartsWith(TEXT("Material_"), ESearchCase::CaseSensitive)) // BINDLEAN-1\n				{\n					static uint32 _mm2 = 0; ++_mm2;'),
('				if (bFilled && B.Kind != EDawnShaderBindingKind::Sampler && B.Kind != EDawnShaderBindingKind::UniformBuffer) // wndr DUMMYHIST-1',
 '				if (false && bFilled && B.Kind != EDawnShaderBindingKind::Sampler && B.Kind != EDawnShaderBindingKind::UniformBuffer) // wndr DUMMYHIST-1 (off: BINDLEAN-1)'),
('''static void WndrLMCount(const FString& Name, int Src)
{
''',
'''static void WndrLMCount(const FString& Name, int Src)
{
	if (!GDawnVerboseBindLog) { return; } // wndr BINDLEAN-1
'''),
])
