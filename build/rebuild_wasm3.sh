#!/usr/bin/env bash
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
LOG=/mnt/vms/ss-build/rebuild_wasm3.log
exec >"$LOG" 2>&1
cd "$UE"
echo "=== wasm rebuild (IoStore container mount) started $(date -Is) ==="
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_ROOT=/mnt/vms/ss-build/UnrealEngine/Engine/Binaries/ThirdParty/DotNet/10.0/linux-x64; export PATH=/mnt/vms/ss-build/UnrealEngine/Engine/Binaries/ThirdParty/DotNet/10.0/linux-x64:/run/wrappers/bin:/home/lucaspick/.nix-profile/bin:/nix/profile/bin:/home/lucaspick/.local/state/nix/profile/bin:/etc/profiles/per-user/lucaspick/bin:/nix/var/nix/profiles/default/bin:/run/current-system/sw/bin; bash Engine/Build/BatchFiles/RunUBT.sh UnrealTournament SimplyStream Development -NoUBA -MaxParallelActions=3"
echo "=== BUILD EXITED rc=$? at $(date -Is) ==="

# wndr: re-apply the atomics-sleep JS patch (emscripten regenerates the abort stub every link)
python3 /mnt/vms/ss-build/patch_atomics_sleep_js.py "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.js" || true
python3 /mnt/vms/ss-build/patch_worker_errstack_js.py "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.js" || true
python3 /mnt/vms/ss-build/patch_refunderflow_stack_js.py "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.js" || true
python3 /mnt/vms/ss-build/patch_async_stdout_js.py "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.js" || true
cp "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.js" /mnt/vms/ss-build/ut-serve/UnrealTournament.js
cp "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.wasm" /mnt/vms/ss-build/ut-serve/UnrealTournamentwasm64.wasm
