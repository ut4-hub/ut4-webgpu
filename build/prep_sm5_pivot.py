#!/usr/bin/env python3
# SM5 PIVOT prep: align the cook with the closed RHI's actual platform (SM5).
import io, sys

UE = "/mnt/vms/ss-build/UnrealEngine"

# 1) RenderMode WebGPUES31 -> WebGPUSM5 (cook targets SF_WEBGPU_SM5 to match the runtime RHI)
F1 = UE + "/Engine/Platforms/SimplyStream/Config/SimplyStreamEngine.ini"
s = io.open(F1, encoding="utf-8").read()
if "RenderMode=WebGPUES31" in s:
    s = s.replace("RenderMode=WebGPUES31", "RenderMode=WebGPUSM5", 1)
    io.open(F1, "w", encoding="utf-8").write(s)
    print("1) RenderMode -> WebGPUSM5")
elif "RenderMode=WebGPUSM5" in s:
    print("1) RenderMode already WebGPUSM5")
else:
    print("1) WARN: RenderMode line not found", file=sys.stderr)

# 2) Revert the ES31 feature-level override in LaunchSimplyStream (runtime is naturally SM5 now)
F2 = UE + "/Engine/Platforms/SimplyStream/Source/Runtime/Launch/Private/LaunchSimplyStream.cpp"
s = io.open(F2, encoding="utf-8").read()
inc = (
    '#include "Misc/AES.h"\n'
    '#if PLATFORM_WASM\n'
    '#include "RHIFeatureLevel.h"\n'
    '#include "RHIShaderPlatform.h"\n'
    '#include "RHIGlobals.h"\n'
    '#endif\n'
)
if inc in s:
    s = s.replace(inc, '#include "Misc/AES.h"\n', 1)
    print("2a) removed ES31 override includes")
force = (
    '\t\t// The closed WebGPU RHI creates an ES31 device but leaves GMaxRHIFeatureLevel at\n'
    '\t\t// SM6 (selects SP_WEBGPU_SM5). Material shader maps are cooked for SP_WEBGPU_ES31,\n'
    '\t\t// so the SM6 lookup misses the inline ES31 shaders (MaterialShared.cpp:3167 "No\n'
    '\t\t// inline shaders"). Force ES3_1 to match the cook before default materials load.\n'
    '\t\tGMaxRHIFeatureLevel = ERHIFeatureLevel::ES3_1;\n'
    '\t\t// SP_WEBGPU_ES31\'s Gen constexpr isn\'t visible in this TU; read the ES3_1 platform\n'
    '\t\t// from the DDPI-populated feature-level map, falling back to its numeric value (41).\n'
    '\t\tEShaderPlatform WndrES31Platform = GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1];\n'
    '\t\tif (WndrES31Platform == SP_NumPlatforms) { WndrES31Platform = static_cast<EShaderPlatform>(41); }\n'
    '\t\tGMaxRHIShaderPlatform = WndrES31Platform;\n'
    '\t\tGShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = WndrES31Platform;\n'
    '\t\tUE_LOG(LogInit, Display, TEXT("[wndr] forced GMaxRHIFeatureLevel=ES3_1, GMaxRHIShaderPlatform=%d"), (int)GMaxRHIShaderPlatform);\n'
)
if force in s:
    s = s.replace(force, "", 1)
    print("2b) removed ES31 force block")
elif "forced GMaxRHIFeatureLevel=ES3_1" in s:
    print("2b) WARN: force block present but text didn't match exactly", file=sys.stderr)
else:
    print("2b) force block already absent")
io.open(F2, "w", encoding="utf-8").write(s)

# 3) bump shader GUID (SM5 shaders are new content -> force real recompile, not DDC-served ES31)
F3 = UE + "/Engine/Platforms/SimplyStream/Shaders/Public/PlatformCommon.ush"
s = io.open(F3, encoding="utf-8").read()
if "8B5F3C41-6EA0-4C53-DF27-6B9EBD405C38" in s:
    s = s.replace("8B5F3C41-6EA0-4C53-DF27-6B9EBD405C38", "9C6A4D52-7FB1-4D64-EA38-7CAFCE516D49", 1)
    io.open(F3, "w", encoding="utf-8").write(s)
    print("3) shader GUID bumped")
else:
    print("3) WARN: expected GUID not found (may already be bumped)", file=sys.stderr)
print("PREP DONE")
