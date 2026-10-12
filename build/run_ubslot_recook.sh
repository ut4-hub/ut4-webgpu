#!/usr/bin/env bash
# UBSLOT recook chain (2026-08-18): DawnShaderCompiler UB BufferIndex fix +
# GUID bump -> editor rebuild -> DDC wipe -> clean ES31 cook -> iostore repack
# -> wasm rebuild.
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
LOG=/mnt/vms/ss-build/ubslot_recook.log
exec >"$LOG" 2>&1
cd "$UE"
echo "=== UBSLOT chain started $(date -Is) ==="
echo "--- [1/5] editor rebuild (DawnShaderFormat host) ---"
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "bash Engine/Build/BatchFiles/RunUBT.sh UnrealTournamentEditor Linux Development -NoUBA -MaxParallelActions=8"
RC=$?
echo "=== EDITOR BUILD rc=$RC $(date -Is) ==="
if [ $RC -ne 0 ]; then echo "=== CHAIN ABORT (editor build failed) ==="; exit 1; fi
echo "--- [2/5] DDC wipe (DawnShaderFormat GetVersion is GUID-only; stale WGSL otherwise) ---"
rm -rf "$UE/Engine/DerivedDataCache"
echo "DDC wiped rc=$?"
echo "--- [3/5] clean ES31 cook ---"
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "$UE/UnrealTournament/Binaries/Linux/UnrealTournamentEditor-Cmd $UE/UnrealTournament/UnrealTournament.uproject -run=Cook -TargetPlatform=SimplyStream -Map=/Game/RestrictedAssets/Maps/UT-Entry -clean -unversioned -DisablePlugins=GPULightmass -AllowPartialShaderMaps -ini:Engine:[ConsoleVariables]:r.AreShaderErrorsFatal=0"
RC=$?
echo "=== COOK EXITED rc=$RC at $(date -Is) ==="
if [ $RC -ne 0 ]; then echo "=== CHAIN CONTINUES ANYWAY (prior cooks exit nonzero benignly) ==="; fi
echo "--- [4/5] iostore repack + bootstrap regen ---"
bash /mnt/vms/ss-build/postcook_package.sh
echo "postcook rc=$?"
echo "--- [5/5] wasm rebuild ---"
cd "$UE"
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "bash Engine/Build/BatchFiles/RunUBT.sh UnrealTournament SimplyStream Development -NoUBA -MaxParallelActions=8"
RC=$?
echo "=== WASM BUILD rc=$RC $(date -Is) ==="
if [ $RC -eq 0 ]; then
  python3 /mnt/vms/ss-build/ut4-webgpu-push/engine-patches/patch-atomics-sleep-shim.py "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournament.js" 2>/dev/null || true
fi
md5sum "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.wasm" "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.js" /mnt/vms/ss-build/ut-local-webgpu/ContentBootstrap.data 2>/dev/null
echo "=== UBSLOT CHAIN DONE $(date -Is) ==="
