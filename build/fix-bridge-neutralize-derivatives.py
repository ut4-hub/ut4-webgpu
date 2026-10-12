#!/usr/bin/env python3
# MATERIAL-SHADER SURVIVAL FIX part 2 (2026-08-13), issue #1 sibling.
#
# After ForceExplicitLodSamples cleared the textureSample failures, the cook
# still failed EVERY material: the ES3.1 mobile base-pass PS
# (TMobileBasePassPSFNoLightMapPolicy, compiled by every material incl. the
# trivial WorldGridMaterial) hit:
#   WgslFromIR: error: 'dpdx' must only be called from uniform control flow
# WGSL requires derivative builtins (dpdx/dpdy/fwidth) in uniform control flow
# and there is no explicit-LOD equivalent, so this blocked all material shader
# maps -> no shader library emitted -> WorldGridMaterial null GTSM ->
# MaterialShared.cpp:3167 fatal.
#
# Fix = rewrite every derivative op to OpCopyObject of its own operand
# (dpdx(x) -> x). SPIR-V guarantees the derivative's Result Type equals the type
# of its operand P, so OpCopyObject %ResultType %Result %P is type-valid and is a
# single-word opcode swap (word count stays 4). This eliminates the WGSL
# uniform-control-flow requirement. The menu needs no real derivatives -- all
# texture samples are already forced to explicit LOD 0 by
# fix-bridge-force-explicit-lod-samples.py, so derivative-based mip selection is
# already moot; derivative use in AA/analytic math degrades cosmetically only.
#
# Runs right after ForceExplicitLodSamples. Rebuild with
# engine-patches/build_dawn_tint_bridge_so.sh, bump UESHADERMETADATA_VERSION, recook.
import io, sys

F = "/mnt/vms/ss-build/ut4-webgpu-push/tools/dawn_tint_bridge.cpp"
s = io.open(F, encoding="utf-8").read()

if "NeutralizeDerivatives" in s:
    print("already patched"); sys.exit(0)

FUNC = r'''	// WGSL requires derivative builtins (dpdx/dpdy/fwidth, SPIR-V opcodes
	// 207..215) in uniform control flow, with no explicit-LOD escape hatch. UE's
	// ES3.1 mobile base-pass PS uses them in non-uniform flow, which failed every
	// material's shader map (WorldGridMaterial included) and stopped the shader
	// library from being emitted. Rewrite each derivative to OpCopyObject of its
	// operand (dpdx(x) -> x): the derivative's Result Type equals the operand
	// type, so this is a type-valid single-word opcode swap. The menu needs no
	// real derivatives -- texture samples are already forced to LOD 0.
	void NeutralizeDerivatives(std::vector<uint32_t>& Spirv)
	{
		if (Spirv.size() < 5) return;
		const uint32_t OpCopyObject = 83;
		size_t i = 5;
		while (i < Spirv.size()) {
			const uint32_t w0 = Spirv[i], op = w0 & 0xFFFFu, wc = w0 >> 16;
			if (wc == 0 || i + wc > Spirv.size()) break;
			if (op >= 207u && op <= 215u && wc == 4) {
				Spirv[i] = (4u << 16) | OpCopyObject;
			}
			i += wc;
		}
	}

'''

anchor = "static FDawnTintCookResult DawnLegalizeAndCookImpl("
if anchor not in s:
    print("impl anchor not found", file=sys.stderr); sys.exit(1)
s = s.replace(anchor, FUNC + anchor, 1)

call_anchor = "\tForceExplicitLodSamples(Spirv);\n"
if call_anchor not in s:
    print("call anchor (ForceExplicitLodSamples) not found -- apply the textureSample patch first", file=sys.stderr); sys.exit(1)
s = s.replace(call_anchor, call_anchor + "\tNeutralizeDerivatives(Spirv);\n", 1)

io.open(F, "w", encoding="utf-8").write(s)
print("patched dawn_tint_bridge.cpp: NeutralizeDerivatives (dpdx/dpdy/fwidth -> identity) + call site")
