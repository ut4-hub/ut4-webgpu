#!/usr/bin/env python3
# wndr LM-fix-1: UBREBIND name match also accepts "<derived>Buffer".
# FPrecomputedLightingUniformParameters derives to "PrecomputedLighting" but its WGSL
# var is "PrecomputedLightingBuffer" (IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT name) ->
# every per-draw lightmap UB was UBDROPped -> 100% dummy (LMCOUNT PLB real=0).
# Plus LMMISS diag: why LightmapResourceCluster_* textures fall to dummy.
import sys, shutil
P = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/DawnRHI/Private/DawnCommandContext.cpp"
s = open(P).read()
if "LM-fix-1" in s: print("already applied"); sys.exit(0)
shutil.copy(P, P + ".wndrbak_lmfix1")
old = "if (B.Kind != EDawnShaderBindingKind::UniformBuffer || !B.Name.Equals(UBN, ESearchCase::CaseSensitive)) { continue; }"
new = ("if (B.Kind != EDawnShaderBindingKind::UniformBuffer || !(B.Name.Equals(UBN, ESearchCase::CaseSensitive) "
       "|| B.Name.Equals(UBN + TEXT(\"Buffer\"), ESearchCase::CaseSensitive))) { continue; } // wndr LM-fix-1: PrecomputedLightingBuffer")
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new, 1)
old2 = """				case EDawnShaderBindingKind::Texture:
					E.textureView = GetDawnDummyTextureView(Owner->GetDevice(), B.TexViewDim, B.TexSampleType, Owner->GetQueue());"""
new2 = """				case EDawnShaderBindingKind::Texture:
#if PLATFORM_WASM
					// wndr LMMISS diag: why does a LightmapResourceCluster texture fall to dummy?
					if (B.Name.Contains(TEXT("LightmapResourceCluster")))
					{
						static uint32 _lmm = 0;
						if (_lmm++ < 12)
						{
							printf("WNDR LMMISS bind=%u name=%s wantDim=%d pendingUBs=%d:", B.Binding, TCHAR_TO_UTF8(*B.Name), (int)B.TexViewDim, PendingUBRefs.Num());
							for (const TRefCountPtr<FRHIUniformBuffer>& R : PendingUBRefs)
							{
								FDawnUniformBuffer* D = static_cast<FDawnUniformBuffer*>(R.GetReference());
								if (!D) { continue; }
								printf(" [%s sn=%s tbl=%d", TCHAR_TO_UTF8(*D->GetLayout().GetDebugName()), TCHAR_TO_UTF8(*D->ShaderName), D->ResTable.Num());
								if (D->ShaderName.Contains(TEXT("Lightmap")))
								{
									for (const FDawnUniformBuffer::FTableEntry& TE : D->ResTable)
									{
										int Dim = -1;
										if (TE.Resource && TE.MemberType == (uint8)UBMT_TEXTURE) { Dim = (int)static_cast<FDawnTexture*>((FRHITexture*)TE.Resource.GetReference())->GetDesc().Dimension; }
										printf(" {%s t=%d res=%d dim=%d}", TCHAR_TO_UTF8(*TE.WgslName), (int)TE.MemberType, TE.Resource ? 1 : 0, Dim);
									}
								}
								printf("]");
							}
							printf("\\n"); fflush(stdout);
						}
					}
#endif
					E.textureView = GetDawnDummyTextureView(Owner->GetDevice(), B.TexViewDim, B.TexSampleType, Owner->GetQueue());"""
assert s.count(old2) == 1, s.count(old2)
s = s.replace(old2, new2, 1)
open(P, "w").write(s); print("applied LM-fix-1 + LMMISS")
