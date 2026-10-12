#!/usr/bin/env bash
CL=/mnt/vms/ss-build/cook_es31.log
COOK=/mnt/vms/ss-build/UnrealEngine/UnrealTournament/Saved/Cooked/SimplyStream
for n in $(seq 1 90); do
  if grep -q "COOK EXITED" "$CL" 2>/dev/null; then break; fi
  sleep 20
done
echo "=== COOK STATUS ==="; grep "COOK EXITED" "$CL" 2>/dev/null || echo "(still running)"
echo "=== SM5 global shader cache produced? ==="
find "$COOK/Engine" -iname "GlobalShaderCache*" 2>/dev/null
echo "=== WorldGridMaterial SM5 compile ==="
grep -c "Failed to compile default material /Engine/EngineMaterials/WorldGridMaterial" "$CL" 2>/dev/null | sed "s/^/  WorldGrid fail: /"
grep -iE "Failed to compile Material /Engine/EngineMaterials/WorldGridMaterial.*SF_WEBGPU" "$CL" 2>/dev/null | tail -1
echo "=== material null-shadermap warnings ==="
grep -c "doesnt have a valid ShaderMap" "$CL" 2>/dev/null
echo "=== total WgslFromIR failures + top reasons (SM5 Tint survival) ==="
grep -c "WgslFromIR:" "$CL" 2>/dev/null | sed "s/^/  total WgslFromIR fails: /"
grep -oE "WgslFromIR: error: [^\"]*" "$CL" 2>/dev/null | sed -E "s/[0-9]+/N/g" | sort | uniq -c | sort -rn | head -8
echo "=== WorldGridMaterial.uexp size (shaders inline?) ==="
ls -la "$COOK/Engine/Content/EngineMaterials/WorldGridMaterial.uexp" 2>/dev/null | awk "{print \$5}"
