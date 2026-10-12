#!/usr/bin/env python3
# MATERIAL-SHADER SURVIVAL FIX part 3 (2026-08-13).
#
# After the textureSample + derivative bridge fixes, the default material
# WorldGridMaterial STILL failed to cook for SF_WEBGPU_ES31 -- but not on a Tint
# error. The real error:
#   "Shader attempted to bind the Primitive uniform buffer even though Vertex
#    Factory FGeometryCollectionVertexFactory computes a PrimitiveId per-instance."
# Default materials must compile for every vertex factory, so this one VF sinks
# the whole material -> no shader library -> WorldGridMaterial null GTSM.
#
# Root cause: FLocalVertexFactory::ValidateCompiledResult skips this check on
# mobile platforms ("On mobile VS may use Primitive UB while GPUScene is
# enabled") via `!IsMobilePlatform(Platform)`, but the otherwise-identical
# FGeometryCollectionVertexFactory::ValidateCompiledResult omits that guard.
# SP_WEBGPU_ES31 is a mobile platform (which is why LocalVF passes and only
# GeometryCollection trips). Fix = add the same mobile guard to the
# GeometryCollection check, mirroring the LocalVertexFactory behavior Epic
# already ships. (GeometryCollection = Chaos destruction geometry, never used by
# the menu anyway.)
#
# This is EDITOR/cook code (Engine module) -> incremental editor relink, then recook.
import io, sys

F = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/Engine/Private/GeometryCollectionRendering.cpp"
s = io.open(F, encoding="utf-8").read()

if "mirror FLocalVertexFactory mobile guard" in s:
    print("already patched"); sys.exit(0)

old = (
    "\tif (Type->SupportsPrimitiveIdStream()\n"
    "\t\t&& UseGPUScene(Platform)\n"
    "\t\t&& ParameterMap.ContainsParameterAllocation(FPrimitiveUniformShaderParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName()))"
)
new = (
    "\tif (Type->SupportsPrimitiveIdStream()\n"
    "\t\t&& UseGPUScene(Platform)\n"
    "\t\t&& !IsMobilePlatform(Platform) // mirror FLocalVertexFactory mobile guard: on mobile VS may use Primitive UB while GPUScene is enabled\n"
    "\t\t&& ParameterMap.ContainsParameterAllocation(FPrimitiveUniformShaderParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName()))"
)
if old not in s:
    print("anchor not found (file may differ)", file=sys.stderr); sys.exit(1)
s = s.replace(old, new, 1)
io.open(F, "w", encoding="utf-8").write(s)
print("patched GeometryCollectionRendering.cpp: mobile guard on Primitive-UB validation")
