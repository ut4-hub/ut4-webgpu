#!/usr/bin/env python3
# MATERIAL-SHADER SURVIVAL FIX (2026-08-13), issue #1.
#
# WGSL forbids implicit-LOD texture sampling (textureSample, which computes
# derivatives) outside *uniform* control flow, and Dawn enforces it at runtime
# too. UE material shaders call Texture2DSample inside if/loop branches
# constantly, so DXC emits OpImageSampleImplicitLod in non-uniform flow and
# Tint's WgslFromIR rejects the whole module:
#   "'textureSample' must only be called from uniform control flow"
# This was THE dominant reason material shader maps failed to cook for
# SP_WEBGPU_ES31 (the cook then emitted NO material shader library at all ->
# WorldGridMaterial null GTSM -> MaterialShared.cpp:3167 fatal at runtime).
#
# Fix = a word-level SPIR-V pass (same style as StripViewportIndexLayer) that
# rewrites every implicit-LOD image sample to an EXPLICIT-LOD sample at Lod 0.0
# (textureSampleLevel), which needs no derivatives and is legal anywhere. For
# menu/UI content (full-res texture reads) losing derivative mip-selection is
# visually moot; this is the standard fallback when a target can't take implicit
# LOD. Handles Sample/SampleDref/SampleProj/SampleProjDref; preserves
# ConstOffset/Offset; drops Bias/MinLod (incompatible with an explicit Lod);
# injects a `float 0.0` constant if the module lacks one.
#
# Runs in DawnLegalizeAndCookImpl right after StripViewportIndexLayer, before
# SpirvToWgsl. Rebuild with engine-patches/build_dawn_tint_bridge_so.sh, then
# bump UESHADERMETADATA_VERSION and recook.
import io, sys

F = "/mnt/vms/ss-build/ut4-webgpu-push/tools/dawn_tint_bridge.cpp"
s = io.open(F, encoding="utf-8").read()

if "ForceExplicitLodSamples" in s:
    print("already patched"); sys.exit(0)

FUNC = r'''	// WGSL forbids implicit-LOD texture sampling (textureSample) outside uniform
	// control flow, and Dawn enforces it at runtime too -- UE material shaders
	// call Texture2DSample inside if/loop branches constantly, so DXC's
	// OpImageSample*ImplicitLod lands in non-uniform flow and Tint's WgslFromIR
	// rejects the module ("'textureSample' must only be called from uniform
	// control flow"), the dominant reason material shader maps failed to cook for
	// WebGPU. Rewrite every implicit-LOD image sample to an explicit-LOD sample at
	// Lod 0.0 (textureSampleLevel), which needs no derivatives and is legal
	// anywhere. Menu/UI content samples full-res textures so losing derivative
	// mip-selection is visually moot. Handles Sample/SampleDref/SampleProj/
	// SampleProjDref; preserves ConstOffset/Offset; drops Bias/MinLod; injects a
	// `float 0.0` constant if none exists.
	void ForceExplicitLodSamples(std::vector<uint32_t>& Spirv)
	{
		if (Spirv.size() < 5) return;
		const uint32_t OpTypeFloat = 22, OpConstant = 43, OpFunction = 54;
		auto ClassifyImplicit = [](uint32_t op, uint32_t& TargetOp, uint32_t& NFixed) -> bool {
			switch (op) {
				case 87: TargetOp = 88; NFixed = 4; return true; // OpImageSampleImplicitLod
				case 89: TargetOp = 90; NFixed = 5; return true; // OpImageSampleDrefImplicitLod (+Dref)
				case 91: TargetOp = 92; NFixed = 4; return true; // OpImageSampleProjImplicitLod
				case 93: TargetOp = 94; NFixed = 5; return true; // OpImageSampleProjDrefImplicitLod (+Dref)
				default: return false;
			}
		};
		// Pass 1: float32 type id, an existing float 0.0 constant, presence of a
		// function + any implicit sample.
		uint32_t FloatTypeId = 0, ZeroConstId = 0, T = 0, N = 0;
		bool AnyImplicit = false, HasFunc = false;
		size_t i = 5;
		while (i < Spirv.size()) {
			const uint32_t w0 = Spirv[i], op = w0 & 0xFFFFu, wc = w0 >> 16;
			if (wc == 0 || i + wc > Spirv.size()) break;
			if (op == OpTypeFloat && wc >= 3 && Spirv[i + 2] == 32u) FloatTypeId = Spirv[i + 1];
			if (op == OpConstant && wc == 4 && FloatTypeId != 0 && Spirv[i + 1] == FloatTypeId && Spirv[i + 3] == 0u) ZeroConstId = Spirv[i + 2];
			if (op == OpFunction) HasFunc = true;
			if (ClassifyImplicit(op, T, N)) AnyImplicit = true;
			i += wc;
		}
		if (!AnyImplicit || FloatTypeId == 0 || !HasFunc) return;
		const bool bInject = (ZeroConstId == 0);
		if (bInject) { ZeroConstId = Spirv[3]; Spirv[3] = ZeroConstId + 1u; } // new id from bound, bump bound
		// Pass 2: rebuild -- inject the constant before the first function and
		// rewrite each implicit sample to explicit Lod 0.0.
		std::vector<uint32_t> Out; Out.reserve(Spirv.size() + 8);
		Out.insert(Out.end(), Spirv.begin(), Spirv.begin() + 5);
		bool bInjected = !bInject;
		i = 5;
		while (i < Spirv.size()) {
			const uint32_t w0 = Spirv[i], op = w0 & 0xFFFFu, wc = w0 >> 16;
			if (wc == 0 || i + wc > Spirv.size()) { Out.insert(Out.end(), Spirv.begin() + i, Spirv.end()); break; }
			if (!bInjected && op == OpFunction) {
				Out.push_back((4u << 16) | OpConstant); Out.push_back(FloatTypeId); Out.push_back(ZeroConstId); Out.push_back(0u);
				bInjected = true;
			}
			uint32_t TargetOp = 0, NFixed = 0;
			if (ClassifyImplicit(op, TargetOp, NFixed) && wc >= 1 + NFixed) {
				uint32_t constOffId = 0, offId = 0;
				const size_t maskIdx = i + 1 + NFixed;
				if (wc > 1 + NFixed) {
					const uint32_t mask = Spirv[maskIdx];
					size_t opnd = maskIdx + 1;
					if (mask & 0x1u) opnd += 1;                                              // Bias -> drop
					if (mask & 0x2u) opnd += 1;                                              // Lod (unexpected)
					if (mask & 0x4u) opnd += 2;                                              // Grad (unexpected)
					if ((mask & 0x8u)  && opnd < i + wc) { constOffId = Spirv[opnd]; opnd += 1; } // ConstOffset -> keep
					if ((mask & 0x10u) && opnd < i + wc) { offId      = Spirv[opnd]; opnd += 1; } // Offset -> keep
					// higher bits (ConstOffsets/Sample/MinLod) unread -> dropped
				}
				uint32_t newMask = 0x2u; // Lod
				std::vector<uint32_t> ops; ops.push_back(ZeroConstId);
				if (constOffId) { newMask |= 0x8u;  ops.push_back(constOffId); }
				if (offId)      { newMask |= 0x10u; ops.push_back(offId); }
				const uint32_t newWc = 1u + NFixed + 1u + (uint32_t)ops.size();
				Out.push_back((newWc << 16) | TargetOp);
				for (uint32_t k = 1; k <= NFixed; ++k) Out.push_back(Spirv[i + k]);
				Out.push_back(newMask);
				for (uint32_t o : ops) Out.push_back(o);
				i += wc; continue;
			}
			Out.insert(Out.end(), Spirv.begin() + i, Spirv.begin() + i + wc);
			i += wc;
		}
		Spirv = std::move(Out);
	}

'''

# Insert the function immediately before DawnLegalizeAndCookImpl (same scope as the Strip* helpers).
anchor = "static FDawnTintCookResult DawnLegalizeAndCookImpl("
if anchor not in s:
    print("impl anchor not found", file=sys.stderr); sys.exit(1)
s = s.replace(anchor, FUNC + anchor, 1)

# Insert the call right after StripViewportIndexLayer(Spirv);
call_anchor = "\tStripViewportIndexLayer(Spirv);\n"
if call_anchor not in s:
    print("call anchor (StripViewportIndexLayer) not found", file=sys.stderr); sys.exit(1)
s = s.replace(call_anchor, call_anchor + "\tForceExplicitLodSamples(Spirv);\n", 1)

io.open(F, "w", encoding="utf-8").write(s)
print("patched dawn_tint_bridge.cpp: ForceExplicitLodSamples (implicit->explicit Lod 0) + call site")
