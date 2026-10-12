#!/usr/bin/env bash
# restart the wasm build cleanly (kills running UBT + rebuild script)
for p in $(pgrep -f "[U]nrealBuildTool.dll") $(pgrep -f "bash [r]ebuild_wasm3.sh"); do kill $p 2>/dev/null; done
sleep 6
cd /mnt/vms/ss-build && setsid nohup bash rebuild_wasm3.sh >/dev/null 2>&1 < /dev/null &
sleep 1; head -1 /mnt/vms/ss-build/rebuild_wasm3.log
