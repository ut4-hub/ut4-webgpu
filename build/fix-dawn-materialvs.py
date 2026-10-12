#!/usr/bin/env python3
# MATVS-1 (flying trees): UE 5.8 gives the VS a compact "MaterialVS" UB. Its WGSL
# bindings are MaterialVS_<Member> (original member names, e.g. Texture2D_3), but our
# capture names every material UB "Material" with formula (ordinal) member names, so
# MaterialVS_* never matched -> grey dummy -> wind/WPO textures read 0.5 -> trees
# displaced. Resolve MaterialVS_<Member> from the draw's FULL Material UB, whose
# formula names line up with the original member names.
import sys, shutil, os
P = '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private/DawnCommandContext.cpp'
s = open(P).read()
if 'MATVS-1' in s: print('already patched'); sys.exit(0)
bak = P + '.wndrbak_matvs'
if not os.path.exists(bak): shutil.copy2(P, bak)
old = '''					if (!WndrHasUBPrefix(B.Name, DUB->ShaderName)) { return false; }
					WNDR_STAGE(23);
					const TCHAR* Suffix = *B.Name + DUB->ShaderName.Len() + 1;'''
new = '''					const TCHAR* Suffix = nullptr;
					if (WndrHasUBPrefix(B.Name, DUB->ShaderName)) { Suffix = *B.Name + DUB->ShaderName.Len() + 1; }
					else if (B.Name.StartsWith(TEXT("MaterialVS_"), ESearchCase::CaseSensitive) // wndr MATVS-1
						&& DUB->ShaderName.Equals(TEXT("Material"), ESearchCase::CaseSensitive)
						&& !DUB->GetLayout().GetDebugName().Equals(TEXT("MaterialVS"), ESearchCase::CaseSensitive))
					{
						Suffix = *B.Name + 11; // len("MaterialVS_")
					}
					else { return false; }
					WNDR_STAGE(23);'''
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
open(P, 'w').write(s); print('patched OK')
