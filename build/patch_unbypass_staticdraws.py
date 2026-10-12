#!/usr/bin/env python3
# wndr: un-bypass DrawStaticElements on wasm (root cause of zero base-pass mesh
# draw commands / zero material PSOs / black world) + add WNDR diagnostics.
import sys

ROOT = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/Renderer/Private/"

def patch(path, old, new, must=1):
    with open(path) as f:
        s = f.read()
    n = s.count(old)
    if n != must:
        print(f"FAIL {path}: found {n} occurrences (expected {must})")
        sys.exit(1)
    with open(path, "w") as f:
        f.write(s.replace(old, new))
    print(f"OK   {path}")

# ---- 1. PrimitiveSceneInfo.cpp: restore DrawStaticElements on wasm ----------
old1 = """#if !PLATFORM_WASM
			// The WEBGPU cook currently emits no material shader maps, so caching the
			// static mesh elements dereferences an incomplete material and crashes
			// (GetIncompleteMaterialWithFallback). Skip it: StaticMeshes stays empty so
			// no draw commands are cached and the 3D scene simply isn't drawn, while the
			// Slate front-end (global shaders) still renders. Remove once materials cook.
			SceneInfo->Proxy->DrawStaticElements(&BatchingSPDI);
#endif"""
new1 = """#if PLATFORM_WASM
			// wndr: material shaders now cross-compile + cook (ES3_1 mobile forward), so
			// static mesh elements are cached again. This call is REQUIRED for any world
			// geometry to render: the entire static scene flows through cached mesh draw
			// commands built from these elements (skipping it -> StaticMeshes empty ->
			// zero base-pass draw commands -> zero material PSOs -> black world).
			// Escape hatch: wndr.SkipStaticDraws=1 restores the old skip.
			static const auto* CVarWndrSkipStatic = IConsoleManager::Get().FindConsoleVariable(TEXT("wndr.SkipStaticDraws"));
			if (!(CVarWndrSkipStatic && CVarWndrSkipStatic->GetInt() != 0))
			{
				SceneInfo->Proxy->DrawStaticElements(&BatchingSPDI);
			}
#else
			SceneInfo->Proxy->DrawStaticElements(&BatchingSPDI);
#endif"""
patch(ROOT + "PrimitiveSceneInfo.cpp", old1, new1)

# ---- 2. MobileShadingRenderer.cpp: log base-pass setup inputs per view ------
old2 = """		if (ViewCommands.MeshCommands[EMeshPass::BasePass].IsEmpty() && ViewCommands.MeshCommands[EMeshPass::MobileBasePassCSM].IsEmpty() && View.NumVisibleDynamicMeshElements[EMeshPass::BasePass] == 0 && ViewCommands.NumDynamicMeshCommandBuildRequestElements[EMeshPass::BasePass] == 0)
		{
			continue;
		}"""
new2 = """#if PLATFORM_WASM
		{
			// wndr diag: how many base-pass mesh draw commands visibility produced.
			// cachedBP>0 after the DrawStaticElements un-bypass = world geometry flowing.
			static int32 WndrBpLog = 0;
			if (WndrBpLog < 60 || (WndrBpLog & 255) == 0)
			{
				UE_LOGF(LogRenderer, Log, "WNDR BPSETUP view=%d cachedBP=%d cachedCSM=%d dynMDE=%d buildReq=%d",
					ViewIndex,
					ViewCommands.MeshCommands[EMeshPass::BasePass].Num(),
					ViewCommands.MeshCommands[EMeshPass::MobileBasePassCSM].Num(),
					View.NumVisibleDynamicMeshElements[EMeshPass::BasePass],
					ViewCommands.NumDynamicMeshCommandBuildRequestElements[EMeshPass::BasePass]);
			}
			WndrBpLog++;
		}
#endif
		if (ViewCommands.MeshCommands[EMeshPass::BasePass].IsEmpty() && ViewCommands.MeshCommands[EMeshPass::MobileBasePassCSM].IsEmpty() && View.NumVisibleDynamicMeshElements[EMeshPass::BasePass] == 0 && ViewCommands.NumDynamicMeshCommandBuildRequestElements[EMeshPass::BasePass] == 0)
		{
			continue;
		}"""
patch(ROOT + "MobileShadingRenderer.cpp", old2, new2)

# ---- 3. MobileShadingRenderer.cpp: prove FMobileSceneRenderer::Render runs --
old3 = """	const ERendererOutput RendererOutput = GetRendererOutput();
	const bool bShadowOnlyTimeslice = IsShadowOnlyTimeslice_RT(&ViewFamily, Scene);
	const bool bRendererOutputFinalSceneColor = (RendererOutput != ERendererOutput::DepthPrepassOnly) && !bShadowOnlyTimeslice;

	// Allow GPU cost of scene captures to be distinguished from main views, useful for profiling the amortized GPU cost of runtime reflection captures."""
new3 = """	const ERendererOutput RendererOutput = GetRendererOutput();
	const bool bShadowOnlyTimeslice = IsShadowOnlyTimeslice_RT(&ViewFamily, Scene);
	const bool bRendererOutputFinalSceneColor = (RendererOutput != ERendererOutput::DepthPrepassOnly) && !bShadowOnlyTimeslice;

#if PLATFORM_WASM
	{
		// wndr diag: proves the mobile scene renderer executes at all (View UB was
		// never observed being created, so renderer execution was in question).
		static int32 WndrRdrLog = 0;
		if (WndrRdrLog < 60 || (WndrRdrLog & 255) == 0)
		{
			UE_LOGF(LogRenderer, Log, "WNDR MOBRDR views=%d cap=%d finalColor=%d prims=%d",
				Views.Num(), Views[0].bIsSceneCapture ? 1 : 0, bRendererOutputFinalSceneColor ? 1 : 0, Scene->Primitives.Num());
		}
		WndrRdrLog++;
	}
#endif

	// Allow GPU cost of scene captures to be distinguished from main views, useful for profiling the amortized GPU cost of runtime reflection captures."""
patch(ROOT + "MobileShadingRenderer.cpp", old3, new3)

# ---- 4. MeshDrawCommands.cpp: log actual submission ------------------------
old4 = """	QUICK_SCOPE_CYCLE_COUNTER(STAT_MeshPassDrawImmediate);

	check(TaskEventRef == nullptr);

	if (TaskContext.bUseGPUScene)"""
new4 = """	QUICK_SCOPE_CYCLE_COUNTER(STAT_MeshPassDrawImmediate);

	check(TaskEventRef == nullptr);

#if PLATFORM_WASM
	{
		// wndr diag: mesh draw commands actually reaching RHI submission.
		static int32 WndrMdcLog = 0;
		if (WndrMdcLog < 80 || (WndrMdcLog & 255) == 0)
		{
			UE_LOGF(LogRenderer, Log, "WNDR MDCDRAW pass=%d maxDraws=%d built=%d gpuScene=%d",
				(int32)TaskContext.PassType, MaxNumDraws, TaskContext.MeshDrawCommands.Num(), TaskContext.bUseGPUScene ? 1 : 0);
		}
		WndrMdcLog++;
	}
#endif

	if (TaskContext.bUseGPUScene)"""
patch(ROOT + "MeshDrawCommands.cpp", old4, new4)

print("ALL PATCHES APPLIED")
