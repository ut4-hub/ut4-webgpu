#!/usr/bin/env bash
# UBSLOT v2 chain (2026-08-18): sequential-UB-slot compiler fix + GUID bump ->
# rebuild ShaderCompileWorker (the process that ACTUALLY compiles shaders —
# round 1 failed because only the editor-side lib was rebuilt and SCW ran the
# stale one) + editor -> CLEAN full-scope cook (incremental cook does not
# re-save packages whose shaders changed) -> iostore repack + bootstrap regen.
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
LOG=/mnt/vms/ss-build/ubslot_recook3.log
exec >"$LOG" 2>&1
cd "$UE"
C="$UE/UnrealTournament/Content/RestrictedAssets"
echo "=== UBSLOT v2 chain started $(date -Is) ==="
echo "--- [1/4] rebuild ShaderCompileWorker + editor ---"
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "bash Engine/Build/BatchFiles/RunUBT.sh ShaderCompileWorker Linux Development -NoUBA -MaxParallelActions=8 && bash Engine/Build/BatchFiles/RunUBT.sh UnrealTournamentEditor Linux Development -NoUBA -MaxParallelActions=8"
RC=$?
echo "=== TOOL BUILDS rc=$RC $(date -Is) ==="
if [ $RC -ne 0 ]; then echo "=== CHAIN ABORT (tool build failed) ==="; exit 1; fi
echo "--- GUID sanity: SCW lib must carry C3F20814 ---"
strings "$UE/Engine/Binaries/Linux/libShaderCompileWorker-DawnShaderFormat.so" | grep -c "C3F20814" || true
echo "--- [2/4] CLEAN full-scope cook (morning recipe + UBSLOT v2 shaders) ---"
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "$UE/UnrealTournament/Binaries/Linux/UnrealTournamentEditor-Cmd $UE/UnrealTournament/UnrealTournament.uproject -run=Cook -TargetPlatform=SimplyStream -Map=/Game/RestrictedAssets/Maps/UT-Entry+/Game/RestrictedAssets/Maps/DM-Chill -CookDir=$C/Character -CookDir=$C/Weapons -CookDir=$C/Audio -CookDir=$C/Pickups -CookDir=$C/Effects -CookDir=$C/Blueprints -CookDir=$C/Functions -clean -unversioned -DisablePlugins=GPULightmass -AllowPartialShaderMaps -ini:Engine:[ConsoleVariables]:r.AreShaderErrorsFatal=0 -ini:Engine:[ConsoleVariables]:r.Shadow.Virtual.Enable=0 -ini:Engine:[ConsoleVariables]:r.BindlessResources=0 -ini:Engine:[ConsoleVariables]:r.BindlessSamplers=0"
RC=$?
echo "=== COOK EXITED rc=$RC at $(date -Is) ==="
echo "--- DM-Chill cooked check ---"
ls -la "$UE/UnrealTournament/Saved/Cooked/SimplyStream/UnrealTournament/Content/RestrictedAssets/Maps/" 2>/dev/null | grep -i "DM-Chill" | head -4
echo "--- [3/4] repack ---"
bash /mnt/vms/ss-build/postcook_package.sh
echo "postcook rc=$?"
echo "--- [4/4] inventory ---"
grep -c "DM-Chill" /mnt/vms/ss-build/ut-iostore/PakListIoStore_utcontent*.txt 2>/dev/null || true
ls -la /mnt/vms/ss-build/ut-local-webgpu/ContentBootstrap*.data /mnt/vms/ss-build/ut-iostore/*.ucas
md5sum "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.wasm" "$UE/UnrealTournament/Binaries/SimplyStream/UnrealTournamentwasm64.js" /mnt/vms/ss-build/ut-local-webgpu/ContentBootstrap*.data
echo "=== UBSLOT3 CHAIN DONE $(date -Is) ==="
