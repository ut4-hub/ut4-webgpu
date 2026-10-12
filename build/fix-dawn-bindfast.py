#!/usr/bin/env python3
# BINDFAST-1: the bind walk (~5000 binds/frame) was ~80% of the frame once the
# STATICUB stalls were gone. (a) per-stage timing probe -> stage store only
# (emscripten_get_now is a JS call per mark); (b) drop the SLOWUB trace; (c) no
# FString temporaries for "<UB>_<member>" prefix/suffix matching; (d) take the
# ResTable lock once per FlushPendingBindGroups instead of per TryUBTable.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')
s = open(CC).read()
if 'BINDFAST-1' in s:
    print('already patched'); sys.exit(0)
bak = CC + '.wndrbak_bindfast'
if not os.path.exists(bak): shutil.copy2(CC, bak)
reps = [
('#define WNDR_STAGE(n) WndrStageMark((uint32)(n))\n',
'''#define WNDR_STAGE(n) GWndrRhiStage.store((uint32)(n), std::memory_order_relaxed) // wndr BINDFAST-1: timing off (WndrStageMark re-arms it)
// wndr BINDFAST-1: "Name starts with P + '_'" without building a temporary FString.
static inline bool WndrHasUBPrefix(const FString& Name, const FString& P)
{
	const int32 L = P.Len();
	return L > 0 && Name.Len() > L && Name[L] == TEXT('_') && FCString::Strncmp(*Name, *P, L) == 0;
}
'''),
('''	struct FWndrStageExit { ~FWndrStageExit() { WNDR_STAGE(0); } } _wndrStageExit; // wndr RHISTAGE-probe-1
''',
'''	struct FWndrStageExit { ~FWndrStageExit() { WNDR_STAGE(0); } } _wndrStageExit; // wndr RHISTAGE-probe-1
	FScopeLock _ResTableLockAll(&GDawnResTableLock); // wndr BINDFAST-1: one lock per call (was per TryUBTable)
'''),
('''					WNDR_STAGE(20);
					FScopeLock _ResTableLock(&GDawnResTableLock); // wndr RESTABLE-lock-1
					WNDR_STAGE(21);
#if PLATFORM_WASM
					struct FWndrSlowUB { const FDawnUniformBuffer* D; const FDawnShaderBinding& Bn; int32 NP; int32 NS; double T0;
						~FWndrSlowUB() { const double Ms = emscripten_get_now() - T0; static uint32 _n = 0;
							if (Ms > 50.0 && _n++ < 60) { printf("WNDR SLOWUB ms=%.1f ub=%p snLen=%d resTbl=%d resTblMax=%d bNameLen=%d pendUBs=%d staticUBs=%d layoutName=%s\\n",
								Ms, (const void*)D, D->ShaderName.Len(), D->ResTable.Num(), D->ResTable.Max(), Bn.Name.Len(), NP, NS, TCHAR_TO_UTF8(*D->GetLayout().GetDebugName())); fflush(stdout); } } }
						_wndrSlowUB{ DUB, B, PendingUBRefs.Num(), StaticUBsByName.Num(), emscripten_get_now() }; // wndr SLOWUB-trace-1
#endif
					if (DUB->ShaderName.IsEmpty() || DUB->ResTable.Num() == 0) { return false; }
					if (!B.Name.StartsWith(DUB->ShaderName + TEXT("_"), ESearchCase::CaseSensitive)) { return false; }
					WNDR_STAGE(23);
					const FString Suffix = B.Name.Mid(DUB->ShaderName.Len() + 1);
					for (const FDawnUniformBuffer::FTableEntry& TE : DUB->ResTable)
					{
						if (!TE.Resource || !TE.WgslName.Equals(Suffix, ESearchCase::CaseSensitive)) { continue; }''',
'''					WNDR_STAGE(21); // ResTable lock held by the caller (wndr BINDFAST-1)
					if (DUB->ShaderName.IsEmpty() || DUB->ResTable.Num() == 0) { return false; }
					if (!WndrHasUBPrefix(B.Name, DUB->ShaderName)) { return false; }
					WNDR_STAGE(23);
					const TCHAR* Suffix = *B.Name + DUB->ShaderName.Len() + 1;
					for (const FDawnUniformBuffer::FTableEntry& TE : DUB->ResTable)
					{
						if (!TE.Resource || FCString::Strcmp(*TE.WgslName, Suffix) != 0) { continue; }'''),
('''						if (!B.Name.StartsWith(UBB.Name + TEXT("_"), ESearchCase::CaseSensitive)) { continue; }''',
'''						if (!WndrHasUBPrefix(B.Name, UBB.Name)) { continue; } // wndr BINDFAST-1'''),
]
for old, new in reps:
    assert s.count(old) == 1, (old[:70], s.count(old))
    s = s.replace(old, new)
open(CC, 'w').write(s)
print('patched', CC)
