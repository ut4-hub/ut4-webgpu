#!/usr/bin/env bash
# "One map + everything else": include ALL cooked content EXCEPT the OTHER maps'
# environment art (Outside/Liandri/Industrial/FacingWorlds/Temple/Desert/Space/
# Fort/Tuba ~6.6GB). Keeps full base (all characters/weapons/cosmetics/monsters/
# effects) + DM-Chill's env + shared env. ~13GB cooked -> ~7GB compressed; fits a
# 16GB heap. Bins <1.9GB each. Runtime mounts utcontent..utcontent12.
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
# other maps' environment subdirs to EXCLUDE (keep Chill + shared env)
EXCLUDE="Environments/Outside
Environments/Liandri
Environments/Industrial
Environments/FacingWorlds
Environments/Temple
Environments/Desert
Environments/Space
Environments/Fort
Environments/Tuba"

python3 - "$COOK" "$OUT" "$BUDGET" "$EXCLUDE" <<'PY'
import os,sys
COOK,OUT,BUDGET=sys.argv[1],sys.argv[2],int(sys.argv[3])
excl=[e.strip() for e in sys.argv[4].splitlines() if e.strip()]
exclpaths=[os.path.join(COOK,'UnrealTournament/Content/RestrictedAssets',e) for e in excl]
exts=('.uasset','.uexp','.ubulk','.umap','.uptnl')
files=[]
for root,_,fs in os.walk(COOK):
    if any(root.startswith(x) for x in exclpaths): continue
    for fn in fs:
        if fn.endswith(exts):
            p=os.path.join(root,fn); files.append((p,os.path.getsize(p)))
files.sort()
bins=[]; cur=[]; cursz=0
for p,sz in files:
    if cursz+sz>BUDGET and cur: bins.append(cur); cur=[]; cursz=0
    cur.append((p,sz)); cursz+=sz
if cur: bins.append(cur)
cmds=[]; tot=0
for i,b in enumerate(bins,1):
    name='utcontent' if i==1 else 'utcontent%d'%i
    resp=os.path.join(OUT,'PakListIoStore_%s.txt'%name)
    with open(resp,'w') as fh:
        for p,sz in b: fh.write('"%s" "../../../%s" -compress\n'%(p,os.path.relpath(p,COOK)))
    s=sum(x for _,x in b); tot+=s
    print('bin %2d %-12s %5d files  %5.2f GB'%(i,name,len(b),s/1e9))
    cmds.append('-Output=%s/%s.utoc -ContainerName=%s -ResponseFile=%s'%(OUT,name,name,resp))
open(os.path.join(OUT,'IoStoreCommands.txt'),'w').write('\n'.join(cmds)+'\n')
print('TOTAL included: %.2f GB cooked, %d containers'%(tot/1e9,len(bins)))
if len(bins)>12: print('!!! >12 bins — raise mount limit')
PY

echo "=== UnrealPak IoStore ($(date -Is)) ==="
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "$UPAK $PROJ -CreateGlobalContainer=$OUT/global.utoc -PackageStoreManifest=$META/packagestore.manifest -ScriptObjects=$META/scriptobjects.bin -CookedDirectory=$COOK -Commands=$OUT/IoStoreCommands.txt -unattended" 2>&1 | tail -5
echo "=== containers ==="; du -shc "$OUT"/utcontent*.ucas 2>/dev/null | tail -1
ls -la "$OUT"/utcontent*.ucas 2>/dev/null | awk '{printf "%6.0f MB  %s\n",$5/1048576,$9}'
echo "MAKE_ONEMAP DONE"
