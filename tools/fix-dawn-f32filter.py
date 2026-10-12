#!/usr/bin/env python3
# F32FILTER-1: request WebGPU 'float32-filterable' when the adapter has it, and stop
# rejecting RGBA32F/RG32F textures on filterable-float slots (BINDFIX 'sampletype').
# Cascade GPU-sprite particles (Link Gun plasma etc.) sample their RGBA32F
# PositionTexture -> was dummy-filled -> particles invisible.
import sys, shutil, os
R = '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private/'
def patch(path, reps):
    s = open(path).read()
    if 'F32FILTER-1' in s: print('already patched', path); return
    bak = path + '.wndrbak_f32filter'
    if not os.path.exists(bak): shutil.copy2(path, bak)
    for old, new in reps:
        assert s.count(old) == 1, (path, old[:70], s.count(old))
        s = s.replace(old, new)
    open(path, 'w').write(s); print('patched', path)
patch(R + 'DawnDynamicRHI.cpp', [
('bool GDawnBCTextureSupport = false;\n', 'bool GDawnBCTextureSupport = false;\nbool GDawnFloat32Filterable = false; // wndr F32FILTER-1\n'),
('	static WGPUFeatureName GRequiredFeatures[4];\n', '	static WGPUFeatureName GRequiredFeatures[8];\n'),
('	DeviceDesc.requiredFeatureCount = NumRequiredFeatures;\n',
 '	if (wgpuAdapterHasFeature(Adapter, WGPUFeatureName_Float32Filterable)) { GRequiredFeatures[NumRequiredFeatures++] = WGPUFeatureName_Float32Filterable; GDawnFloat32Filterable = true; } // wndr F32FILTER-1\n'
 '	printf("DAWNBG FEATURES float32filterable=%d\\n", GDawnFloat32Filterable ? 1 : 0);\n'
 '	DeviceDesc.requiredFeatureCount = NumRequiredFeatures;\n'),
])
patch(R + 'DawnCommandContext.cpp', [
('extern bool DawnLiveHandleCheck(void* H);\n', 'extern bool DawnLiveHandleCheck(void* H);\nextern bool GDawnFloat32Filterable; // wndr F32FILTER-1 (DawnDynamicRHI.cpp)\n'),
('					else if (RB->TexSampleType == WGPUTextureSampleType_Float && (bDepthFmt || bFloat32Fmt)) { Why = "sampletype"; }',
 '					else if (RB->TexSampleType == WGPUTextureSampleType_Float && (bDepthFmt || (bFloat32Fmt && !(GDawnFloat32Filterable && TD.Format != PF_R32_FLOAT)))) { Why = "sampletype"; } // wndr F32FILTER-1'),
])
