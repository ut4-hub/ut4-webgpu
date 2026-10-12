#!/usr/bin/env bash
CL=/mnt/vms/ss-build/cook_es31.log
COOK=/mnt/vms/ss-build/UnrealEngine/UnrealTournament/Saved/Cooked/SimplyStream
last=""
for n in $(seq 1 150); do
  if grep -q "COOK EXITED" "$CL" 2>/dev/null; then break; fi
  cur=$(grep -oE "Cooked packages [0-9]+ Packages Remain [0-9]+" "$CL" 2>/dev/null | tail -1)
  [ "$cur" != "$last" ] && echo "[$(date +%H:%M)] $cur" && last="$cur"
  sleep 60
done
echo "=== FINAL COOK STATUS ==="; grep "COOK EXITED" "$CL" 2>/dev/null || echo "(still running after window)"
echo "=== SM5 global cache ==="; find "$COOK/Engine" -iname "GlobalShaderCache*" 2>/dev/null
echo "=== WorldGridMaterial fail / null-shadermap ==="
grep -c "Failed to compile default material /Engine/EngineMaterials/WorldGridMaterial" "$CL" 2>/dev/null | sed "s/^/  WorldGrid fail: /"
grep -c "doesnt have a valid ShaderMap" "$CL" 2>/dev/null | sed "s/^/  null-shadermap: /"
echo "=== WorldGridMaterial.uexp size ==="; ls -la "$COOK/Engine/Content/EngineMaterials/WorldGridMaterial.uexp" 2>/dev/null | awk "{print \$5}"
echo "=== total Tint fails ==="; grep -c "WgslFromIR:" "$CL" 2>/dev/null
