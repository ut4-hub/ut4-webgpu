#!/usr/bin/env bash
# DM-Chill-scoped containers from the FULL cook: include DM-Chill's environment +
# shared art + characters (textured), EXCLUDE the heavy stuff that doesn't fit
# (Weapons ~2.3GB, other maps' environments ~6GB, Monsters, Cosmetics). Bins
# <1.9GB each so they package for the browser. Target ~3.5GB compressed -> fits.
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
COOK="$UE/UnrealTournament/Saved/Cooked/SimplyStream"
META="$COOK/UnrealTournament/Metadata"
OUT=/mnt/vms/ss-build/ut-iostore
UPAK="$UE/Engine/Binaries/Linux/UnrealPak"
PROJ="$UE/UnrealTournament/UnrealTournament.uproject"
mkdir -p "$OUT"
rm -f "$OUT"/PakListIoStore_*.txt "$OUT"/utcontent*.utoc "$OUT"/utcontent*.ucas "$OUT"/IoStoreCommands.txt "$OUT"/global.utoc "$OUT"/global.ucas
BUDGET=$((1900*1024*1024))

INCLUDE="Engine/Content
Engine/Plugins
UnrealTournament/Content/RestrictedAssets/Maps
UnrealTournament/Content/RestrictedAssets/Environments/Chill
UnrealTournament/Content/RestrictedAssets/Environments/Textures
UnrealTournament/Content/RestrictedAssets/Environments/Rocks
UnrealTournament/Content/RestrictedAssets/Environments/Skies
UnrealTournament/Content/RestrictedAssets/Environments/Statues
UnrealTournament/Content/RestrictedAssets/Environments/Stone
UnrealTournament/Content/RestrictedAssets/Environments/AutomotiveMaterials
UnrealTournament/Content/RestrictedAssets/Materials
UnrealTournament/Content/RestrictedAssets/MaterialLayers
UnrealTournament/Content/RestrictedAssets/MaterialLayerAssets
UnrealTournament/Content/RestrictedAssets/PhysicalMaterials
UnrealTournament/Content/RestrictedAssets/Meshes
UnrealTournament/Content/RestrictedAssets/Textures
UnrealTournament/Content/RestrictedAssets/Character
UnrealTournament/Content/RestrictedAssets/Animations
UnrealTournament/Content/RestrictedAssets/Effects
UnrealTournament/Content/RestrictedAssets/Audio
UnrealTournament/Content/RestrictedAssets/Pickups
UnrealTournament/Content/RestrictedAssets/Blueprints
UnrealTournament/Content/RestrictedAssets/Functions
UnrealTournament/Content/RestrictedAssets/UI
UnrealTournament/Content/RestrictedAssets/SlateLargeImages"

python3 - "$COOK" "$OUT" "$BUDGET" "$INCLUDE" <<'PY'
import os,sys
COOK,OUT,BUDGET=sys.argv[1],sys.argv[2],int(sys.argv[3])
includes=[d for d in sys.argv[4].splitlines() if d.strip()]
exts=('.uasset','.uexp','.ubulk','.umap','.uptnl')
files=[]
for d in includes:
    base=os.path.join(COOK,d)
    for root,_,fs in os.walk(base):
        for fn in fs:
            if fn.endswith(exts):
                p=os.path.join(root,fn); files.append((p,os.path.getsize(p)))
files.sort()
bins=[]; cur=[]; cursz=0
for p,sz in files:
    if cursz+sz>BUDGET and cur: bins.append(cur); cur=[]; cursz=0
    cur.append((p,sz)); cursz+=sz
if cur: bins.append(cur)
cmds=[]
tot=0
for i,b in enumerate(bins,1):
    name='utcontent' if i==1 else 'utcontent%d'%i
    resp=os.path.join(OUT,'PakListIoStore_%s.txt'%name)
    with open(resp,'w') as fh:
        for p,sz in b:
            fh.write('"%s" "../../../%s" -compress\n'%(p,os.path.relpath(p,COOK)))
    s=sum(x for _,x in b); tot+=s
    print('bin %2d %-12s %5d files  %5.2f GB'%(i,name,len(b),s/1e9))
    cmds.append('-Output=%s/%s.utoc -ContainerName=%s -ResponseFile=%s'%(OUT,name,name,resp))
open(os.path.join(OUT,'IoStoreCommands.txt'),'w').write('\n'.join(cmds)+'\n')
print('TOTAL included: %.2f GB cooked, %d containers'%(tot/1e9,len(bins)))
PY

echo "=== UnrealPak IoStore creation ($(date -Is)) ==="
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "$UPAK $PROJ -CreateGlobalContainer=$OUT/global.utoc -PackageStoreManifest=$META/packagestore.manifest -ScriptObjects=$META/scriptobjects.bin -CookedDirectory=$COOK -Commands=$OUT/IoStoreCommands.txt -unattended" 2>&1 | tail -6
echo "=== containers ==="
du -shc "$OUT"/utcontent*.ucas 2>/dev/null | tail -1
ls -la "$OUT"/utcontent*.ucas 2>/dev/null | awk '{printf "%6.0f MB  %s\n", $5/1048576, $9}'
echo "MAKE_CHILL DONE"
