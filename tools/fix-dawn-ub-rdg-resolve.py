#!/usr/bin/env python3
# RDGRES-fix-1: DawnCaptureUBResourceTable read every UB resource slot as a raw
# FRHIResource*. RDG members (UBMT_RDG_TEXTURE / _SRV / RDG_BUFFER_SRV, e.g. in
# FMobileSceneTextureUniformParameters, FMobileBasePassUniformParameters) hold
# FRDGTexture*/FRDGTextureSRV*/FRDGBufferSRV* instead -> TRefCountPtr AddRef'd /
# Released RDG allocator objects (freed + reused each graph) -> REFUNDERFLOW,
# use-after-free and the DeleteResources crashes. Resolve with the engine's
# GetShaderParameterResourceRHI. For the deferred (UBRACE) update, resolve on the
# render thread (RDG objects may be gone when the RHI thread runs) and hand the
# resolved, ref-held array to the capture.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
DR = os.path.join(ROOT, 'DawnDynamicRHI.cpp')

s = open(DR).read()
if 'RDGRES-fix-1' in s:
    print('already patched'); sys.exit(0)
bak = DR + '.wndrbak_rdgres'
if not os.path.exists(bak): shutil.copy2(DR, bak)
reps = [
(
'#include "RHITextureReference.h" // wndr TEXREF-fix-1\n',
'#include "RHITextureReference.h" // wndr TEXREF-fix-1\n#include "ShaderParameterStruct.h" // wndr RDGRES-fix-1 GetShaderParameterResourceRHI\n'),
(
'FCriticalSection GDawnResTableLock;',
'// wndr RDGRES-fix-1: resources pre-resolved on the render thread for a deferred update (index-aligned with Layout.Resources).\nstatic thread_local const TArray<TRefCountPtr<FRHIResource>>* GWndrPreResolvedRes = nullptr;\nFCriticalSection GDawnResTableLock;'),
(
'''		FRHIResource* Res = *(FRHIResource* const*)((const uint8*)Contents + R.MemberOffset);
		FString Name;''',
'''		const int32 _ri = (int32)(&R - Layout.Resources.GetData());
		FRHIResource* Res = (GWndrPreResolvedRes && GWndrPreResolvedRes->IsValidIndex(_ri)) ? (*GWndrPreResolvedRes)[_ri].GetReference()
			: GetShaderParameterResourceRHI(Contents, R.MemberOffset, (EUniformBufferBaseType)R.MemberType); // wndr RDGRES-fix-1
		FString Name;'''),
(
'''			FRHIResource* Res = *(FRHIResource* const*)((const uint8*)Contents + R.MemberOffset);
			if (Res && Res->IsValid()) { KeepRes.Emplace(Res); }''',
'''			FRHIResource* Res = GetShaderParameterResourceRHI(Contents, R.MemberOffset, (EUniformBufferBaseType)R.MemberType); // wndr RDGRES-fix-1
			KeepRes.Emplace((Res && Res->IsValid()) ? Res : nullptr); // index-aligned with L.Resources'''),
(
'''			RHIUpdateUniformBuffer(Cmd, Keep.GetReference(), Copy);''',
'''			GWndrPreResolvedRes = &KeepRes; // wndr RDGRES-fix-1
			RHIUpdateUniformBuffer(Cmd, Keep.GetReference(), Copy);
			GWndrPreResolvedRes = nullptr;'''),
]
for old, new in reps:
    assert s.count(old) == 1, (old[:70], s.count(old))
    s = s.replace(old, new)
open(DR, 'w').write(s)
print('patched', DR)
