#!/usr/bin/env bash
UE=/mnt/vms/ss-build/UnrealEngine
CL=/mnt/vms/ss-build/cook_es31.log
COOK=$UE/UnrealTournament/Saved/Cooked/SimplyStream
OUT=/mnt/vms/ss-build/orchestrate_er.out
exec >"$OUT" 2>&1
echo "=== editor rebuild (GeomCollection WebGPU exclusion) $(date -Is) ==="
cd "$UE"
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "bash Engine/Build/BatchFiles/RunUBT.sh UnrealTournamentEditor Linux Development -NoUBA -MaxParallelActions=12"
RC=$?
echo "=== EDITOR BUILD rc=$RC $(date -Is) ==="
if [ $RC -ne 0 ]; then echo "EDITOR BUILD FAILED"; grep -iE "error:|GeometryCollectionRendering|LegacyShaderPlatform" "$UE/../rebuild_editor_engine.log" 2>/dev/null | head; grep -iE "error:" "$OUT" | head; echo "=== ER ORCHESTRATION DONE ==="; exit 1; fi
echo "=== recook $(date -Is) ==="
cd /mnt/vms/ss-build && bash run_webgpu_es31_cook.sh
echo "--- WorldGridMaterial fail ---"; grep -c "Failed to compile default material /Engine/EngineMaterials/WorldGridMaterial" "$CL" 2>/dev/null
echo "--- null-shadermap ---"; grep -c "doesnt have a valid ShaderMap" "$CL" 2>/dev/null
echo "--- WorldGridMaterial.uexp size (want ~1MB) ---"; ls -la "$COOK/Engine/Content/EngineMaterials/WorldGridMaterial.uexp" 2>/dev/null | awk "{print \$5}"
echo "--- GeomCollection still compiling? ---"; grep -c "FGeometryCollectionVertexFactory" "$CL" 2>/dev/null
echo "=== ER ORCHESTRATION DONE ==="
