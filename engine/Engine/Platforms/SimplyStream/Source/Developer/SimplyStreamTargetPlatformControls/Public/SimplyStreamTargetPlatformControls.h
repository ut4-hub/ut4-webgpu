// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SimplyStreamTargetPlatformControls.h: Declares the FSimplyStreamTargetPlatformCotrols class.
=============================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Delegates/IDelegateInstance.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"
#include "Containers/Ticker.h"
#include "Misc/ScopeLock.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Common/TargetPlatformControlsBase.h"
#include "../../../Runtime/Core/Public/SimplyStreamPlatformProperties.h"
#include "Interfaces/ITargetPlatform.h"
#include "Common/TargetPlatformBase.h"
#include "SimplyStreamTargetDevice.h"

#if WITH_ENGINE
#include "StaticMeshResources.h"
#include "Sound/SoundWave.h"		// USoundWave::IsStreaming() in GetWaveFormat (RADA vs OGG select)
#endif // WITH_ENGINE
#include <DataDrivenShaderPlatformInfo.h>

#define LOCTEXT_NAMESPACE "FSimplyStreamTargetPlatform"
class UTextureLODSettings;

namespace SimplyStreamTexFormat
{
	// Compressed Texture Formats
	const static FName NameDXT1(TEXT("DXT1"));
	const static FName NameDXT5(TEXT("DXT5"));
	const static FName NameDXT5n(TEXT("DXT5n"));
	const static FName NameAutoDXT(TEXT("AutoDXT"));
	const static FName NameBC4(TEXT("BC4"));
	const static FName NameBC5(TEXT("BC5"));
	const static FName NameBC6H(TEXT("BC6H"));
	const static FName NameBC7(TEXT("BC7"));

	const static FName NameETC2_RGB(TEXT("ETC2_RGB"));
	const static FName NameETC2_RGBA(TEXT("ETC2_RGBA"));
	const static FName NameETC2_R11(TEXT("ETC2_R11"));
	const static FName NameETC2_RG11(TEXT("ETC2_RG11"));
	const static FName NameAutoETC2(TEXT("AutoETC2"));

	const static FName NameAutoASTC(TEXT("ASTC_RGBAuto"));
	const static FName NameASTC_NormalRG(TEXT("ASTC_NormalRG"));
	// L+A mode suppoprted by ARM ASTC encoder
	const static FName NameASTC_NormalLA(TEXT("ASTC_NormalLA"));

	// Uncompressed Texture Formats
	const static FName NameBGRA8(TEXT("BGRA8"));
	const static FName NameG8(TEXT("G8"));
	const static FName NameRGBA16F(TEXT("RGBA16F"));
	const static FName NameR16F(TEXT("R16F"));
	const static FName NameG16(TEXT("G16"));

	//A1RGB555 is mapped to RGB555A1, because OpenGL GL_RGB5_A1 only supports alpha on the lowest bit
	const static FName NameA1RGB555(TEXT("A1RGB555"));
	const static FName NameRGB555A1(TEXT("RGB555A1"));

	const static FName GenericRemap[][2] =
	{
		{ NameA1RGB555,		NameRGB555A1			},
		{ NameG16,			NameR16F				}, // GLES does not support R16Unorm, fallback all SimplyStream to R16F
	};

	static const FName NameASTC_RGB_HDR(TEXT("ASTC_RGB_HDR"));

	const static FName ASTCRemap[][2] =
	{
		// Default format:		ASTC format:
		{ NameDXT1,			FName(TEXT("ASTC_RGB"))		},
		{ NameDXT5,			FName(TEXT("ASTC_RGBA"))	},
		{ NameDXT5n,		FName(TEXT("ASTC_NormalAG"))},
		{ NameBC5,			NameASTC_NormalRG			},
		{ NameBC4,			NameETC2_R11				},
		{ NameBC6H,			NameASTC_RGB_HDR			},
		{ NameBC7,			FName(TEXT("ASTC_RGBA_HQ"))	},
		{ NameAutoDXT,		NameAutoASTC				},
	};

	const static FName ETCRemap[][2] =
	{
		// Default format:	ETC2 format:
		{ NameDXT1,			NameETC2_RGB	},
		{ NameDXT5,			NameETC2_RGBA	},
		{ NameDXT5n,		NameETC2_RGB	},
		{ NameBC5,			NameETC2_RG11	},
		{ NameBC4,			NameETC2_R11	},
		{ NameBC6H,			NameRGBA16F		},
		{ NameBC7,			NameETC2_RGBA	},
		{ NameAutoDXT,		NameAutoETC2	},
	};
}

/**
 * Implements the SimplyStream target platform controls.
 */
template<typename TProperties>
class FSimplyStreamTargetPlatformControls : public TTargetPlatformControlsBase<TProperties>
{
public:
	typedef TTargetPlatformControlsBase<TProperties> TSuper;

	FSimplyStreamTargetPlatformControls(bool bIsClient, ITargetPlatformSettings* TargetPlatformSettings, const TCHAR* CookFlavor, const TCHAR* OverrideIniPlatformName = nullptr)
		: TSuper(new PlatformInfo::FTargetPlatformInfo(
			OverrideIniPlatformName ? FString(OverrideIniPlatformName) : FString(TProperties::IniPlatformName()),
			TProperties::IsServerOnly() ? EBuildTargetType::Server :
			bIsClient ? EBuildTargetType::Client :
			EBuildTargetType::Game, CookFlavor), TargetPlatformSettings)
		, bIsClientOnly(bIsClient) {
	}

	bool bIsClientOnly;

	//virtual ~FSimplyStreamTargetPlatformControls();

public:

	virtual FString PlatformName() const override
	{
		// instead of TPlatformProperties (which won't have Client for non-desktop platforms), use the Info's name, which is programmatically made
		return this->PlatformInfo->Name.ToString();
	}

	virtual FString CookingDeviceProfileName() const override
	{
		// when cooking for non-desktop platforms, always use the base platform name as the DP to cook with (there aren't IOSClient DPs, there are IOS (base), IPhone7, iPad4, etc DPs)
		return  this->PlatformInfo->IniPlatformName.ToString();
	}

	virtual bool HasEditorOnlyData() const override
	{
		return false;
	}

	virtual void GetExtraPackagesToCook(TArray<FName>& PackageNames) const override
	{
		// Keep the synchronous browser BootSet deterministic without force-cooking whole engine directories.
		// This list is mirrored by BOOTSET_ENGINE_PACKAGES in simplystream-project.mjs, which fails staging if
		// either side drifts. Directory-wide -COOKDIR previously added thousands of unused Substrate assets.
		static const TCHAR* const BootPackages[] = {
			TEXT("/Engine/EngineDamageTypes/DmgTypeBP_Environmental"),
			TEXT("/Engine/EngineDebugMaterials/DebugMeshMaterial"),
			TEXT("/Engine/EngineDebugMaterials/M_VolumeRenderSphereTracePP"),
			TEXT("/Engine/EngineDebugMaterials/VolumeToRender"),
			TEXT("/Engine/EngineFonts/Roboto"),
			TEXT("/Engine/EngineFonts/RobotoDistanceField"),
			TEXT("/Engine/EngineFonts/RobotoTiny"),
			TEXT("/Engine/EngineMaterials/DefaultBokeh"),
			TEXT("/Engine/EngineMaterials/DefaultDeferredDecalMaterial"),
			TEXT("/Engine/EngineMaterials/DefaultDestructiblePhysicalMaterial"),
			TEXT("/Engine/EngineMaterials/DefaultLightFunctionMaterial"),
			TEXT("/Engine/EngineMaterials/DefaultMaterial"),
			TEXT("/Engine/EngineMaterials/DefaultPhysicalMaterial"),
			TEXT("/Engine/EngineMaterials/DefaultPostProcessMaterial"),
			TEXT("/Engine/EngineMaterials/DefaultTextMaterialOpaque"),
			TEXT("/Engine/EngineMaterials/DefaultTextMaterialTranslucent"),
			TEXT("/Engine/EngineMaterials/DefaultWhiteGrid"),
			TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial"),
			TEXT("/Engine/EngineMaterials/GizmoMaterial"),
			TEXT("/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq"),
			TEXT("/Engine/EngineMaterials/LTC/GGX_LTCAmp"),
			TEXT("/Engine/EngineMaterials/LTC/GGX_LTCMat"),
			TEXT("/Engine/EngineMaterials/LTC/Sheen_LTC"),
			TEXT("/Engine/EngineMaterials/M_InvalidLightmapSettings"),
			TEXT("/Engine/EngineMaterials/MiniFont"),
			TEXT("/Engine/EngineMaterials/NaniteHiddenSectionMaterial"),
			TEXT("/Engine/EngineMaterials/PhAT_JointLimitMaterial"),
			TEXT("/Engine/EngineMaterials/PreintegratedSkinBRDF"),
			TEXT("/Engine/EngineMaterials/RemoveSurfaceMaterial"),
			TEXT("/Engine/EngineMaterials/WeightMapArrayPlaceholderTexture"),
			TEXT("/Engine/EngineMaterials/WeightMapPlaceholderTexture"),
			TEXT("/Engine/EngineMaterials/Widget3DPassThrough"),
			TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Masked"),
			TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Masked_OneSided"),
			TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque"),
			TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque_OneSided"),
			TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent"),
			TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided"),
			TEXT("/Engine/EngineMaterials/WorldGridMaterial"),
			TEXT("/Engine/EngineMeshes/Sphere"),
			TEXT("/Engine/EngineResources/Black"),
			TEXT("/Engine/EngineResources/DefaultTexture"),
			TEXT("/Engine/EngineResources/GradientTexture0"),
			TEXT("/Engine/EngineResources/WhiteSquareTexture"),
			TEXT("/Engine/EngineSounds/Submixes/MasterReverbSubmixDefault"),
		};
		for (const TCHAR* PackageName : BootPackages)
		{
			PackageNames.AddUnique(FName(PackageName));
		}
	}

	// wndr 5.8: IsRunningPlatform() moved from ITargetPlatform to ITargetPlatformSettings (5.8 split into
	// ITargetPlatformSettings + ITargetPlatformControls). It is no longer part of the Controls interface this
	// class implements, so overriding it here fails ("does not override"). The Settings base already returns
	// false for non-editor platforms, so just drop the override.


	/**
	 * Gets the name of the SimplyStream platform variant, i.e. ASTC, ETC2, DXT, etc.
	 *
	 * @param Variant name.
	 */
	FString GetSimplyStreamVariantName() const
	{
		return this->PlatformInfo->PlatformFlavor.ToString();
	}

public:

	//~ Begin ITargetPlatform Interface

	virtual float GetVariantPriority() const override
	{
		return 0.f;
	}
	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override
	{
		OutDevices.Reset();

		for (auto Iter = Devices.CreateConstIterator(); Iter; ++Iter)
		{
			OutDevices.Add(Iter.Value());
		}
	}

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& PakchunkMap, const TSet<int32>& PakchunkIndicesInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice( ) const override
	{
		// return the first device in the list
		if (Devices.Num() > 0)
		{
			auto Iter = Devices.CreateConstIterator();
			if (Iter)
			{
				return Iter.Value();
			}
		}

		return nullptr;
	}

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId ) override
	{
		if (DeviceId.GetPlatformName() == PlatformName())
		{
			return Devices.FindRef(DeviceId.GetDeviceName());
		}

		return nullptr;
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override
	{
		OutDocumentationPath = FString("Shared/Tutorials/SettingUpSimplyStreamTutorial");
		return true;
	}

	//virtual void GetPlatformSpecificProjectAnalytics(TArray<struct FAnalyticsEventAttribute>& AnalyticsParamArray) const override {};

#if WITH_ENGINE
	virtual FName FinalizeVirtualTextureLayerFormat(FName Format) const override
	{
#if WITH_EDITOR
		// The flavorless SimplyStream target and its DXT flavor are the WebGPU BC path. Keep
		// their DXT/BC virtual-texture formats intact: remapping them to ETC2 both bloats the
		// cook and produces payloads the BC runtime did not request. The ASTC/ETC2 flavors
		// retain the mobile RVT workaround below.
		static const FName ASTCFlavor(TEXT("ASTC"));
		static const FName ETC2Flavor(TEXT("ETC2"));
		const FName CookFlavor = this->PlatformInfo->PlatformFlavor;
		if (CookFlavor != ASTCFlavor && CookFlavor != ETC2Flavor)
		{
			return Format;
		}

		// Remap non-ETC variants to ETC

		// VirtualTexture Format was already run through the ordinary texture remaps to change AutoDXT to ASTC or ETC
		// this then runs again
		// currently it forces all ASTC to ETC
		// this is needed because the runtime virtual texture encoder only supports ETC

		// code dupe with IOSTargetPlatform

		// @todo Oodle: restrict this so it's only done when needed for RVT, not for all VT that would be better left as ASTC ; UE-212640

		const static FName VTRemap[][2] =
		{
			{ { FName(TEXT("ASTC_RGB")) },			{ SimplyStreamTexFormat::NameETC2_RGB } },
			{ { FName(TEXT("ASTC_RGBA")) },			{ SimplyStreamTexFormat::NameETC2_RGBA } },
			{ { FName(TEXT("ASTC_RGBA_HQ")) },		{ SimplyStreamTexFormat::NameETC2_RGBA } },
			//		{ { FName(TEXT("ASTC_RGB_HDR")) },		{ NameRGBA16F } }, // ?
					{ { FName(TEXT("ASTC_RGBAuto")) },		{ SimplyStreamTexFormat::NameAutoETC2 } },
					{ { FName(TEXT("ASTC_NormalAG")) },		{ SimplyStreamTexFormat::NameETC2_RGB } },

					{ { SimplyStreamTexFormat::NameASTC_NormalRG },	{ SimplyStreamTexFormat::NameETC2_RG11 } },
					{ { SimplyStreamTexFormat::NameASTC_NormalLA	},	{ SimplyStreamTexFormat::NameETC2_RG11 } },

					{ { SimplyStreamTexFormat::NameDXT1 },		{ SimplyStreamTexFormat::NameETC2_RGB } },
					{ { SimplyStreamTexFormat::NameDXT5 },		{ SimplyStreamTexFormat::NameETC2_RGBA } },
					{ { SimplyStreamTexFormat::NameAutoDXT },	{ SimplyStreamTexFormat::NameAutoETC2 } }
		};

		for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(VTRemap); RemapIndex++)
		{
			if (VTRemap[RemapIndex][0] == Format)
			{
				return VTRemap[RemapIndex][1];
			}
		}
#endif
		return Format;
	}
	virtual void GetTextureFormats( const UTexture* InTexture, TArray< TArray<FName> >& OutFormats) const override
	{
		// FSimplyStreamTargetPlatformControls aside from being the base class for all the concrete simplystream target platforms
		//	it is also usable on its own as "flavorless" SimplyStream
		//	but I don't understand how that's supposed to work or what that's supposed to mean
		//	and no information has been forthcoming
		check(InTexture);

		const bool bSupportCompressedVolumeTexture = false; // SimplyStreamTargetPlatformSettings->SupportsTextureFormatCategory(ESimplyStreamTextureFormatCategory::ASTC);
		// OpenGL ES has F32 textures but doesn't allow linear filtering unless OES_texture_float_linear
		const bool bSupportFilteredFloat32Textures = false;

		bool bCompressLandscapeWeightMaps = false;

		TSuper::GetTargetPlatformSettings()->GetConfigSystem()->GetBool(TEXT("/Script/Engine.RendererSettings"), TEXT("r.Mobile.CompressLandscapeWeightMaps"), bCompressLandscapeWeightMaps, GEngineIni);

		TArray<FName>& LayerFormats = OutFormats.AddDefaulted_GetRef();
		int32 BlockSize = 1; // this looks wrong? should be 4 for FSimplyStream_DXTTargetPlatform ? - it is wrong, but BlockSize is ignored

		GetDefaultTextureFormatNamePerLayer(LayerFormats, this->GetTargetPlatformSettings(), this, InTexture, bSupportCompressedVolumeTexture, BlockSize, bSupportFilteredFloat32Textures);

		for (FName& TextureFormatName : LayerFormats)
		{
			if (InTexture->LODGroup == TEXTUREGROUP_Terrain_Weightmap && bCompressLandscapeWeightMaps)
			{
				TextureFormatName = SimplyStreamTexFormat::NameAutoDXT;
			}

			if (InTexture->GetTextureClass() == ETextureClass::Cube)
			{
				FTextureFormatSettings FormatSettings;
				InTexture->GetDefaultFormatSettings(FormatSettings);
				// TC_EncodedReflectionCapture is no longer used and could be deleted
				if (FormatSettings.CompressionSettings == TC_EncodedReflectionCapture && !FormatSettings.CompressionNone)
				{
					TextureFormatName = FName(TEXT("ETC2_RGBA"));
				}
			}

			for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(SimplyStreamTexFormat::GenericRemap); ++RemapIndex)
			{
				if (TextureFormatName == SimplyStreamTexFormat::GenericRemap[RemapIndex][0])
				{
					TextureFormatName = SimplyStreamTexFormat::GenericRemap[RemapIndex][1];
				}
			}
		}
	}

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		GetAllDefaultTextureFormats(this->GetTargetPlatformSettings(), OutFormats);

		for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(SimplyStreamTexFormat::GenericRemap); ++RemapIndex)
		{
			OutFormats.Remove(SimplyStreamTexFormat::GenericRemap[RemapIndex][0]);
		}

		for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(SimplyStreamTexFormat::GenericRemap); ++RemapIndex)
		{
			OutFormats.AddUnique(SimplyStreamTexFormat::GenericRemap[RemapIndex][1]);
		}
	}

	virtual FName GetWaveFormat( const class USoundWave* Wave ) const override
	{
		static const FName NAME_OGG(TEXT("OGG"));
		static const FName NAME_RADA(TEXT("RADA"));

		// Streamed waves need a codec that supports *seekable* streaming so looping sounds can
		// seek back to frame 0 each loop. OGG/Vorbis is the only engine codec that advertises
		// bSupportsSeekableStreaming=false (VorbisAudioInfo.cpp), so a streamed-OGG loop warns
		// "Attempt to seek on non-seekable wave ... to frame 0" and goes silent. RAD Audio (RADA)
		// supports seekable streaming, decodes on the wasm64 RAD decoder the platform ships
		// (RadAudioDecoder_SimplyStream), and is low-CPU — ideal for the wasm audio thread.
		// Non-streamed waves are fully resident (seekable regardless), so they stay on OGG.
		return Wave->IsStreaming() ? NAME_RADA : NAME_OGG;
	}

	virtual void GetAllWaveFormats(TArray<FName>& OutFormats) const override
	{
		static const FName NAME_ADPCM(TEXT("ADPCM"));
		static const FName NAME_OGG(TEXT("OGG"));
		static const FName NAME_RADA(TEXT("RADA"));

		OutFormats.Add(NAME_ADPCM);
		OutFormats.Add(NAME_OGG);
		OutFormats.Add(NAME_RADA);
	}

#endif // WITH_ENGINE
	virtual bool SupportsVariants() const override
	{
		return true;
	}

	//~ End ITargetPlatform Interface
protected:
	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const
	{
		return true;
	}

	// Holds the local device.
	TMap<FString, FSimplyStreamTargetDevicePtr> Devices;

#if WITH_ENGINE
	ITargetDevicePtr DefaultDevice;
#endif

	// The name of the default device
	FString DefaultDeviceName;
};

template<typename TProperties>
class FSimplyStream_DXTTargetPlatformControls : public FSimplyStreamTargetPlatformControls<TProperties>
{
public:
	FSimplyStream_DXTTargetPlatformControls(bool bIsClient, ITargetPlatformSettings* TargetPlatformSettings) : FSimplyStreamTargetPlatformControls<TProperties>(bIsClient, TargetPlatformSettings, TEXT("DXT"))
	{
	}

	/*virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return (ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc")));
	}*/

	virtual float GetVariantPriority() const override
	{
		return 0.5;
	}
};

template<typename TProperties>
class FSimplyStream_ASTCTargetPlatformControls : public FSimplyStreamTargetPlatformControls<TProperties>
{
public:
	typedef FSimplyStreamTargetPlatformControls<TProperties> TSuper;

	FSimplyStream_ASTCTargetPlatformControls(bool bIsClient, ITargetPlatformSettings* TargetPlatformSettings, const TCHAR* OverrideIniPlatformName = TEXT("ASTC"))
		: FSimplyStreamTargetPlatformControls<TProperties>(bIsClient, TargetPlatformSettings, OverrideIniPlatformName)
	{
	}

	//virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	//{
	//	return ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr"));
	//}

	virtual float GetVariantPriority() const override
	{
		return 0.5f;
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const override
	{
		FSimplyStreamTargetPlatformControls<TProperties>::GetTextureFormats(Texture, OutFormats);

		// L+A mode for normal map compression
		const bool bSupportsNormalLA = TSuper::SupportsFeature(ETargetPlatformFeatures::NormalmapLAEncodingMode);

		// perform any remapping away from defaults
		TArray<FName>& LayerFormats = OutFormats.Last();
		for (FName& TextureFormatName : LayerFormats)
		{
			if (bSupportsNormalLA && TextureFormatName == SimplyStreamTexFormat::NameBC5)
			{
				TextureFormatName = SimplyStreamTexFormat::NameASTC_NormalLA;
				continue;
			}

			for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(SimplyStreamTexFormat::ASTCRemap); ++RemapIndex)
			{
				if (TextureFormatName == SimplyStreamTexFormat::ASTCRemap[RemapIndex][0])
				{
					TextureFormatName = SimplyStreamTexFormat::ASTCRemap[RemapIndex][1];
					break;
				}
			}
		}

		bool bSupportASTCHDR = TSuper::UsesASTCHDR();

		if (!bSupportASTCHDR)
		{
			for (FName& TextureFormatName : LayerFormats)
			{
				if (TextureFormatName == SimplyStreamTexFormat::NameASTC_RGB_HDR)
				{
					TextureFormatName = SimplyStreamTexFormat::NameRGBA16F;
				}
			}
		}
	}
	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		FSimplyStreamTargetPlatformControls<TProperties>::GetAllTextureFormats(OutFormats);

		for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(SimplyStreamTexFormat::ASTCRemap); ++RemapIndex)
		{
			OutFormats.Remove(SimplyStreamTexFormat::ASTCRemap[RemapIndex][0]);
		}

		// ASTC for compressed textures
		OutFormats.Add(SimplyStreamTexFormat::NameAutoASTC);
		// ETC for ETC2_R11
		OutFormats.Add(SimplyStreamTexFormat::NameAutoETC2);
	}
#endif
};

template<typename TProperties>
class FSimplyStream_ETCTargetPlatformControls : public FSimplyStreamTargetPlatformControls<TProperties>
{
public:

	FSimplyStream_ETCTargetPlatformControls(bool bIsClient, ITargetPlatformSettings* TargetPlatformSettings) : FSimplyStreamTargetPlatformControls<TProperties>(bIsClient, TargetPlatformSettings, TEXT("ETC"))
	{
	}

	// End FSimplyStreamTargetPlatform overrides

	//virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	//{
	//	return ExtensionsString.Contains(TEXT("GL_OES_compressed_ETC1_RGB8_texture"));
	//}

	virtual float GetVariantPriority() const override
	{
		return 0.5f;
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const override
	{
		FSimplyStreamTargetPlatformControls<TProperties>::GetTextureFormats(Texture, OutFormats);

		// perform any remapping away from defaults
		TArray<FName>& LayerFormats = OutFormats.Last();
		for (FName& TextureFormatName : LayerFormats)
		{
			for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(SimplyStreamTexFormat::ETCRemap); ++RemapIndex)
			{
				if (TextureFormatName == SimplyStreamTexFormat::ETCRemap[RemapIndex][0])
				{
					TextureFormatName = SimplyStreamTexFormat::ETCRemap[RemapIndex][1];
					break;
				}
			}
		}
	}
	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		FSimplyStreamTargetPlatformControls<TProperties>::GetAllTextureFormats(OutFormats);

		for (int32 RemapIndex = 0; RemapIndex < UE_ARRAY_COUNT(SimplyStreamTexFormat::ETCRemap); ++RemapIndex)
		{
			OutFormats.Remove(SimplyStreamTexFormat::ETCRemap[RemapIndex][0]);
		}

		// support only ETC for compressed textures
		OutFormats.Add(SimplyStreamTexFormat::NameAutoETC2);
	}
#endif
};

template<typename TProperties>
class FSimplyStream_MultiTargetPlatformControls : public FSimplyStreamTargetPlatformControls<TProperties>
{
	typedef FSimplyStreamTargetPlatformControls<TProperties> TSuper;
	TArray<TSuper*> FormatTargetPlatforms;
	FString FormatTargetString;

public:
	FSimplyStream_MultiTargetPlatformControls(bool bIsClient, ITargetPlatformSettings* TargetPlatformSettings) : FSimplyStreamTargetPlatformControls<TProperties>(bIsClient, TargetPlatformSettings, TEXT("Multi"))
	{
	}

	// set up all of the multiple formats together into this one
	void LoadFormats(TArray<TSuper*> SingleFormatTPs)
	{
		// sort formats by priority so higher priority formats are packaged (and thus used by the device) first
		// note that we passed this by value, not ref, so we can sort it
		SingleFormatTPs.Sort([](const TSuper& A, const TSuper& B)
			{
				float PriorityA = 0.f;
				float PriorityB = 0.f;
				FString VariantA = A.GetSimplyStreamVariantName().Replace(TEXT("Client"), TEXT(""));
				FString VariantB = B.GetSimplyStreamVariantName().Replace(TEXT("Client"), TEXT(""));
				GConfig->GetFloat(TEXT("/Script/SimplyStreamRuntimeSettings.SimplyStreamRuntimeSettings"), *(FString(TEXT("TextureFormatPriority_")) + VariantA), PriorityA, GEngineIni);
				GConfig->GetFloat(TEXT("/Script/SimplyStreamRuntimeSettings.SimplyStreamRuntimeSettings"), *(FString(TEXT("TextureFormatPriority_")) + VariantB), PriorityB, GEngineIni);
				return PriorityA > PriorityB;
			});

		FormatTargetPlatforms.Empty();
		FormatTargetString = TEXT("");

		TSet<FString> SeenFormats;

		// Load the TargetPlatform module for each format
		for (FSimplyStreamTargetPlatformControls<TProperties>* SingleFormatTP : SingleFormatTPs)
		{
			// only use once each
			if (SeenFormats.Contains(SingleFormatTP->GetSimplyStreamVariantName()))
			{
				continue;
			}
			SeenFormats.Add(SingleFormatTP->GetSimplyStreamVariantName());

			bool bEnabled = false;
			FString SettingsName = FString(TEXT("bMultiTargetFormat_")) + *SingleFormatTP->GetSimplyStreamVariantName();
			GConfig->GetBool(TEXT("/Script/SimplyStreamRuntimeSettings.SimplyStreamRuntimeSettings"), *SettingsName, bEnabled, GEngineIni);
			if (bEnabled)
			{
				if (FormatTargetPlatforms.Num())
				{
					FormatTargetString += TEXT(",");
				}
				FormatTargetString += SingleFormatTP->GetSimplyStreamVariantName();
				FormatTargetPlatforms.Add(SingleFormatTP);
			}
		}

		// @todo do we need this with DisplayName below?
		PlatformInfo::UpdatePlatformDisplayName(TEXT("SimplyStream_Multi"), DisplayName());
	}

	virtual float GetVariantPriority() const override
	{
		// lowest priority so specific variants are chosen first
		return 0.5f;
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const;
	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;
#endif	
	virtual FText DisplayName() const override;
};


// Flavorless SimplyStream platform defaults to MultiTargetPlatformControls
template<typename TProperties>
class FSimplyStream_DefaultTargetPlatformControls : public FSimplyStream_MultiTargetPlatformControls<TProperties>
{
public:
	FSimplyStream_DefaultTargetPlatformControls(bool bIsClient, ITargetPlatformSettings* TargetPlatformSettings)
		: FSimplyStream_MultiTargetPlatformControls<TProperties>(bIsClient, TargetPlatformSettings)
	{
	}
};
#undef LOCTEXT_NAMESPACE
