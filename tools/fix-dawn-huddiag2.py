#!/usr/bin/env python3
# HUDDIAG-2: canvas (HUD) draws reach the backbuffer (37/frame) but nothing shows.
# Dump each canvas PSO (PS binds "InTexture"): vertex layout vs VS inputs, blend,
# write mask, depth, target format, VS input + PS output WGSL lines; and the
# viewport/scissor in force for the first canvas draws.
import sys, shutil, os
ROOT = sys.argv[1] if len(sys.argv) > 1 else '/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private'
DR = os.path.join(ROOT, 'DawnDynamicRHI.cpp')
CC = os.path.join(ROOT, 'DawnCommandContext.cpp')

def patch(path, mark, reps, bak_suffix):
    s = open(path).read()
    if mark in s:
        print('already patched', path); return
    bak = path + bak_suffix
    if not os.path.exists(bak): shutil.copy2(path, bak)
    for old, new in reps:
        assert s.count(old) == 1, (path, old[:80], s.count(old))
        s = s.replace(old, new)
    open(path, 'w').write(s)
    print('patched', path)

patch(DR, 'HUDDIAG-2', [(
'''	WGPURenderPipelineDescriptor PipeDesc = {};
	PipeDesc.layout = PipelineLayout;''',
'''	{ // wndr HUDDIAG-2
		bool bCanvas = false;
		if (PS) { for (const FDawnShaderBinding& Bd : PS->Bindings) { if (Bd.Name == TEXT("InTexture")) { bCanvas = true; break; } } }
		static uint32 _hc = 0;
		if (bCanvas && _hc++ < 6)
		{
			printf("WNDR HUDPSO #%u fmt=%d blend c(op=%d src=%d dst=%d) a(op=%d src=%d dst=%d) mask=0x%x depth=%d cmp=%d dwrite=%d slots=%u\\n", _hc, (int)ColorTarget.format,
				(int)BlendState.color.operation, (int)BlendState.color.srcFactor, (int)BlendState.color.dstFactor, (int)BlendState.alpha.operation, (int)BlendState.alpha.srcFactor, (int)BlendState.alpha.dstFactor,
				(unsigned)WriteMask, bHasDepth ? 1 : 0, (int)DepthStencil.depthCompare, (int)DepthStencil.depthWriteEnabled, NumStreamSlots);
			for (uint32 S = 0; S < NumStreamSlots; ++S)
			{
				printf("WNDR HUDPSO #%u  stream%u stride=%llu nAttr=%d\\n", _hc, S, (unsigned long long)StreamStrides[S], StreamAttributes[S].Num());
				for (const WGPUVertexAttribute& A : StreamAttributes[S]) { printf("WNDR HUDPSO #%u    loc=%u off=%llu fmt=%d\\n", _hc, (uint32)A.shaderLocation, (unsigned long long)A.offset, (int)A.format); }
			}
			TMap<uint32, WGPUVertexFormat> _hi; ParseWgslVertexInputs(VS->WGSLSource, _hi);
			for (const TPair<uint32, WGPUVertexFormat>& It : _hi) { printf("WNDR HUDPSO #%u  VS-WANTS loc=%u fmt=%d\\n", _hc, It.Key, (int)It.Value); }
			auto DumpLines = [&](const FString& Src, const TCHAR* Tag, const TCHAR* K1, const TCHAR* K2)
			{
				int32 St = 0, Nl = 0;
				while (St < Src.Len() && Nl < 14)
				{
					int32 En = Src.Find(TEXT("\\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, St); if (En < 0) { En = Src.Len(); }
					const FString Ln = Src.Mid(St, En - St);
					if (Ln.Contains(K1) || Ln.Contains(K2)) { printf("WNDR HUDPSO #%u %s %s\\n", _hc, TCHAR_TO_UTF8(Tag), TCHAR_TO_UTF8(*Ln.TrimStartAndEnd().Left(220))); ++Nl; }
					St = En + 1;
				}
			};
			DumpLines(VS->WGSLSource, TEXT("VS"), TEXT("fn Main("), TEXT("@location"));
			if (PS) { DumpLines(PS->WGSLSource, TEXT("PS"), TEXT("fn Main("), TEXT("return ")); }
			fflush(stdout);
		}
	}
	WGPURenderPipelineDescriptor PipeDesc = {};
	PipeDesc.layout = PipelineLayout;''')], '.wndrbak_huddiag2')

patch(CC, 'HUDDIAG-2', [
('''void FDawnCommandContext::RHISetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ)
{
''',
'''static float GWndrVP[6] = {}; static uint32 GWndrSc[5] = {}; // wndr HUDDIAG-2
void FDawnCommandContext::RHISetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ)
{
	GWndrVP[0]=MinX; GWndrVP[1]=MinY; GWndrVP[2]=MinZ; GWndrVP[3]=MaxX; GWndrVP[4]=MaxY; GWndrVP[5]=MaxZ;
'''),
('''void FDawnCommandContext::RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
''',
'''void FDawnCommandContext::RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
	GWndrSc[0]=bEnable?1:0; GWndrSc[1]=MinX; GWndrSc[2]=MinY; GWndrSc[3]=MaxX; GWndrSc[4]=MaxY;
'''),
], '.wndrbak_huddiag2')

s = open(CC).read()
old = '''	if (GWndrCanvasIs) { if (bCurRTPresentable && bCurPassHasColor) { ++GDawnFrameCanvasBB; } else { ++GDawnFrameCanvasInt; } }
'''
new = '''	if (GWndrCanvasIs) { if (bCurRTPresentable && bCurPassHasColor) { ++GDawnFrameCanvasBB; } else { ++GDawnFrameCanvasInt; }
		static uint32 _hv = 0; if (_hv++ < 24) { printf("WNDR HUDDRAW #%u pso=%p rt=%ux%u vp=[%.0f,%.0f %.0f..%.0f z%.2f..%.2f] sc=[%u %u,%u %u,%u]\\n", _hv, (void*)CurrentPSO, CurrentRTWidth, CurrentRTHeight,
			GWndrVP[0], GWndrVP[1], GWndrVP[3], GWndrVP[4], GWndrVP[2], GWndrVP[5], GWndrSc[0], GWndrSc[1], GWndrSc[2], GWndrSc[3], GWndrSc[4]); fflush(stdout); } } // wndr HUDDIAG-2
'''
if 'HUDDRAW' not in s:
    assert s.count(old) == 2, s.count(old)
    s = s.replace(old, new)
    open(CC, 'w').write(s)
    print('patched draw sites')
