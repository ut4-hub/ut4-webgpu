#!/usr/bin/env bash
# UBSLOT chain 4 (2026-08-18): DDC WIPE + clean full cook with the ALREADY-
# CORRECT tools (SCW lib carries C3F20814 + sequential UB slots). Chain 3
# failed because the GUID bump does not invalidate DDC here (the known
# DawnShaderFormat GetVersion quirk) and the cache still held shaders compiled
# by the stale Aug-12 SCW (BufferIndex=0 for every UB -> Add-time slot
# collision -> Material UB overwritten by the last-added UB -> delivered NULL).
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
LOG=/mnt/vms/ss-build/ubslot_recook4.log
exec >"$LOG" 2>&1
cd "$UE"
C="$UE/UnrealTournament/Content/RestrictedAssets"
echo "=== UBSLOT4 chain started $(date -Is) ==="
echo "--- SCW lib sanity (must be 1) ---"
strings "$UE/Engine/Binaries/Linux/libShaderCompileWorker-DawnShaderFormat.so" | grep -c "C3F20814" || true
echo "--- [1/3] DDC WIPE (GUID bump does NOT invalidate DDC here — known quirk) ---"
rm -rf "$UE/Engine/DerivedDataCache"
echo "DDC wiped rc=$?"
echo "--- [2/3] CLEAN full-scope cook ---"
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "$UE/UnrealTournament/Binaries/Linux/UnrealTournamentEditor-Cmd $UE/UnrealTournament/UnrealTournament.uproject -run=Cook -TargetPlatform=SimplyStream -Map=/Game/RestrictedAssets/Maps/UT-Entry+/Game/RestrictedAssets/Maps/DM-Chill -CookDir=$C/Character -CookDir=$C/Weapons -CookDir=$C/Audio -CookDir=$C/Pickups -CookDir=$C/Effects -CookDir=$C/Blueprints -CookDir=$C/Functions -clean -unversioned -DisablePlugins=GPULightmass -AllowPartialShaderMaps -ini:Engine:[ConsoleVariables]:r.AreShaderErrorsFatal=0 -ini:Engine:[ConsoleVariables]:r.Shadow.Virtual.Enable=0 -ini:Engine:[ConsoleVariables]:r.BindlessResources=0 -ini:Engine:[ConsoleVariables]:r.BindlessSamplers=0"
RC=$?
echo "=== COOK EXITED rc=$RC at $(date -Is) ==="
ls -la "$UE/UnrealTournament/Saved/Cooked/SimplyStream/UnrealTournament/Content/RestrictedAssets/Maps/" 2>/dev/null | grep -i "DM-Chill" | head -4
echo "--- [3/3] repack ---"
bash /mnt/vms/ss-build/postcook_package.sh
echo "postcook rc=$?"
grep -c "DM-Chill" /mnt/vms/ss-build/ut-iostore/PakListIoStore_utcontent*.txt 2>/dev/null || true
ls -la /mnt/vms/ss-build/ut-local-webgpu/ContentBootstrap*.data
md5sum "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.wasm" "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.js" /mnt/vms/ss-build/ut-local-webgpu/ContentBootstrap*.data
echo "=== UBSLOT4 CHAIN DONE $(date -Is) ==="
