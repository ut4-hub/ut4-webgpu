#!/usr/bin/env bash
cd /mnt/vms/ss-build/UnrealEngine
LOG=/mnt/vms/ss-build/verify_chain.log
echo "=== waiting for DawnCookProbe build $(date +%H:%M:%S) ===" > $LOG
while pgrep -f "RunUBT.sh DawnCookProbe" >/dev/null 2>&1; do sleep 30; done
if grep -aqE "Result: Succeeded" /mnt/vms/ss-build/rebuild_cookprobe.log; then
  echo "BUILD_OK $(date +%H:%M:%S)" >> $LOG
  ls -la Engine/Binaries/Linux/DawnCookProbe-Linux-Shipping >> $LOG 2>&1
  echo "=== running cook verify ===" >> $LOG
  nix-shell /mnt/vms/ss-build/ue-cook-shell.nix --run "bash /mnt/vms/ss-build/ut4-webgpu-push/tools/cook_real_shader.sh /mnt/vms/ss-build/UnrealEngine/Engine/Shaders/Private/NullPixelShader.usf Main ps /tmp/nullps.wgsl" >> $LOG 2>&1
  echo "COOK_EXIT=$?" >> $LOG
  echo "=== WGSL OUTPUT ===" >> $LOG
  cat /tmp/nullps.wgsl >> $LOG 2>&1
else
  echo "BUILD_FAILED $(date +%H:%M:%S)" >> $LOG
  tail -8 /mnt/vms/ss-build/rebuild_cookprobe.log >> $LOG 2>&1
fi
echo "=== chain done $(date +%H:%M:%S) ===" >> $LOG
