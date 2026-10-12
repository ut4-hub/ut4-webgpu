#!/usr/bin/env bash
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
LOG=/mnt/vms/ss-build/cook_es31.log
exec >"$LOG" 2>&1
cd "$UE"
C="$UE/UnrealTournament/Content/RestrictedAssets"
echo "=== ES3.1 FULL gameplay cook (maps + character/weapons/audio/gameplay) started $(date -Is) ==="
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "$UE/UnrealTournament/Binaries/Linux/UnrealTournamentEditor-Cmd $UE/UnrealTournament/UnrealTournament.uproject -run=Cook -TargetPlatform=SimplyStream -Map=/Game/RestrictedAssets/Maps/UT-Entry+/Game/RestrictedAssets/Maps/DM-Chill -CookDir=$C/Character -CookDir=$C/Weapons -CookDir=$C/Audio -CookDir=$C/Pickups -CookDir=$C/Effects -CookDir=$C/Blueprints -CookDir=$C/Functions -clean -unversioned -DisablePlugins=GPULightmass -AllowPartialShaderMaps -ini:Engine:[ConsoleVariables]:r.AreShaderErrorsFatal=0 -ini:Engine:[ConsoleVariables]:r.Shadow.Virtual.Enable=0 -ini:Engine:[ConsoleVariables]:r.BindlessResources=0 -ini:Engine:[ConsoleVariables]:r.BindlessSamplers=0"
rc=$?
echo "=== COOK EXITED rc=$rc at $(date -Is) ==="
exit "$rc"
