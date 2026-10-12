#!/usr/bin/env bash
# Dynamic IoStore container split: bin ALL cooked files by cumulative size into
# containers each <1.9GB COOKED (so each compressed .ucas is guaranteed <2GB, the
# emscripten file_packager per-file limit). Produces utcontent, utcontent2..N.
# Runtime mounts utcontent..utcontent12 (LaunchSimplyStream Tocs[]).
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

python3 - "$COOK" "$OUT" "$BUDGET" <<'PY'
import os,sys
COOK,OUT,BUDGET=sys.argv[1],sys.argv[2],int(sys.argv[3])
exts=('.uasset','.uexp','.ubulk','.umap','.uptnl')
files=[]
for root,_,fs in os.walk(COOK):
    for fn in fs:
        if fn.endswith(exts):
            p=os.path.join(root,fn); files.append((p,os.path.getsize(p)))
files.sort()
bins=[]; cur=[]; cursz=0
for p,sz in files:
    if cursz+sz>BUDGET and cur:
        bins.append(cur); cur=[]; cursz=0
    cur.append((p,sz)); cursz+=sz
if cur: bins.append(cur)
cmds=[]
for i,b in enumerate(bins,1):
    name='utcontent' if i==1 else 'utcontent%d'%i
    resp=os.path.join(OUT,'PakListIoStore_%s.txt'%name)
    with open(resp,'w') as fh:
        for p,sz in b:
            rel=os.path.relpath(p,COOK)
            fh.write('"%s" "../../../%s" -compress\n'%(p,rel))
    print('bin %2d %-12s %5d files  %5.2f GB cooked'%(i,name,len(b),sum(s for _,s in b)/1e9))
    cmds.append('-Output=%s/%s.utoc -ContainerName=%s -ResponseFile=%s'%(OUT,name,name,resp))
open(os.path.join(OUT,'IoStoreCommands.txt'),'w').write('\n'.join(cmds)+'\n')
print('TOTAL bins: %d (mount limit 12)'%len(bins))
if len(bins)>12: print('!!! MORE THAN 12 BINS — need to raise mount limit + rebuild')
PY

echo "=== running UnrealPak IoStore creation ($(date -Is)) ==="
nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "$UPAK $PROJ \
  -CreateGlobalContainer=$OUT/global.utoc \
  -PackageStoreManifest=$META/packagestore.manifest \
  -ScriptObjects=$META/scriptobjects.bin \
  -CookedDirectory=$COOK \
  -Commands=$OUT/IoStoreCommands.txt \
  -unattended" 2>&1 | tail -12
echo "=== produced containers (each MUST be <2GB) ==="
OVER=0
for f in "$OUT"/global.ucas "$OUT"/utcontent*.ucas; do
  [ -f "$f" ] || continue
  s=$(stat -c%s "$f")
  if [ "$s" -gt 2145386496 ]; then FLAG="  <<< OVER 2GB!"; OVER=1; else FLAG=""; fi
  printf "%6.0f MB  %s%s\n" "$((s/1048576))" "$(basename "$f")" "$FLAG"
done
echo "TOTAL: $(du -shc "$OUT"/*.ucas 2>/dev/null | tail -1 | cut -f1)   OVER_2GB=$OVER"
echo "=== MAKE_IOSTORE DONE $(date -Is) ==="
