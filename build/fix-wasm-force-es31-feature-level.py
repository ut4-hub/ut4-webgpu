#!/usr/bin/env python3
# THE FEATURE-LEVEL MISMATCH FIX (2026-08-13) — final root cause of MaterialShared.cpp:3167.
#
# The runtime fataled on WorldGridMaterial "No inline shaders, null GTSM" even
# though the material cooks correctly with INLINE shaders. Root cause: a feature-
# level mismatch. The closed WebGPU RHI creates an ES31 *device*
# (LogWebGPURHI "featureLevel = ES31") but leaves GMaxRHIFeatureLevel at SM6
# (rhifeaturelevel="SM6"): per DDPI it selects SP_WEBGPU_SM5 (MaxFeatureLevel=SM6)
# as GMaxRHIShaderPlatform. Material shader maps are cooked for SP_WEBGPU_ES31
# (MaxFeatureLevel=ES3_1), so the runtime looks up the SM6 platform's map, misses
# the inline ES31 shaders, and reports "No inline shaders" -> null -> Fatal. (Also
# explains the DataDrivenShaderPlatformInfo IsValid(Platform) breaks.)
#
# Fix: force GMaxRHIFeatureLevel=ES3_1 + GMaxRHIShaderPlatform=SP_WEBGPU_ES31 (and
# the feature-level->platform map entry) after the RHI inits in
# PreInitPreStartupScreen but BEFORE PreInitPostStartupScreen loads content
# (default materials). Everything is ES3.1 (device, global shader cache, material
# shaders) so this makes the runtime consistent with the cook.
#
# Runtime SimplyStream Launch module -> wasm rebuild + atomics shim + pull wasm/js. No recook.
import io, sys

F = "/mnt/vms/ss-build/UnrealEngine/Engine/Platforms/SimplyStream/Source/Runtime/Launch/Private/LaunchSimplyStream.cpp"
s = io.open(F, encoding="utf-8").read()

if "forced GMaxRHIFeatureLevel=ES3_1" in s:
    print("already patched"); sys.exit(0)

# 1) includes for the RHI globals (idempotent)
inc_anchor = '#include "Misc/AES.h"\n'
inc_new = (
    '#include "Misc/AES.h"\n'
    '#if PLATFORM_WASM\n'
    '#include "RHIFeatureLevel.h"\n'
    '#include "RHIShaderPlatform.h"\n'
    '#include "RHIGlobals.h"\n'
    '#endif\n'
)
if inc_anchor in s and '#include "RHIFeatureLevel.h"' not in s:
    s = s.replace(inc_anchor, inc_new, 1)

# 2) force ES3_1 right after mounting containers, before PreInitPostStartupScreen
call_anchor = (
    '\t\tSimplyStream_MountIoStoreContainers();\n'
    '\t\tGEngineLoop.PreInitPostStartupScreen(GCmdLine);\n'
)
call_new = (
    '\t\tSimplyStream_MountIoStoreContainers();\n'
    '\t\t// The closed WebGPU RHI creates an ES31 device but leaves GMaxRHIFeatureLevel at\n'
    '\t\t// SM6 (selects SP_WEBGPU_SM5). Material shader maps are cooked for SP_WEBGPU_ES31,\n'
    '\t\t// so the SM6 lookup misses the inline ES31 shaders (MaterialShared.cpp:3167 "No\n'
    '\t\t// inline shaders"). Force ES3_1 to match the cook before default materials load.\n'
    '\t\tGMaxRHIFeatureLevel = ERHIFeatureLevel::ES3_1;\n'
    '\t\tGMaxRHIShaderPlatform = SP_WEBGPU_ES31;\n'
    '\t\tGShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_WEBGPU_ES31;\n'
    '\t\tUE_LOG(LogInit, Display, TEXT("[wndr] forced GMaxRHIFeatureLevel=ES3_1, GMaxRHIShaderPlatform=%d"), (int)GMaxRHIShaderPlatform);\n'
    '\t\tGEngineLoop.PreInitPostStartupScreen(GCmdLine);\n'
)
if call_anchor not in s:
    print("call anchor (mount + PreInitPostStartupScreen) not found", file=sys.stderr); sys.exit(1)
s = s.replace(call_anchor, call_new, 1)

io.open(F, "w", encoding="utf-8").write(s)
print("patched LaunchSimplyStream.cpp: force ES3_1 feature level / SP_WEBGPU_ES31 on wasm")
