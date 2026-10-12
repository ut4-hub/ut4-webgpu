#!/usr/bin/env bash
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
LOG=/mnt/vms/ss-build/rebuild_wasm_wndr.log
exec >"$LOG" 2>&1
cd "$UE"
echo "=== wasm rebuild (wndr un-bypass DrawStaticElements + diags) started $(date -Is) ==="
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "bash Engine/Build/BatchFiles/RunUBT.sh UnrealTournament SimplyStream Development -NoUBA -MaxParallelActions=3"
echo "=== BUILD EXITED rc=$? at $(date -Is) ==="
