#!/usr/bin/env bash
# FULL content cook (2026-08-20): ALL gameplay maps + the ENTIRE RestrictedAssets
# art tree (Environments/Materials/Meshes/Textures/etc), to eliminate the
# untextured-gray world (missing cooked textures/materials). Reuses the DDC
# (shaders already correct post-UBSLOT4) so only NEW content compiles shaders.
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
C="$UE/UnrealTournament/Content/RestrictedAssets"
LOG=/mnt/vms/ss-build/full_cook.log
exec >"$LOG" 2>&1
cd "$UE"
echo "=== FULL COOK started $(date -Is) ==="
echo "--- disk before ---"; df -h /mnt/vms | tail -1

MAPS="/Game/RestrictedAssets/Maps/CTF-Face"
MAPS="$MAPS+/Game/RestrictedAssets/Maps/CTF-TitanPass"
MAPS="$MAPS+/Game/RestrictedAssets/Maps/DM-BioTower"
MAPS="$MAPS+/Game/RestrictedAssets/Maps/DM-Chill"
MAPS="$MAPS+/Game/RestrictedAssets/Maps/DM-Outpost23"
MAPS="$MAPS+/Game/RestrictedAssets/Maps/DM-Underland"
MAPS="$MAPS+/Game/RestrictedAssets/Maps/UT-Entry"

echo "--- [1/2] full-scope cook (all maps + entire RestrictedAssets) ---"
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "$UE/UnrealTournament/Binaries/Linux/UnrealTournamentEditor-Cmd $UE/UnrealTournament/UnrealTournament.uproject -run=Cook -TargetPlatform=SimplyStream -Map=$MAPS -CookDir=$C -clean -unversioned -DisablePlugins=GPULightmass -AllowPartialShaderMaps -ini:Engine:[ConsoleVariables]:r.AreShaderErrorsFatal=0 -ini:Engine:[ConsoleVariables]:r.Shadow.Virtual.Enable=0 -ini:Engine:[ConsoleVariables]:r.BindlessResources=0 -ini:Engine:[ConsoleVariables]:r.BindlessSamplers=0"
RC=$?
echo "=== COOK EXITED rc=$RC at $(date -Is) ==="
echo "--- disk after cook ---"; df -h /mnt/vms | tail -1
echo "--- cooked maps ---"
ls "$UE/UnrealTournament/Saved/Cooked/SimplyStream/UnrealTournament/Content/RestrictedAssets/Maps/"*.umap 2>/dev/null | wc -l
echo "--- cooked package count (approx) ---"
find "$UE/UnrealTournament/Saved/Cooked/SimplyStream/UnrealTournament/Content" -name "*.uasset" -o -name "*.umap" 2>/dev/null | wc -l

echo "--- [2/2] repackage (IoStore + bootstrap incl. loose fonts) ---"
bash /mnt/vms/ss-build/postcook_package.sh
echo "postcook rc=$?"
echo "--- final pak + bundle sizes ---"
ls -la /mnt/vms/ss-build/ut-iostore/utcontent*.ucas 2>/dev/null
ls -la /mnt/vms/ss-build/ut-local-webgpu/ContentBootstrap*.data 2>/dev/null
md5sum /mnt/vms/ss-build/ut-local-webgpu/ContentBootstrap*.data 2>/dev/null
echo "=== FULL COOK CHAIN DONE $(date -Is) ==="
