p='/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/Renderer/Private/MeshPassProcessor.cpp'
s=open(p).read()
old='''	ON_SCOPE_EXIT { if (_wndrFx) { RHICmdList.EnqueueLambda(TEXT("WndrFxDiagOff"), [](FRHICommandListBase&) { extern RHI_API int32 GWndrDiagDraw; GWndrDiagDraw = 0; }); } };
'''
assert s.count(old)==1; s=s.replace(old,'')
a='''				bDoOverrideArgs ? SceneArgs.IndirectArgsByteOffset : MeshDrawCommand.IndirectArgs.Offset
			);
		}
	}
}

bool FMeshDrawCommand::SubmitDrawIndirectBegin('''
assert s.count(a)==1
s=s.replace(a,a.replace('''	}
}

bool''','''	}
#if PLATFORM_WASM && MESH_DRAW_COMMAND_DEBUG_DATA
	if (_wndrFx) { RHICmdList.EnqueueLambda(TEXT("WndrFxDiagOff"), [](FRHICommandListBase&) { extern RHI_API int32 GWndrDiagDraw; GWndrDiagDraw = 0; }); } // wndr FXDIAG-1
#endif
}

bool'''))
open(p,'w').write(s); print("ok")
