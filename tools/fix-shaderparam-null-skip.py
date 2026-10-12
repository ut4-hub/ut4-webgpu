#!/usr/bin/env python3
# wndr LG-fix-1: on PLATFORM_WASM, skip NULL resource parameters in
# ExtractShaderParameterResources instead of dereferencing them. In wasm a NULL
# RDG view doesn't trap -- its vtable is read from address 0 and call_indirect
# dies with "table index is out of bounds" (seen in FLightGridInjectionCS when
# r.Mobile.Forward.EnableLocalLights=1). DawnRHI fills unbound slots with dummies.
import sys, shutil
P = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/RenderCore/Private/ShaderParameterStruct.cpp"
s = open(P).read()
if "LG-fix-1" in s:
    print("already applied"); sys.exit(0)
shutil.copy(P, P + ".wndrbak_lgfix1")
old = """			if (ResourceExclusions.Contains(Parameter.BaseIndex))
			{
				continue;
			}

			const FRHIShaderParameterResource ShaderParameterResource = ExtractShaderParameterResource(Reader, Parameter);
			BatchedParameters.AddResourceParameter(ShaderParameterResource);"""
new = """			if (ResourceExclusions.Contains(Parameter.BaseIndex))
			{
				continue;
			}
#if PLATFORM_WASM
			// wndr LG-fix-1: NULL resource -> skip (DawnRHI binds a dummy).
			if (Reader.Read<void*>(Parameter) == nullptr)
			{
				static int32 GWndrNullResLogs = 0;
				if (GWndrNullResLogs++ < 40)
				{
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[wndr LG-fix-1] NULL resource param skipped: byteOffset=%u baseIndex=%u baseType=%u\\n"),
						(uint32)Parameter.ByteOffset, (uint32)Parameter.BaseIndex, (uint32)Parameter.BaseType);
				}
				continue;
			}
#endif

			const FRHIShaderParameterResource ShaderParameterResource = ExtractShaderParameterResource(Reader, Parameter);
			BatchedParameters.AddResourceParameter(ShaderParameterResource);"""
assert s.count(old) == 1, "anchor count %d" % s.count(old)
s = s.replace(old, new, 1)
open(P, "w").write(s)
print("applied LG-fix-1")
