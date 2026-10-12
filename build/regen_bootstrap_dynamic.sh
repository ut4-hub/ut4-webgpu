#!/usr/bin/env bash
# Dynamic bootstrap: one ContentBootstrap package per IoStore container so every
# browser .data download stays <2GB. Package 0 = engine data + global + loose
# fonts; packages 1..N = one utcontent container each.
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
COOK="$UE/UnrealTournament/Saved/Cooked/SimplyStream"
META="$COOK/UnrealTournament/Metadata"
IOS=/mnt/vms/ss-build/ut-iostore
OUT=/mnt/vms/ss-build/ut-local-webgpu
PACKAGER=/mnt/vms/ss-build/stock-emsdk/upstream/emscripten/tools/file_packager.py
mkdir -p "$OUT"
rm -f "$OUT"/ContentBootstrap*.data "$OUT"/ContentBootstrap*.js

echo "=== package 0: engine data + global + fonts ==="
python3 "$PACKAGER" "$OUT/ContentBootstrap.data" \
  --preload "$COOK/Engine/GlobalShaderCache-SP_WEBGPU_ES31.bin@Engine/GlobalShaderCache-None.bin" \
  --preload "$COOK/UnrealTournament/AssetRegistry.bin@UnrealTournament/AssetRegistry.bin" \
  --preload "$META/scriptobjects.bin@UnrealTournament/Metadata/scriptobjects.bin" \
  --preload "$UE/Engine/Platforms/SimplyStream/Config/DataDrivenPlatformInfo.ini@Engine/Platforms/SimplyStream/Config/DataDrivenPlatformInfo.ini" \
  --preload "$UE/Engine/Platforms/SimplyStream/Config/DataDrivenPlatformInfo.ini@Engine/Config/SimplyStream/DataDrivenPlatformInfo.ini" \
  --preload "$UE/Engine/Platforms/SimplyStream/Config/SimplyStreamEngine.ini@Engine/Platforms/SimplyStream/Config/SimplyStreamEngine.ini" \
  --preload "$UE/Engine/Config@Engine/Config" \
  --preload "$UE/Engine/Content/Slate@Engine/Content/Slate" \
  --preload "$UE/Engine/Content/SlateDebug@Engine/Content/SlateDebug" \
  --preload "$UE/UnrealTournament/Config@UnrealTournament/Config" \
  --preload "$UE/UnrealTournament/Content/RestrictedAssets/Slate@UnrealTournament/Content/RestrictedAssets/Slate" \
  --preload "$IOS/global.utoc@UnrealTournament/Content/Paks/global.utoc" \
  --preload "$IOS/global.ucas@UnrealTournament/Content/Paks/global.ucas" \
  --js-output="$OUT/ContentBootstrap.js" >/dev/null 2>&1
echo "pkg0 done ($(du -h "$OUT/ContentBootstrap.data" 2>/dev/null | cut -f1))"

i=1
for name in utcontent $(for t in "$IOS"/utcontent[0-9]*.utoc; do basename "$t" .utoc; done | sort -V); do
  toc="$IOS/$name.utoc"; ucas="$IOS/$name.ucas"
  [ -f "$toc" ] && [ -f "$ucas" ] || continue
  python3 "$PACKAGER" "$OUT/ContentBootstrap_$i.data" \
    --preload "$toc@UnrealTournament/Content/Paks/$name.utoc" \
    --preload "$ucas@UnrealTournament/Content/Paks/$name.ucas" \
    --js-output="$OUT/ContentBootstrap_$i.js" >/dev/null 2>&1
  echo "pkg $i: $name -> ContentBootstrap_$i.data ($(du -h "$OUT/ContentBootstrap_$i.data" 2>/dev/null | cut -f1))"
  i=$((i+1))
done
NPKG=$((i-1))
echo "=== $NPKG content packages + 1 base ==="
ls -la "$OUT"/ContentBootstrap*.data 2>/dev/null | awk '{printf "%6.0f MB  %s\n", $5/1048576, $9}'
echo "TOTAL bundle: $(du -shc "$OUT"/ContentBootstrap*.data 2>/dev/null | tail -1 | cut -f1)"
echo "SCRIPTS_NEEDED: ContentBootstrap.js $(for n in $(seq 1 $NPKG); do echo -n "ContentBootstrap_$n.js "; done)"
echo "=== REGEN DONE $(date -Is) ==="
