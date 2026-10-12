#!/bin/bash
exec >/mnt/vms/ss-build/cook_clean.log 2>&1
cd /mnt/vms/ss-build/UnrealEngine
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "/mnt/vms/ss-build/UnrealEngine/UnrealTournament/Binaries/Linux/UnrealTournamentEditor-Cmd /mnt/vms/ss-build/UnrealEngine/UnrealTournament/UnrealTournament.uproject -run=Cook -TargetPlatform=SimplyStream -Map=/Game/RestrictedAssets/Maps/UT-Entry -unversioned -DisablePlugins=GPULightmass -AllowPartialShaderMaps -ini:Engine:[ConsoleVariables]:r.AreShaderErrorsFatal=0"
echo "=== COOK EXITED rc=$? ==="
