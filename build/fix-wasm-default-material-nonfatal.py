#!/usr/bin/env python3
# WebGPU MENU BRING-UP (2026-08-13): default-material-incomplete non-fatal on wasm.
#
# The closed SimplyStream RHI is immovably SP_WEBGPU_SM5, but the SM5 material cook
# can't complete every default-material permutation for WebGPU (Nanite / Virtual
# Shadow Maps / niche-VF shaders use features -- 64-bit image atomics, read_write
# storage in the vertex stage -- that WGSL forbids). So WorldGridMaterial's shader
# map is incomplete/null and MaterialShared.cpp:3167 hard-fatals at runtime,
# killing boot before anything renders.
#
# For a Slate MENU this fatal is unnecessary: Slate draws with GLOBAL shaders (which
# DO cook), not the default material. The code already falls through to
# SetGameThreadShaderMap(nullptr) (engine fallback) right after the log call, so
# demoting Fatal->Warning on wasm lets the engine boot past it and render the menu.
# A degraded default material only affects 3D material rendering the menu doesn't do.
#
# Runtime Engine code -> wasm rebuild (+ atomics shim + pull). Cook SM5@SM6.
import io, sys

F = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/Engine/Private/Materials/MaterialShared.cpp"
s = io.open(F, encoding="utf-8").read()

if "WebGPU bring-up: don't kill boot" in s:
    print("already patched"); sys.exit(0)

old = (
    '\t\t\t\t//assert if the default material\'s shader map was not found, since it will cause problems later\n'
    '\t\t\t\tUE_LOGF(LogMaterial, Fatal,"Failed to find shader map for default material %ls(%ls)! Please make sure cooking was successful (%ls inline shaders, %ls GTSM)",\n'
    '\t\t\t\t\t*GetFriendlyName(),\n'
    '\t\t\t\t\t*Instance,\n'
    '\t\t\t\t\tbContainsInlineShaders ? TEXT("Contains") : TEXT("No"),\n'
    '\t\t\t\t\tGameThreadShaderMap ? TEXT("has") : TEXT("null")\n'
    '\t\t\t\t);\n'
)
new = (
    '\t\t\t\t//assert if the default material\'s shader map was not found, since it will cause problems later\n'
    '\t\t\t\t// WebGPU bring-up: don\'t kill boot when the SM5 cook can\'t complete a default\n'
    '\t\t\t\t// material (Nanite/VSM/niche-VF shaders are WGSL-incompatible). Warn and fall\n'
    '\t\t\t\t// through to SetGameThreadShaderMap(nullptr); the Slate menu renders via global shaders.\n'
    '\t\t\t\tUE_LOGF(LogMaterial,\n'
    '#if PLATFORM_WASM\n'
    '\t\t\t\t\tWarning,\n'
    '#else\n'
    '\t\t\t\t\tFatal,\n'
    '#endif\n'
    '\t\t\t\t\t"Failed to find shader map for default material %ls(%ls)! Please make sure cooking was successful (%ls inline shaders, %ls GTSM)",\n'
    '\t\t\t\t\t*GetFriendlyName(),\n'
    '\t\t\t\t\t*Instance,\n'
    '\t\t\t\t\tbContainsInlineShaders ? TEXT("Contains") : TEXT("No"),\n'
    '\t\t\t\t\tGameThreadShaderMap ? TEXT("has") : TEXT("null")\n'
    '\t\t\t\t);\n'
)
if old not in s:
    print("anchor not found (file may differ)", file=sys.stderr); sys.exit(1)
s = s.replace(old, new, 1)
io.open(F, "w", encoding="utf-8").write(s)
print("patched MaterialShared.cpp: default-material-incomplete is non-fatal on wasm")
