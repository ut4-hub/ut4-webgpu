#!/usr/bin/env bash
# UBSLOT recook part 2 (2026-08-18): FULL gameplay content scope (the morning
# wall-fix recipe: UT-Entry+DM-Chill + Character/Weapons/Audio/Pickups/Effects/
# Blueprints/Functions), NO -clean (DDC already holds the new-GUID UB-slot
# shaders from the UT-Entry cook; incremental adds DM-Chill + gameplay deps),
# then iostore repack + bootstrap regen.
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
LOG=/mnt/vms/ss-build/ubslot_recook2.log
exec >"$LOG" 2>&1
cd "$UE"
C="$UE/UnrealTournament/Content/RestrictedAssets"
echo "=== UBSLOT full-content cook started $(date -Is) ==="
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "$UE/UnrealTournament/Binaries/Linux/UnrealTournamentEditor-Cmd $UE/UnrealTournament/UnrealTournament.uproject -run=Cook -TargetPlatform=SimplyStream -Map=/Game/RestrictedAssets/Maps/UT-Entry+/Game/RestrictedAssets/Maps/DM-Chill -CookDir=$C/Character -CookDir=$C/Weapons -CookDir=$C/Audio -CookDir=$C/Pickups -CookDir=$C/Effects -CookDir=$C/Blueprints -CookDir=$C/Functions -unversioned -DisablePlugins=GPULightmass -AllowPartialShaderMaps -ini:Engine:[ConsoleVariables]:r.AreShaderErrorsFatal=0 -ini:Engine:[ConsoleVariables]:r.Shadow.Virtual.Enable=0 -ini:Engine:[ConsoleVariables]:r.BindlessResources=0 -ini:Engine:[ConsoleVariables]:r.BindlessSamplers=0"
RC=$?
echo "=== COOK EXITED rc=$RC at $(date -Is) ==="
echo "--- DM-Chill cooked check ---"
ls -la "$UE/UnrealTournament/Saved/Cooked/SimplyStream/UnrealTournament/Content/RestrictedAssets/Maps/DM-Chill.umap" "$UE/UnrealTournament/Saved/Cooked/SimplyStream/UnrealTournament/Content/RestrictedAssets/Maps/DM-Chill_BuiltData.uexp" 2>&1 | head -4
echo "--- repack ---"
bash /mnt/vms/ss-build/postcook_package.sh
echo "postcook rc=$?"
echo "--- chunk + md5 inventory ---"
ls -la /mnt/vms/ss-build/ut-local-webgpu/ContentBootstrap*.data
ls -la /mnt/vms/ss-build/ut-iostore/*.ucas
md5sum "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.wasm" "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.js" /mnt/vms/ss-build/ut-local-webgpu/ContentBootstrap*.data
echo "=== UBSLOT2 CHAIN DONE $(date -Is) ==="
