#!/usr/bin/env bash
CL=/mnt/vms/ss-build/cook_es31.log
COOK=/mnt/vms/ss-build/UnrealEngine/UnrealTournament/Saved/Cooked/SimplyStream
for n in $(seq 1 70); do grep -q "COOK EXITED" "$CL" 2>/dev/null && break; sleep 20; done
echo "=== cook: $(grep "COOK EXITED" "$CL" | tail -1)"
echo "WorldGrid fail: $(grep -c "Failed to compile default material /Engine/EngineMaterials/WorldGridMaterial" "$CL" 2>/dev/null)"
echo "=== did Slate shaders recompile without bindless? (BindlessSRV gone from new cache) ==="
CACHE="$COOK/Engine/GlobalShaderCache-SP_WEBGPU_SM5.bin"
echo "BindlessSRV_SlateElement in new cache: $(strings "$CACHE" 2>/dev/null | grep -c BindlessSRV_SlateElement)"
echo "=== rebuild container + bootstrap ==="
cd /mnt/vms/ss-build
bash ut4-webgpu-push/engine-patches/make_iostore_containers.sh 2>&1 | grep -E "response file entries|Success" | tail -2
bash ut4-webgpu-push/engine-patches/regen-bootstrap-iostore.sh 2>&1 | tail -1
echo "=== BINDLESS-RECOOK DONE ==="
