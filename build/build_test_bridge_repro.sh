#!/usr/bin/env bash
# Build the isolated bridge regression repro (see test_bridge_repro.cpp).
set -euo pipefail
UE=/mnt/vms/ss-build/UnrealEngine
D=$UE/Engine/Source/ThirdParty/DawnTint
TC=$UE/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v26_clang-20.1.8-rockylinux8/x86_64-unknown-linux-gnu
CXX=$TC/bin/clang++
REPO=/mnt/vms/ss-build/ut4-webgpu-push
SRC=${1:-/mnt/vms/ss-build/test_bridge_repro.cpp}
OUT=/mnt/vms/ss-build/test_bridge_repro

# Only spirv-tools (assembler) is linked statically; the bridge itself is dlopen'd.
mapfile -t LIBS < <(find "$D/lib/Linux" -name "libSPIRV-Tools*.a")
"$CXX" -std=c++20 --sysroot="$TC" -nostdinc++ -isystem "$TC/include/c++/v1" \
  -I"$REPO/tools" \
  -I"$D/src/third_party/spirv-headers/src/include" \
  -I"$D/src/third_party/spirv-tools/src/include" \
  -o "$OUT" "$SRC" \
  -Wl,--start-group "${LIBS[@]}" -Wl,--end-group \
  -static-libstdc++ -lc++ -lc++abi -lpthread -ldl
echo REPRO_BUILD_OK
