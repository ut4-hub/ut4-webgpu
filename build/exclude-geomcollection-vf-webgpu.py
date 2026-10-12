#!/usr/bin/env python3
# SM5 MATERIAL-SURVIVAL FIX (2026-08-13): exclude the GeometryCollection VF on WebGPU.
#
# With the SM5 cook, WorldGridMaterial (a special engine material, so it compiles
# for every VF) failed on FGeometryCollectionVertexFactory / TShadowDepthVSCombined:
#   Tint ReadIR: spirv error: SPIR-V failed validation
# The GeometryCollection shadow-depth VS binds a read_write storage buffer in the
# vertex stage, which WGSL forbids ("var with 'storage' address space and
# 'read_write' access mode cannot be used by vertex pipeline stage"). GeometryCollection
# is Chaos destruction geometry -- never used by the menu -- so the clean fix is to
# skip the VF entirely on the WebGPU platform, so no material has to compile its
# WGSL-invalid shaders. (Supersedes the narrower mobile-guard fix for the ES31
# Primitive-UB check, which stays but is now moot for this VF.)
#
# ShouldCompilePermutation runs at cook time -> editor rebuild, then recook.
import io, sys

F = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/Engine/Private/GeometryCollectionRendering.cpp"
s = io.open(F, encoding="utf-8").read()

if "Skip GeometryCollection on WebGPU" in s:
    print("already patched"); sys.exit(0)

# ensure RHIStrings.h (for LegacyShaderPlatformToShaderFormat) is included
if '#include "RHIStrings.h"' not in s:
    anchor_inc = '#include "DataDrivenShaderPlatformInfo.h"\n'
    if anchor_inc in s:
        s = s.replace(anchor_inc, anchor_inc + '#include "RHIStrings.h"\n', 1)

anchor = (
    "bool FGeometryCollectionVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)\n"
    "{\n"
)
inject = (
    "bool FGeometryCollectionVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)\n"
    "{\n"
    "\t// Skip GeometryCollection on WebGPU: Chaos-destruction geometry is unused by the\n"
    "\t// menu, and its shaders (e.g. TShadowDepthVSCombined) bind read_write storage in\n"
    "\t// the vertex stage, which WGSL forbids -> Tint SPIR-V validation failure that sinks\n"
    "\t// special engine materials (WorldGridMaterial).\n"
    "\tif (LegacyShaderPlatformToShaderFormat(Parameters.Platform).ToString().StartsWith(TEXT(\"SF_WEBGPU\")))\n"
    "\t{\n"
    "\t\treturn false;\n"
    "\t}\n"
)
if anchor not in s:
    print("ShouldCompilePermutation anchor not found", file=sys.stderr); sys.exit(1)
s = s.replace(anchor, inject, 1)
io.open(F, "w", encoding="utf-8").write(s)
print("patched GeometryCollectionRendering.cpp: exclude GeometryCollection VF on WebGPU")
