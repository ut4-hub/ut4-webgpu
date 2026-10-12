#!/usr/bin/env bash
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
LOG=/mnt/vms/ss-build/rebuild_editor_engine.log
exec >"$LOG" 2>&1
cd "$UE"
echo "=== editor incremental rebuild (GeometryCollection mobile-guard) started $(date -Is) ==="
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "bash Engine/Build/BatchFiles/RunUBT.sh UnrealTournamentEditor Linux Development -NoUBA -MaxParallelActions=8"
echo "=== EDITOR BUILD EXITED rc=$? at $(date -Is) ==="
