#!/usr/bin/env python3
# RHISTAGE-probe-2: per-stage TIME accumulation (ms spent since each mark) +
# finer marks inside the UB resource-table matching. Exported wndr_rhi_stage_ms(i).
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')
s = open(CC).read()
if 'RHISTAGE-probe-2' in s:
    print('already patched'); sys.exit(0)
assert 'RHISTAGE-probe-1' in s
bak = CC + '.wndrbak_rhistage2'
if not os.path.exists(bak): shutil.copy2(CC, bak)
reps = [
('#define WNDR_STAGE(n) GWndrRhiStage.store((uint32)(n), std::memory_order_relaxed)\n',
'''// wndr RHISTAGE-probe-2: accumulate wall ms spent after each mark (RHI thread only).
static double GWndrStageMs[128] = {}; static double GWndrStageT = 0.0; static uint32 GWndrPrevStage = 0;
static inline void WndrStageMark(uint32 n)
{
#if PLATFORM_WASM
	const double t = emscripten_get_now();
	if (GWndrStageT > 0.0) { GWndrStageMs[GWndrPrevStage & 127] += t - GWndrStageT; }
	GWndrStageT = t; GWndrPrevStage = n;
#endif
	GWndrRhiStage.store(n, std::memory_order_relaxed);
}
#define WNDR_STAGE(n) WndrStageMark((uint32)(n))
#if PLATFORM_WASM
extern "C" EMSCRIPTEN_KEEPALIVE double wndr_rhi_stage_ms(int i) { return GWndrStageMs[i & 127]; }
#endif
'''),
('	for (const TPair<uint32, TArray<FDawnShaderBinding>>& LG : LayoutByGroup)\n',
 '	WNDR_STAGE(12);\n	for (const TPair<uint32, TArray<FDawnShaderBinding>>& LG : LayoutByGroup)\n'),
('					const FString Suffix = B.Name.Mid(DUB->ShaderName.Len() + 1);\n',
 '					WNDR_STAGE(23);\n					const FString Suffix = B.Name.Mid(DUB->ShaderName.Len() + 1);\n'),
('					return (E.textureView != nullptr) || (E.sampler != nullptr) || (E.buffer != nullptr);\n',
 '					WNDR_STAGE(24);\n					return (E.textureView != nullptr) || (E.sampler != nullptr) || (E.buffer != nullptr);\n'),
("				// wndr LM-fix-2: LightmapResourceCluster_* via this draw's delivered PLB.\n",
 "				WNDR_STAGE(26);\n				// wndr LM-fix-2: LightmapResourceCluster_* via this draw's delivered PLB.\n"),
('				// MATMISS diag: a Material_* binding about to fall to the dummy —\n',
 '				WNDR_STAGE(27);\n				// MATMISS diag: a Material_* binding about to fall to the dummy —\n'),
('				if (bResFilled)\n				{\n					static uint32 _uf = 0; ++_uf;\n',
 '				WNDR_STAGE(28);\n				if (bResFilled)\n				{\n					static uint32 _uf = 0; ++_uf;\n'),
('			// Step 3 (fallback-material visibility): neither the draw\'s own\n',
 '			WNDR_STAGE(32);\n			// Step 3 (fallback-material visibility): neither the draw\'s own\n'),
]
for old, new in reps:
    assert s.count(old) == 1, (old[:70], s.count(old))
    s = s.replace(old, new)
open(CC, 'w').write(s)
print('patched', CC)
