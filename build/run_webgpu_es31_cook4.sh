#!/usr/bin/env bash
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
LOG=/mnt/vms/ss-build/cook_es31_fix4.log
exec >"$LOG" 2>&1
cd "$UE"
echo "=== ES3.1 recook (bridge viewport-strip; GUID 6E3D) started $(date -Is) ==="
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "$UE/UnrealTournament/Binaries/Linux/UnrealTournamentEditor-Cmd $UE/UnrealTournament/UnrealTournament.uproject -run=Cook -TargetPlatform=SimplyStream -Map=/Game/RestrictedAssets/Maps/UT-Entry -clean -unversioned -DisablePlugins=GPULightmass -AllowPartialShaderMaps -ini:Engine:[ConsoleVariables]:r.AreShaderErrorsFatal=0"
echo "=== COOK EXITED rc=$? at $(date -Is) ==="
