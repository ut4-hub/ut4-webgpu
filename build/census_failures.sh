#!/usr/bin/env bash
UE=/mnt/vms/ss-build/UnrealEngine
SCW=$UE/Engine/Binaries/Linux/ShaderCompileWorker
OUT=/mnt/vms/ss-build/failure_census.txt
: > $OUT
find $UE/UnrealTournament/Saved/ShaderDebugInfo/SP_WEBGPU_ES31 -name DebugCompile.in | while read f; do
  d=$(dirname "$f")
  rel=${d#$UE/UnrealTournament/Saved/ShaderDebugInfo/SP_WEBGPU_ES31/}
  src=$(ls "$d" | grep -v -e "^Debug" -e "^Output" | head -1)
  banner=$(cd "$d" && timeout 120 "$SCW" "$d/" 0 DebugCompile DebugCompile.in DebugCompile.out -DebugSourceFiles="$src" -TimeToLive=0.0f -KeepInput 2>&1 | grep -m2 "internal compiler error\|Tint SPIR-V->WGSL failed\|WgslFromIR\|ReadIR" | head -2 | tr "\n" ";")
  echo "$rel | $banner" >> $OUT
done
echo CENSUS_DONE
wc -l $OUT
