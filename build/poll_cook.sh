#!/usr/bin/env bash
LOG=/mnt/vms/ss-build/cook_es31.log
COOK=/mnt/vms/ss-build/UnrealEngine/UnrealTournament/Saved/Cooked/SimplyStream
for n in $(seq 1 70); do
  if grep -q "COOK EXITED" "$LOG" 2>/dev/null; then break; fi
  sleep 20
done
echo "=== COOK STATUS ==="; grep "COOK EXITED" "$LOG" 2>/dev/null || echo "(still running)"
echo "=== WorldGridMaterial compile result ==="
grep -iE "Failed to compile.*WorldGridMaterial|WorldGridMaterial.*Default Material will be used" "$LOG" 2>/dev/null | tail -2
grep -c "Failed to compile default material /Engine/EngineMaterials/WorldGridMaterial" "$LOG" 2>/dev/null | sed "s/^/  WorldGrid fail count: /"
echo "=== remaining WgslFromIR failure reasons ==="
grep -oE "WgslFromIR: error: [^\"]*" "$LOG" 2>/dev/null | sed -E "s/[0-9]+/N/g" | sort | uniq -c | sort -rn | head
echo "=== dpdx/derivative failures remaining ==="
grep -c "dpd. .must only be called\|Fwidth.*uniform" "$LOG" 2>/dev/null
echo "=== material shader library emitted? ==="
find "$COOK" -iname "*.ushaderbytecode" -o -iname "ShaderArchive*" 2>/dev/null | head
echo "=== ShaderLibraryCooking errors ==="
grep -iE "ShaderLibraryCooking.cpp:160|BeginCookFinishShaderCodeLibrary|SaveGlobalShaderMapFiles" "$LOG" 2>/dev/null | grep -v Callstack | tail -3
