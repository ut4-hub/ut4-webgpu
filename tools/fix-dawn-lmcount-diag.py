#!/usr/bin/env python3
# wndr LMCOUNT diag: count, per bind-group flush, whether lightmap-related bindings
# (PrecomputedLightingBuffer, LightmapResourceCluster_*) get REAL / RESTABLE / DUMMY.
import sys, shutil
P = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private/DawnCommandContext.cpp"
s = open(P).read()
if "WndrLMCount" in s: print("already applied"); sys.exit(0)
shutil.copy(P, P + ".wndrbak_lmcount")
helper = r'''
// wndr LMCOUNT: Src 0=REAL(per-draw) 1=STATIC 2=RESTABLE 3=DUMMY
static void WndrLMCount(const FString& Name, int Src)
{
	const bool bPLB = Name.Contains(TEXT("Precomputed"));
	const bool bLMT = Name.Contains(TEXT("LightmapResourceCluster")) || Name.Contains(TEXT("LightMapTexture"));
	if (!bPLB && !bLMT) { return; }
	static uint64 C[2][4] = {}; static uint64 N = 0;
	++C[bPLB ? 0 : 1][Src];
	if ((++N % 4000) == 1)
	{
		printf("WNDR LMCOUNT n=%llu PLB real=%llu static=%llu restable=%llu dummy=%llu | LMTEX real=%llu static=%llu restable=%llu dummy=%llu\n",
			(unsigned long long)N, C[0][0], C[0][1], C[0][2], C[0][3], C[1][0], C[1][1], C[1][2], C[1][3]);
		fflush(stdout);
	}
}
'''
anchor = "bool FDawnCommandContext::FlushPendingBindGroups()"
assert s.count(anchor) == 1
s = s.replace(anchor, helper + anchor, 1)
reps = [
 ("			if (PendE && !bStaticFirst) { Entries.Add(*PendE); continue; }",
  "			if (PendE && !bStaticFirst) { WndrLMCount(B.Name, 0); Entries.Add(*PendE); continue; }"),
 ("			if (PendE) { Entries.Add(*PendE); continue; }",
  "			if (PendE) { WndrLMCount(B.Name, 0); Entries.Add(*PendE); continue; }"),
 ("E.size=DUB->GetLayout().ConstantBufferSize; Entries.Add(E);",
  "E.size=DUB->GetLayout().ConstantBufferSize; WndrLMCount(B.Name, 1); Entries.Add(E);"),
 ("""					if (GDawnVerboseBindLog && (_uf <= 24 || (_uf % 5000) == 0)) { printf("DAWNBG UBRESBIND grp=%u bind=%u name=%s (n=%u)\\n", Group, B.Binding, TCHAR_TO_UTF8(*B.Name), _uf); fflush(stdout); }
					Entries.Add(E);""",
  """					if (GDawnVerboseBindLog && (_uf <= 24 || (_uf % 5000) == 0)) { printf("DAWNBG UBRESBIND grp=%u bind=%u name=%s (n=%u)\\n", Group, B.Binding, TCHAR_TO_UTF8(*B.Name), _uf); fflush(stdout); }
					WndrLMCount(B.Name, 2);
					Entries.Add(E);"""),
 ("""					if (DAWNRHI_VERBOSE_DIAG && _df++ < 40) { UE_LOG(LogDawnRHI, Warning, TEXT("DUMMYFILL""",
  """					WndrLMCount(B.Name, 3);
					if (DAWNRHI_VERBOSE_DIAG && _df++ < 40) { UE_LOG(LogDawnRHI, Warning, TEXT("DUMMYFILL"""),
]
for a, b in reps:
    n = s.count(a); assert n == 1, (n, a[:60])
    s = s.replace(a, b, 1)
open(P, "w").write(s); print("applied LMCOUNT")
