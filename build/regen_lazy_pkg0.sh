#!/usr/bin/env bash
# LAZY-STREAMING package 0: preload only engine data + global + ALL .utoc indexes.
# The big utcontent*.ucas are NOT packaged — they're fetched on demand via
# createLazyFile (HTTP range requests), so memory tracks only what the map reads.
set -uo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
COOK="$UE/UnrealTournament/Saved/Cooked/SimplyStream"
META="$COOK/UnrealTournament/Metadata"
IOS=/mnt/vms/ss-build/ut-iostore
OUT=/mnt/vms/ss-build/ut-serve
PACKAGER=/mnt/vms/ss-build/stock-emsdk/upstream/emscripten/tools/file_packager.py

ARGS=(
  "$OUT/ContentBootstrap.data"
  --preload "$COOK/Engine/GlobalShaderCache-SP_WEBGPU_ES31.bin@Engine/GlobalShaderCache-None.bin"
  --preload "$COOK/UnrealTournament/AssetRegistry.bin@UnrealTournament/AssetRegistry.bin"
  --preload "$META/scriptobjects.bin@UnrealTournament/Metadata/scriptobjects.bin"
  --preload "$UE/Engine/Platforms/SimplyStream/Config/DataDrivenPlatformInfo.ini@Engine/Platforms/SimplyStream/Config/DataDrivenPlatformInfo.ini"
  --preload "$UE/Engine/Platforms/SimplyStream/Config/DataDrivenPlatformInfo.ini@Engine/Config/SimplyStream/DataDrivenPlatformInfo.ini"
  --preload "$UE/Engine/Platforms/SimplyStream/Config/SimplyStreamEngine.ini@Engine/Platforms/SimplyStream/Config/SimplyStreamEngine.ini"
  --preload "$UE/Engine/Config@Engine/Config"
  --preload "$UE/Engine/Content/Slate@Engine/Content/Slate"
  --preload "$UE/Engine/Content/SlateDebug@Engine/Content/SlateDebug"
  --preload "$UE/UnrealTournament/Config@UnrealTournament/Config"
  --preload "$UE/UnrealTournament/Content/RestrictedAssets/Slate@UnrealTournament/Content/RestrictedAssets/Slate"
  --preload "$IOS/global.utoc@UnrealTournament/Content/Paks/global.utoc"
  --preload "$IOS/global.ucas@UnrealTournament/Content/Paks/global.ucas"
)
for t in "$IOS"/utcontent.utoc "$IOS"/utcontent[0-9]*.utoc; do
  [ -f "$t" ] || continue
  n=$(basename "$t")
  ARGS+=( --preload "$t@UnrealTournament/Content/Paks/$n" )
done
ARGS+=( --js-output="$OUT/ContentBootstrap.js" )

rm -f "$OUT"/ContentBootstrap*.data "$OUT"/ContentBootstrap*.js
python3 "$PACKAGER" "${ARGS[@]}" 2>&1 | tail -4
echo "=== package 0 (indexes only) ==="
ls -la "$OUT/ContentBootstrap.data" 2>/dev/null | awk '{printf "%.1f MB  %s\n", $5/1048576, $9}'
echo "=== .ucas available for lazy fetch ($(ls -L "$OUT"/utcontent*.ucas 2>/dev/null | wc -l)) ==="
echo "DONE"
