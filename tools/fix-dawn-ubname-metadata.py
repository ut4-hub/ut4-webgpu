#!/usr/bin/env python3
# UBNAME-meta-1: DawnDeriveShaderUBName stripped "F...Parameters" off the struct
# TYPE name, but the WGSL var carries the registered SHADER VARIABLE name. They
# differ for e.g. FGPUSkinVertexFactoryUniformShaderParameters -> "GPUSkinVFBase":
# the skin UB was UBDROPped and its BoneMatrices never resolved -> characters
# skinned with dummy matrices (blocks). Prefer the engine's registered name
# (FShaderParametersMetadata struct list), keeping the "<derived>Buffer" alias
# cases (PrecomputedLighting) on the derived name the rest of the code expects.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')
s = open(CC).read()
if 'UBNAME-meta-1' in s:
    print('already patched'); sys.exit(0)
bak = CC + '.wndrbak_ubnamemeta'
if not os.path.exists(bak): shutil.copy2(CC, bak)
old = '''	for (const TCHAR* Sfx : Suffixes) { if (N.EndsWith(Sfx, ESearchCase::CaseSensitive)) { N.LeftChopInline(FCString::Strlen(Sfx)); break; } }
	return N;
}'''
new = '''	for (const TCHAR* Sfx : Suffixes) { if (N.EndsWith(Sfx, ESearchCase::CaseSensitive)) { N.LeftChopInline(FCString::Strlen(Sfx)); break; } }
	// wndr UBNAME-meta-1: the engine-registered shader variable name wins.
	static FCriticalSection Lock;
	static TMap<FString, FString> TypeToVar;
	static std::atomic<bool> bBuilt{false};
	if (!bBuilt.load(std::memory_order_acquire))
	{
		FScopeLock L(&Lock);
		if (!bBuilt.load(std::memory_order_relaxed))
		{
			for (TLinkedList<FShaderParametersMetadata*>::TIterator It(FShaderParametersMetadata::GetStructList()); It; It.Next())
			{
				const FShaderParametersMetadata* M = *It;
				if (M && M->GetStructTypeName() && M->GetShaderVariableName()) { TypeToVar.Add(FString(M->GetStructTypeName()), FString(M->GetShaderVariableName())); }
			}
			printf("WNDR UBNAME-meta map built: %d structs\\n", TypeToVar.Num()); fflush(stdout);
			bBuilt.store(true, std::memory_order_release);
		}
	}
	if (const FString* Var = TypeToVar.Find(DebugName))
	{
		if (!Var->IsEmpty() && !Var->Equals(N, ESearchCase::CaseSensitive) && !Var->Equals(N + TEXT("Buffer"), ESearchCase::CaseSensitive))
		{
			static uint32 _ml = 0; if (_ml++ < 40) { printf("WNDR UBNAME-meta %s -> %s (was %s)\\n", TCHAR_TO_UTF8(*DebugName), TCHAR_TO_UTF8(**Var), TCHAR_TO_UTF8(*N)); fflush(stdout); }
			return *Var;
		}
	}
	return N;
}'''
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
if '#include "ShaderParameterMetadata.h"' not in s:
    s = s.replace('#include "DawnResources.h"\n', '#include "DawnResources.h"\n#include "ShaderParameterMetadata.h" // wndr UBNAME-meta-1\n#include <atomic>\n', 1)
open(CC, 'w').write(s)
print('patched', CC)
