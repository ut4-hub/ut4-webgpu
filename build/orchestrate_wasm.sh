#!/usr/bin/env bash
UE=/mnt/vms/ss-build/UnrealEngine
JS=$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournament.js
OUT=/mnt/vms/ss-build/orchestrate_wasm.out
exec >"$OUT" 2>&1
echo "=== wasm rebuild (sync default materials) started $(date -Is) ==="
cd "$UE"
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "bash Engine/Build/BatchFiles/RunUBT.sh UnrealTournament SimplyStream Development -NoUBA -MaxParallelActions=8"
RC=$?
echo "=== BUILD EXITED rc=$RC at $(date -Is) ==="
if [ $RC -eq 0 ]; then
  echo "=== applying atomics-sleep shim ==="
  python3 /mnt/vms/ss-build/ut4-webgpu-push/engine-patches/patch-atomics-sleep-shim.py "$JS"
  ls -la "$JS" "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournament.wasm"
fi
echo "=== WASM ORCHESTRATION DONE ==="
