#!/usr/bin/env bash
# Wait for editor rebuild -> verify -> recook (DDC-served, no GUID bump) -> report.
BL=/mnt/vms/ss-build/rebuild_editor_engine.log
CL=/mnt/vms/ss-build/cook_es31.log
COOK=/mnt/vms/ss-build/UnrealEngine/UnrealTournament/Saved/Cooked/SimplyStream
OUT=/mnt/vms/ss-build/orchestrate.out
exec >"$OUT" 2>&1
echo "=== waiting for editor build $(date -Is) ==="
until grep -q "EDITOR BUILD EXITED" "$BL" 2>/dev/null; do sleep 30; done
RC=$(grep -oE "EDITOR BUILD EXITED rc=[0-9]+" "$BL" | tail -1)
echo "editor build: $RC"
if ! echo "$RC" | grep -q "rc=0"; then
  echo "EDITOR BUILD FAILED -- errors:"
  grep -iE "error:|GeometryCollectionRendering|IsMobilePlatform|undeclared" "$BL" | head -15
  echo "=== ORCHESTRATION DONE (build-fail) ==="
  exit 1
fi
echo "=== editor OK; launching recook (DDC-served) $(date -Is) ==="
cd /mnt/vms/ss-build && bash run_webgpu_es31_cook.sh
echo "=== cook finished $(date -Is) ==="
echo "--- WorldGridMaterial ---"
grep -c "Failed to compile default material /Engine/EngineMaterials/WorldGridMaterial" "$CL" 2>/dev/null | sed "s/^/WorldGrid fail count: /"
echo "--- shader library emitted? ---"
find "$COOK" -iname "*.ushaderbytecode" -o -iname "ShaderArchive*" 2>/dev/null | head
echo "--- material null-shadermap warnings ---"
grep -c "doesnt have a valid ShaderMap" "$CL" 2>/dev/null
echo "=== ORCHESTRATION DONE ==="
