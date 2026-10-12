#!/usr/bin/env python3
# FXDIAG-1 (diag): why are Link plasma / rocket fireball / explosion draws invisible?
# Renderer marks draws of matching materials (RHI-thread flag via EnqueueLambda);
# DawnRHI then logs, for those draws: pass name, PSO blend/depth/format, every bind
# group entry (REAL/DUMMY + texture format), vertex/instance counts.
import sys, os, shutil
UE = '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime'
RR = os.path.join(UE, 'RHI/Private/RHIResources.cpp')
MP = os.path.join(UE, 'Renderer/Private/MeshPassProcessor.cpp')
CC = os.path.join(UE, 'DawnRHI/Private/DawnCommandContext.cpp')
DR = os.path.join(UE, 'DawnRHI/Private/DawnDynamicRHI.cpp')
RH = os.path.join(UE, 'DawnRHI/Public/DawnResources.h')
MARK = 'FXDIAG-1'

def patch(path, reps):
    s = open(path).read()
    if MARK in s:
        print('already patched', path); return
    bak = path + '.wndrbak_fxdiag'
    if not os.path.exists(bak): shutil.copy2(path, bak)
    for old, new in reps:
        assert s.count(old) == 1, (path, old[:80], s.count(old))
        s = s.replace(old, new)
    open(path, 'w').write(s)
    print('patched', path)

patch(RR, [(
'RHI_API bool GWndrSuppressDeleteFlush = false;',
'RHI_API bool GWndrSuppressDeleteFlush = false;\nRHI_API int32 GWndrDiagDraw = 0; // wndr FXDIAG-1 (RHI-thread: current draw is a diagnosed FX material)')])

patch(MP, [
('''void FMeshDrawCommand::SubmitDrawEnd(const FMeshDrawCommand& MeshDrawCommand, const FMeshDrawCommandSceneArgs& SceneArgs, uint32 InstanceFactor, FRHICommandList& RHICmdList)
{
''',
'''void FMeshDrawCommand::SubmitDrawEnd(const FMeshDrawCommand& MeshDrawCommand, const FMeshDrawCommandSceneArgs& SceneArgs, uint32 InstanceFactor, FRHICommandList& RHICmdList)
{
#if PLATFORM_WASM && MESH_DRAW_COMMAND_DEBUG_DATA
	// wndr FXDIAG-1: flag draws of the invisible FX materials for the DawnRHI bind/PSO dump
	uint8 _wndrFx = 0;
	{
		static FCriticalSection _fxl; static TMap<const FMaterial*, uint8> _fxm; static uint32 _fxn = 0;
		FScopeLock _l(&_fxl);
		if (_fxn < 400)
		{
			const FMaterial* M = MeshDrawCommand.DebugData.Material;
			uint8* F = _fxm.Find(M);
			if (!F)
			{
				const FString& N = MeshDrawCommand.DebugData.MaterialRenderProxyName;
				const uint8 V = (N.Contains(TEXT("LinkProjectile")) || N.Contains(TEXT("Fireball")) || N.Contains(TEXT("explosion_base")) || N.Contains(TEXT("Linkgun_Soft"))) ? 1 : 0;
				F = &_fxm.Add(M, V);
			}
			_wndrFx = *F;
			if (_wndrFx)
			{
				++_fxn;
				printf("WNDR FXDRAW mat=%s prims=%u inst=%u idx=%d\\n", TCHAR_TO_UTF8(*MeshDrawCommand.DebugData.MaterialRenderProxyName), MeshDrawCommand.NumPrimitives, MeshDrawCommand.NumInstances, MeshDrawCommand.IndexBuffer ? 1 : 0); fflush(stdout);
			}
		}
	}
	if (_wndrFx) { RHICmdList.EnqueueLambda(TEXT("WndrFxDiagOn"), [](FRHICommandListBase&) { extern RHI_API int32 GWndrDiagDraw; GWndrDiagDraw = 1; }); }
	ON_SCOPE_EXIT { if (_wndrFx) { RHICmdList.EnqueueLambda(TEXT("WndrFxDiagOff"), [](FRHICommandListBase&) { extern RHI_API int32 GWndrDiagDraw; GWndrDiagDraw = 0; }); } };
#endif
'''),
])

patch(RH, [(
'	mutable bool bWndrLayoutBuilt = false;',
'	mutable bool bWndrLayoutBuilt = false;\n	FString WndrDesc; // wndr FXDIAG-1')])

patch(DR, [
('''	PSO->bFragmentless = bFragmentless; // FRAGLESS: draw path must keep these out of color passes
''',
'''	PSO->bFragmentless = bFragmentless; // FRAGLESS: draw path must keep these out of color passes
	PSO->WndrDesc = FString::Printf(TEXT("fmt=%d blend=%d c(op=%d src=%d dst=%d) a(op=%d src=%d dst=%d) mask=0x%x depth=%d cmp=%d dwrite=%d topo=%d"), (int)ColorTarget.format, ColorTarget.blend ? 1 : 0,
		(int)BlendState.color.operation, (int)BlendState.color.srcFactor, (int)BlendState.color.dstFactor, (int)BlendState.alpha.operation, (int)BlendState.alpha.srcFactor, (int)BlendState.alpha.dstFactor,
		(unsigned)WriteMask, bHasDepth ? 1 : 0, (int)DepthStencil.depthCompare, (int)DepthStencil.depthWriteEnabled, (int)Initializer.PrimitiveType); // wndr FXDIAG-1
'''),
('''		NewTexture->View = wgpuTextureCreateView(Texture, nullptr);
		DawnLiveHandleAdd((void*)NewTexture->View); // LIVEHANDLE
''',
'''		NewTexture->View = wgpuTextureCreateView(Texture, nullptr);
		DawnLiveHandleAdd((void*)NewTexture->View); // LIVEHANDLE
		{ extern void WndrViewInfoAdd(void* V, int32 Fmt, int32 W, int32 H, const TCHAR* Name); WndrViewInfoAdd((void*)NewTexture->View, (int32)TexDesc.format, (int32)TexDesc.size.width, (int32)TexDesc.size.height, CreateDesc.DebugName ? CreateDesc.DebugName : TEXT("?")); } // wndr FXDIAG-1
'''),
])

patch(CC, [
('''static const void* GWndrCanvasPSO = nullptr; static bool GWndrCanvasIs = false;''',
'''static const void* GWndrCanvasPSO = nullptr; static bool GWndrCanvasIs = false;
// wndr FXDIAG-1
extern RHI_API int32 GWndrDiagDraw;
static uint32 GWndrFxBudget = 0;
static FString GWndrCurPassName;
static TSet<void*> GWndrDummyViews;
static FCriticalSection GWndrViewInfoLock;
static TMap<void*, FString> GWndrViewInfo;
void WndrViewInfoAdd(void* V, int32 Fmt, int32 W, int32 H, const TCHAR* Name)
{
	FScopeLock L(&GWndrViewInfoLock);
	GWndrViewInfo.Add(V, FString::Printf(TEXT("%s fmt=%d %dx%d"), Name, Fmt, W, H));
}
static FString WndrViewDesc(void* V)
{
	if (GWndrDummyViews.Contains(V)) { return TEXT("DUMMY"); }
	FScopeLock L(&GWndrViewInfoLock);
	if (const FString* S = GWndrViewInfo.Find(V)) { return *S; }
	return TEXT("view?");
}'''),
('''	Cache.Add(Key, View);
	return View;
}
''',
'''	Cache.Add(Key, View);
	if (View) { GWndrDummyViews.Add((void*)View); } // wndr FXDIAG-1
	return View;
}
'''),
('''		if (const TArray<TPair<uint32, uint64>>* Hit = GWndrWalkCache.Find(WndrWalkKey))''',
'''		if (const TArray<TPair<uint32, uint64>>* Hit = (GWndrDiagDraw && GWndrFxBudget < 60) ? nullptr : GWndrWalkCache.Find(WndrWalkKey)) // wndr FXDIAG-1: walk diag draws'''),
('''		WGPUBindGroupDescriptor BgDesc = {};
		BgDesc.layout = CurrentPSO->BindGroupLayouts[Group];''',
'''		if (GWndrDiagDraw && GWndrFxBudget < 60) // wndr FXDIAG-1
		{
			printf("WNDR FXBG grp=%u pass=%s pso=%p %s\\n", Group, TCHAR_TO_UTF8(*GWndrCurPassName), (void*)CurrentPSO, TCHAR_TO_UTF8(*CurrentPSO->WndrDesc));
			for (const WGPUBindGroupEntry& E : Entries)
			{
				const FDawnShaderBinding* NB = nullptr; for (const FDawnShaderBinding& B : LG.Value) { if (B.Binding == E.binding) { NB = &B; break; } }
				const FString D = E.textureView ? WndrViewDesc((void*)E.textureView) : (E.buffer ? FString::Printf(TEXT("buf sz=%llu"), (unsigned long long)E.size) : (E.sampler ? FString(TEXT("sampler")) : FString(TEXT("-"))));
				printf("WNDR FXBG   %u %s st=%d : %s\\n", E.binding, NB ? TCHAR_TO_UTF8(*NB->Name) : "?", NB ? (int)NB->TexSampleType : -1, TCHAR_TO_UTF8(*D));
			}
			fflush(stdout);
		}
		WGPUBindGroupDescriptor BgDesc = {};
		BgDesc.layout = CurrentPSO->BindGroupLayouts[Group];'''),
('''	if (bAllGroupsComplete && bWndrWalkCacheable && WndrWalkGroups.Num() > 0) // wndr WALKCACHE-1''',
'''	if (GWndrDiagDraw && GWndrFxBudget < 60) { ++GWndrFxBudget; printf("WNDR FXBG end complete=%d rt=%ux%u\\n", bAllGroupsComplete ? 1 : 0, CurrentRTWidth, CurrentRTHeight); fflush(stdout); } // wndr FXDIAG-1
	if (bAllGroupsComplete && bWndrWalkCacheable && WndrWalkGroups.Num() > 0) // wndr WALKCACHE-1'''),
('''	FDawnTexture* ColorTex = static_cast<FDawnTexture*>(InInfo.ColorRenderTargets[0].RenderTarget);
	if (ColorTex) { CurrentRTWidth''',
'''	GWndrCurPassName = InName ? InName : TEXT("?"); // wndr FXDIAG-1
	FDawnTexture* ColorTex = static_cast<FDawnTexture*>(InInfo.ColorRenderTargets[0].RenderTarget);
	if (ColorTex) { CurrentRTWidth'''),
])
