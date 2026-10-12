#!/usr/bin/env python3
# MATERIAL RUNTIME-LOAD FIX (2026-08-13) — the last mile of issue #1.
#
# After the cook fixes, WorldGridMaterial compiles and its shaders are INLINE in
# the cooked .uexp. But at runtime the engine still fataled at
# MaterialShared.cpp:3167 ("No inline shaders, null GTSM"). Root cause is NOT the
# cook: it is a default-material async-LOAD RACE. InitDefaultMaterials kicks off
# LoadPackageAsync for every default material at MAX priority. WorldGridMaterial's
# package is large (1.18MB, inline shaders) so its IO is still in flight
# (WaitingForIo) when a small dependent default material (DefaultTextMaterialOpaque)
# finishes first and, in PostLoad, recursively flushes WorldGridMaterial. The
# loader refuses to block on IO from inside a PostLoad (deadlock avoidance) and
# leaves WorldGridMaterial PARTIALLY loaded -> its inline shader-map export never
# deserializes -> GameThreadShaderMap null -> Fatal.
#
# Fix: on wasm, load the default materials SYNCHRONOUSLY and sequentially
# (MD_Surface / WorldGridMaterial first) instead of async. ConditionalFinishInit's
# existing wasm path does a synchronous LoadObject, which fully completes IO +
# deserializes the inline shader map as the OUTERMOST load, so each default
# material is fully present before any dependent material's PostLoad references it.
# No recursive partial-load, no race.
#
# Runtime Engine code -> rebuild the WASM target (RunUBT UnrealTournament
# SimplyStream Development), re-apply the atomics-sleep shim, pull wasm+js. No recook.
import io, sys

F = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/Engine/Private/Materials/Material.cpp"
s = io.open(F, encoding="utf-8").read()

if "WebGPU: default materials carry INLINE shader maps" in s:
    print("already patched"); sys.exit(0)

old = (
'\t\tfor (int32 Domain = 0; Domain < MD_MAX; ++Domain)\n'
'\t\t{\n'
'\t\t\tFString ResolvedPath = ResolveIniObjectsReference(GDefaultMaterialNames[Domain]);\n'
'\n'
'\t\t\tFLoadPackageAsyncOptionalParams Params;\n'
'\t\t\tParams.PackagePriority = MAX_int32;\n'
'\t\t\tParams.CompletionDelegate = MakeUnique<FLoadPackageAsyncDelegate>(FLoadPackageAsyncDelegate::CreateLambda(\n'
'\t\t\t\t[Domain](const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result)\n'
'\t\t\t\t{\n'
'\t\t\t\t\tMaterialImpl::ConditionalFinishInitialization(Domain);\n'
'\t\t\t\t}));\n'
'\n'
'\t\t\tLoadPackageAsync(FPackagePath::FromPackageNameChecked(FTopLevelAssetPath(ResolvedPath).GetPackageName()), MoveTemp(Params));\n'
'\t\t}\n'
)

new = (
'#if PLATFORM_WASM\n'
'\t\t// WebGPU: default materials carry INLINE shader maps and are large; the async\n'
'\t\t// loader races -- a small dependent default material (DefaultTextMaterialOpaque)\n'
'\t\t// finishes first and in PostLoad recursively flushes the still-WaitingForIo\n'
'\t\t// WorldGridMaterial, leaving it PARTIALLY loaded (inline shader map not\n'
'\t\t// deserialized) -> null GameThreadShaderMap -> MaterialShared.cpp:3167 fatal.\n'
'\t\t// Load them synchronously & sequentially, MD_Surface (WorldGridMaterial) first,\n'
'\t\t// so each is fully present before any dependent postload. ConditionalFinishInit\'s\n'
'\t\t// wasm path does a synchronous (outermost) LoadObject that completes IO fully.\n'
'\t\tfor (int32 Domain = 0; Domain < MD_MAX; ++Domain)\n'
'\t\t{\n'
'\t\t\tMaterialImpl::ConditionalFinishInitialization(Domain);\n'
'\t\t}\n'
'#else\n'
'\t\tfor (int32 Domain = 0; Domain < MD_MAX; ++Domain)\n'
'\t\t{\n'
'\t\t\tFString ResolvedPath = ResolveIniObjectsReference(GDefaultMaterialNames[Domain]);\n'
'\n'
'\t\t\tFLoadPackageAsyncOptionalParams Params;\n'
'\t\t\tParams.PackagePriority = MAX_int32;\n'
'\t\t\tParams.CompletionDelegate = MakeUnique<FLoadPackageAsyncDelegate>(FLoadPackageAsyncDelegate::CreateLambda(\n'
'\t\t\t\t[Domain](const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result)\n'
'\t\t\t\t{\n'
'\t\t\t\t\tMaterialImpl::ConditionalFinishInitialization(Domain);\n'
'\t\t\t\t}));\n'
'\n'
'\t\t\tLoadPackageAsync(FPackagePath::FromPackageNameChecked(FTopLevelAssetPath(ResolvedPath).GetPackageName()), MoveTemp(Params));\n'
'\t\t}\n'
'#endif\n'
)

if old not in s:
    print("anchor (async default-material load loop) not found", file=sys.stderr); sys.exit(1)
s = s.replace(old, new, 1)
io.open(F, "w", encoding="utf-8").write(s)
print("patched Material.cpp: synchronous default-material load on wasm")
