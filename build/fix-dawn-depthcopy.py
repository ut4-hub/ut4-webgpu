#!/usr/bin/env python3
# DEPTHCOPY-1: translucent FX (explosions, Link plasma, rocket fireballs) sample
# MobileBasePass_SceneTextures_SceneDepthTexture for depth fade. That slot was
# dummy-filled (grey 0.5 = "surface right in front of the camera" in reversed-Z)
# -> depth fade made them fully transparent. WebGPU can't sample the depth target
# on a filterable float slot (and can't sample it while it's attached), so:
#   * before any color pass that LOADS depth (DecalsAndTranslucency), blit the
#     depth buffer into an R32F (R16F without float32-filterable) copy,
#   * bind that copy to every 2D float "*SceneDepthTexture" slot.
# Depth-stencil textures also get TextureBinding usage so they can be blitted.
# Also: FXDIAG-1 budget becomes once-per-PSO (Link projectile never printed).
import os, shutil
P = '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private/'
CC = P + 'DawnCommandContext.cpp'
DR = P + 'DawnDynamicRHI.cpp'

def patch(path, reps):
    s = open(path).read()
    if 'DEPTHCOPY-1' in s:
        print('already patched', path); return
    bak = path + '.wndrbak_depthcopy'
    if not os.path.exists(bak): shutil.copy2(path, bak)
    for o, n in reps:
        assert s.count(o) == 1, (path, o[:70], s.count(o))
        s = s.replace(o, n)
    open(path, 'w').write(s)
    print('patched', path)

patch(DR, [(
'''	if (bIsDepthStencil)
	{
		Usage |= WGPUTextureUsage_RenderAttachment;
	}''',
'''	if (bIsDepthStencil)
	{
		Usage |= WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding; // wndr DEPTHCOPY-1: blit source
	}''')])

patch(CC, [
('''static int32 GWndrDbgBits = 0; // wndr LMTOGGLE-1''',
'''// wndr DEPTHCOPY-1: sampleable float copy of the scene depth for translucency depth fade.
extern void DawnLiveHandleRemove(void* H);
static WGPUTexture GWndrDCTex = nullptr;
static WGPUTextureView GWndrDCView = nullptr;
static uint32 GWndrDCW = 0, GWndrDCH = 0;
static WGPURenderPipeline GWndrDCPipe = nullptr;
static WGPUBindGroupLayout GWndrDCBGL = nullptr;
static WGPUTextureFormat GWndrDCFmt = WGPUTextureFormat_R16Float;
static void WndrCopySceneDepth(WGPUDevice Dev, WGPUCommandEncoder Enc, FDawnTexture* DepthTex)
{
	if (!Dev || !Enc || !DepthTex || !DepthTex->Texture || !DepthTex->View) { return; }
	if (DepthTex->GetDesc().NumSamples > 1) { return; }
	if (!(wgpuTextureGetUsage(DepthTex->Texture) & WGPUTextureUsage_TextureBinding)) { return; }
	const uint32 W = (uint32)DepthTex->GetDesc().Extent.X, H = (uint32)DepthTex->GetDesc().Extent.Y;
	if (!W || !H) { return; }
	if (!GWndrDCPipe)
	{
		GWndrDCFmt = GDawnFloat32Filterable ? WGPUTextureFormat_R32Float : WGPUTextureFormat_R16Float;
		static const char* Src =
			"@group(0) @binding(0) var d : texture_depth_2d;\\n"
			"@vertex fn vs(@builtin(vertex_index) i : u32) -> @builtin(position) vec4f {\\n"
			"  var p = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));\\n"
			"  return vec4f(p[i], 0.0, 1.0);\\n"
			"}\\n"
			"@fragment fn fs(@builtin(position) pos : vec4f) -> @location(0) vec4f {\\n"
			"  return vec4f(textureLoad(d, vec2i(pos.xy), 0), 0.0, 0.0, 1.0);\\n"
			"}\\n";
		WGPUShaderSourceWGSL S = {}; S.chain.sType = WGPUSType_ShaderSourceWGSL; S.code = { Src, WGPU_STRLEN };
		WGPUShaderModuleDescriptor MD = {}; MD.nextInChain = &S.chain;
		WGPUShaderModule M = wgpuDeviceCreateShaderModule(Dev, &MD);
		WGPUBindGroupLayoutEntry LE = {}; LE.binding = 0; LE.visibility = WGPUShaderStage_Fragment;
		LE.texture.sampleType = WGPUTextureSampleType_Depth; LE.texture.viewDimension = WGPUTextureViewDimension_2D;
		WGPUBindGroupLayoutDescriptor BLD = {}; BLD.entryCount = 1; BLD.entries = &LE;
		GWndrDCBGL = wgpuDeviceCreateBindGroupLayout(Dev, &BLD);
		WGPUPipelineLayoutDescriptor PLD = {}; PLD.bindGroupLayoutCount = 1; PLD.bindGroupLayouts = &GWndrDCBGL;
		WGPUPipelineLayout PL = wgpuDeviceCreatePipelineLayout(Dev, &PLD);
		WGPUColorTargetState CT = {}; CT.format = GWndrDCFmt; CT.writeMask = WGPUColorWriteMask_All;
		WGPUFragmentState FS = {}; FS.module = M; FS.entryPoint = { "fs", WGPU_STRLEN }; FS.targetCount = 1; FS.targets = &CT;
		WGPURenderPipelineDescriptor PD = {}; PD.layout = PL; PD.vertex.module = M; PD.vertex.entryPoint = { "vs", WGPU_STRLEN };
		PD.primitive.topology = WGPUPrimitiveTopology_TriangleList; PD.multisample.count = 1; PD.multisample.mask = 0xFFFFFFFFu; PD.fragment = &FS;
		GWndrDCPipe = wgpuDeviceCreateRenderPipeline(Dev, &PD);
		printf("WNDR DEPTHCOPY pipeline=%p fmt=%d\\n", (void*)GWndrDCPipe, (int)GWndrDCFmt); fflush(stdout);
	}
	if (!GWndrDCPipe) { return; }
	if (!GWndrDCTex || W != GWndrDCW || H != GWndrDCH)
	{
		if (GWndrDCView) { DawnLiveHandleRemove((void*)GWndrDCView); wgpuTextureViewRelease(GWndrDCView); GWndrDCView = nullptr; }
		if (GWndrDCTex) { wgpuTextureDestroy(GWndrDCTex); wgpuTextureRelease(GWndrDCTex); GWndrDCTex = nullptr; }
		WGPUTextureDescriptor TD = {}; TD.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
		TD.dimension = WGPUTextureDimension_2D; TD.size = { W, H, 1 }; TD.format = GWndrDCFmt; TD.mipLevelCount = 1; TD.sampleCount = 1;
		GWndrDCTex = wgpuDeviceCreateTexture(Dev, &TD);
		GWndrDCView = GWndrDCTex ? wgpuTextureCreateView(GWndrDCTex, nullptr) : nullptr;
		if (GWndrDCView) { DawnLiveHandleAdd((void*)GWndrDCView); }
		GWndrDCW = W; GWndrDCH = H;
		printf("WNDR DEPTHCOPY target %ux%u view=%p\\n", W, H, (void*)GWndrDCView); fflush(stdout);
	}
	if (!GWndrDCView) { return; }
	WGPUBindGroupEntry BE = {}; BE.binding = 0; BE.textureView = DepthTex->View;
	WGPUBindGroupDescriptor BD = {}; BD.layout = GWndrDCBGL; BD.entryCount = 1; BD.entries = &BE;
	WGPUBindGroup BG = wgpuDeviceCreateBindGroup(Dev, &BD);
	WGPURenderPassColorAttachment CA = {}; CA.view = GWndrDCView; CA.loadOp = WGPULoadOp_Clear; CA.storeOp = WGPUStoreOp_Store;
	CA.clearValue = { 0.0, 0.0, 0.0, 1.0 }; CA.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
	WGPURenderPassDescriptor RD = {}; RD.colorAttachmentCount = 1; RD.colorAttachments = &CA;
	WGPURenderPassEncoder RP = wgpuCommandEncoderBeginRenderPass(Enc, &RD);
	wgpuRenderPassEncoderSetPipeline(RP, GWndrDCPipe);
	wgpuRenderPassEncoderSetBindGroup(RP, 0, BG, 0, nullptr);
	wgpuRenderPassEncoderDraw(RP, 3, 1, 0, 0);
	wgpuRenderPassEncoderEnd(RP);
	wgpuRenderPassEncoderRelease(RP);
	wgpuBindGroupRelease(BG);
}
static int32 GWndrDbgBits = 0; // wndr LMTOGGLE-1'''),
('''		if (GWndrDbgBits) // wndr LMTOGGLE-1
		{''',
'''		if (GWndrDCView) // wndr DEPTHCOPY-1: scene depth for translucency depth fade
		{
			for (WGPUBindGroupEntry& E : Entries)
			{
				if (!E.textureView) { continue; }
				const FDawnShaderBinding* NB = nullptr; for (const FDawnShaderBinding& B : LG.Value) { if (B.Binding == E.binding) { NB = &B; break; } }
				if (NB && NB->Kind == EDawnShaderBindingKind::Texture && NB->TexViewDim == WGPUTextureViewDimension_2D
					&& (NB->TexSampleType == WGPUTextureSampleType_Float || NB->TexSampleType == WGPUTextureSampleType_UnfilterableFloat)
					&& NB->Name.EndsWith(TEXT("SceneDepthTexture"), ESearchCase::CaseSensitive))
				{
					E.textureView = GWndrDCView;
				}
			}
		}
		if (GWndrDbgBits) // wndr LMTOGGLE-1
		{'''),
('''	ActiveRenderPass = wgpuCommandEncoderBeginRenderPass(CommandEncoder, &RpDesc);''',
'''	if (bWndrHasColor && DepthTex && RpDesc.depthStencilAttachment && DepthAttach.depthLoadOp == WGPULoadOp_Load) // wndr DEPTHCOPY-1
	{
		WndrCopySceneDepth(Owner->GetDevice(), CommandEncoder, DepthTex);
	}
	ActiveRenderPass = wgpuCommandEncoderBeginRenderPass(CommandEncoder, &RpDesc);'''),
('''K.Add((uint64)GWndrDbgBits); // LMTOGGLE-1''', '''K.Add((uint64)GWndrDbgBits); K.Add((uint64)GWndrDCView); // LMTOGGLE-1 + DEPTHCOPY-1'''),
# FXDIAG budget: once per PSO
('''		if (const TArray<TPair<uint32, uint64>>* Hit = (GWndrDiagDraw && GWndrFxBudget < 60) ? nullptr : GWndrWalkCache.Find(WndrWalkKey)) // wndr FXDIAG-1: walk diag draws''',
'''		static TSet<const void*> _fxSeen; // wndr FXDIAG-1: dump each FX PSO once
		const bool _fxDump = GWndrDiagDraw && GWndrFxBudget < 60 && !_fxSeen.Contains((const void*)CurrentPSO);
		if (_fxDump) { _fxSeen.Add((const void*)CurrentPSO); }
		GWndrFxNow = _fxDump;
		if (const TArray<TPair<uint32, uint64>>* Hit = _fxDump ? nullptr : GWndrWalkCache.Find(WndrWalkKey)) // wndr FXDIAG-1: walk diag draws'''),
('''		if (GWndrDiagDraw && GWndrFxBudget < 60) // wndr FXDIAG-1
		{''',
'''		if (GWndrFxNow) // wndr FXDIAG-1
		{'''),
('''	if (GWndrDiagDraw && GWndrFxBudget < 60) { ++GWndrFxBudget;''',
'''	if (GWndrFxNow) { GWndrFxNow = false; ++GWndrFxBudget;'''),
('''static uint32 GWndrFxBudget = 0;''',
'''static uint32 GWndrFxBudget = 0;
static bool GWndrFxNow = false;'''),
])
