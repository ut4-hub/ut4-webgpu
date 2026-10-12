#!/usr/bin/env bash
# Run AFTER cook completes: rebuild IoStore containers from the fresh cook, then
# regen the ContentBootstrap bundle (with the DDPI-consistent global shader cache).
set -uo pipefail
cd /mnt/vms/ss-build
exec > /mnt/vms/ss-build/postcook_package.out 2>&1
echo "=== postcook: make_iostore_containers $(date -Is) ==="
bash ut4-webgpu-push/engine-patches/make_iostore_containers.sh
echo "iostore rc=$?"
echo "=== postcook: regen-bootstrap-iostore $(date -Is) ==="
bash ut4-webgpu-push/engine-patches/regen-bootstrap-iostore.sh
echo "regen rc=$?"
echo "=== new global shader cache mtime ==="
ls -la --time-style=+"%m-%d_%H:%M" UnrealEngine/UnrealTournament/Saved/Cooked/SimplyStream/Engine/GlobalShaderCache-SP_WEBGPU_SM5.bin
echo "=== new ContentBootstrap.data ==="
ls -la /mnt/vms/ss-build/ut-local-webgpu/ContentBootstrap.data
echo "=== POSTCOOK DONE $(date -Is) ==="
