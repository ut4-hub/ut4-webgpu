#!/usr/bin/env bash
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
LOG=/mnt/vms/ss-build/build_dawnrhi_native.log
exec >"$LOG" 2>&1
cd "$UE"
echo "=== native DawnRHITest build (Phase 2 iteration-loop baseline) $(date -Is) ==="
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "bash Engine/Build/BatchFiles/RunUBT.sh DawnRHITest Linux Development -NoUBA -MaxParallelActions=8"
echo "=== DAWNRHI NATIVE BUILD EXITED rc=$? at $(date -Is) ==="
