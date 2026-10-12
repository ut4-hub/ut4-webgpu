#!/usr/bin/env python3
# SKIPDIAG-1 (particles invisible / trees misplaced hunt):
#  - BGINCOMPLETE / BGSKIPDRAW were UE_LOG (stripped) -> printf, with the entry names that
#    did get filled vs the reflected layout, rate-limited; per-300-frame skip totals.
#  - INSTDIAG-1: for PSOs whose vertex decl has per-instance streams, print the VS WGSL
#    input locations/formats and the decl elements (attr, fmt, offset, stream, instanced).
import sys, shutil, os
R = '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private/'
def patch(path, reps):
    s = open(path).read()
    if 'SKIPDIAG-1' in s: print('already patched', path); return
    bak = path + '.wndrbak_skipdiag'
    if not os.path.exists(bak): shutil.copy2(path, bak)
    for old, new, cnt in reps:
        assert s.count(old) == cnt, (path, old[:80], s.count(old))
        s = s.replace(old, new)
    open(path, 'w').write(s); print('patched', path)

patch(R + 'DawnCommandContext.cpp', [
('''				UE_LOG(LogDawnRHI, Warning, TEXT("BGINCOMPLETE grp=%u have=%d need=%d staticUB=%d"), Group, Entries.Num(), LG.Value.Num(), StaticUBsByName.Num());
				for (const FDawnShaderBinding& B : LG.Value)
				{
					bool bHave = (Pending && Pending->Find(B.Binding));
					UE_LOG(LogDawnRHI, Warning, TEXT("   BGImiss bind=%u kind=%d name=%s filled=%d"), B.Binding, (int)B.Kind, *B.Name, bHave?1:0);
				}''',
'''				// wndr SKIPDIAG-1: printf (UE_LOG is stripped); names of reflected bindings with no entry
				printf("WNDR BGINCOMPLETE #%u grp=%u have=%d need=%d missing:", _bi, Group, Entries.Num(), LG.Value.Num());
				for (const FDawnShaderBinding& B : LG.Value)
				{
					bool bGot = false; for (const WGPUBindGroupEntry& E : Entries) { if (E.binding == B.Binding) { bGot = true; break; } }
					if (!bGot) { printf(" %u:%s(k%d)", B.Binding, TCHAR_TO_UTF8(*B.Name), (int)B.Kind); }
				}
				printf("\\n"); fflush(stdout);''', 1),
('		if (_bgs<=8 || (_bgs%500)==0) { UE_LOG(LogDawnRHI, Warning, TEXT("BGSKIPDRAW: RHIDrawIndexedPrimitive skipped on incomplete bind group (n=%u) — command buffer stays valid"), _bgs); }',
 '		if (_bgs<=20 || (_bgs%500)==0) { printf("WNDR BGSKIPDRAW indexed n=%u inst=%u prims=%u\\n", _bgs, NumInstances, NumPrimitives); fflush(stdout); } // SKIPDIAG-1', 1),
])

patch(R + 'DawnDynamicRHI.cpp', [
('	// VTXPADFIX (frame-3 stall): Dawn requires every attribute location the VS',
 '''	// wndr SKIPDIAG-1 / INSTDIAG-1: per-instance-stream PSOs: VS inputs vs decl
	if (VertexDecl && VS && !VS->WGSLSource.IsEmpty())
	{
		bool bInst = false; for (const FVertexElement& Elem : VertexDecl->Elements) { if (Elem.bUseInstanceIndex) { bInst = true; break; } }
		static uint32 _idg = 0;
		if (bInst && _idg < 10)
		{
			++_idg;
			TMap<uint32, WGPUVertexFormat> SIn; ParseWgslVertexInputs(VS->WGSLSource, SIn);
			TArray<uint32> Ls; SIn.GenerateKeyArray(Ls); Ls.Sort();
			printf("WNDR INSTDIAG #%u wgslInputs:", _idg); for (uint32 L : Ls) { printf(" L%u=0x%x", L, (uint32)SIn[L]); }
			printf(" | decl:"); for (const FVertexElement& Elem : VertexDecl->Elements) { printf(" [A%u t%d off%u s%u str%u%s]", (uint32)Elem.AttributeIndex, (int)Elem.Type.GetValue(), (uint32)Elem.Offset, (uint32)Elem.StreamIndex, (uint32)Elem.Stride, Elem.bUseInstanceIndex ? " INST" : ""); }
			printf(" | ubs:"); for (const FDawnShaderBinding& B : VS->Bindings) { if (B.Kind == EDawnShaderBindingKind::UniformBuffer) { printf(" %s", TCHAR_TO_UTF8(*B.Name)); } }
			printf("\\n"); fflush(stdout);
		}
	}
	// VTXPADFIX (frame-3 stall): Dawn requires every attribute location the VS''', 1),
])
