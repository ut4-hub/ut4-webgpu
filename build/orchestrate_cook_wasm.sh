#!/usr/bin/env bash
UE=/mnt/vms/ss-build/UnrealEngine
CL=/mnt/vms/ss-build/cook_es31.log
COOK=$UE/UnrealTournament/Saved/Cooked/SimplyStream
JS=$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournament.js
OUT=/mnt/vms/ss-build/orchestrate_cw.out
exec >"$OUT" 2>&1
echo "=== waiting for cook $(date -Is) ==="
until grep -q "COOK EXITED" "$CL" 2>/dev/null; do sleep 20; done
echo "cook: $(grep "COOK EXITED" "$CL" | tail -1)"
echo "SM5 global cache: $(find "$COOK/Engine" -iname "GlobalShaderCache*SM5*" 2>/dev/null)"
echo "Slate global shaders present? $(grep -c "FSlateElementPS\|SlateElement" "$CL" 2>/dev/null) refs"
echo "WorldGrid uexp: $(ls -la "$COOK/Engine/Content/EngineMaterials/WorldGridMaterial.uexp" 2>/dev/null | awk "{print \$5}")"
echo "=== wasm rebuild (default-material non-fatal) $(date -Is) ==="
cd "$UE"
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "bash Engine/Build/BatchFiles/RunUBT.sh UnrealTournament SimplyStream Development -NoUBA -MaxParallelActions=8"
RC=$?
echo "=== WASM BUILD rc=$RC $(date -Is) ==="
if [ $RC -eq 0 ]; then
  python3 /mnt/vms/ss-build/ut4-webgpu-push/engine-patches/patch-atomics-sleep-shim.py "$JS"
  ls -la "$JS" "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournament.wasm"
else
  echo "WASM BUILD FAILED"; grep -iE "error:|MaterialShared" "$OUT" | head
fi
echo "=== CW ORCHESTRATION DONE ==="
