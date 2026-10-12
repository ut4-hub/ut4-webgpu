// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Common/TargetPlatformSettingsBase.h"
#include "../../../Runtime/Core/Public/SimplyStreamPlatformProperties.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MonitoredProcess.h"
#include "Containers/Ticker.h"

#if WITH_ENGINE
#include "StaticMeshResources.h"
#endif // WITH_ENGINE


/** Listed in order of priority...if device supports multiple formats, first format in list will be chosen */
enum class ESimplyStreamTextureFormatCategory
{
	DXT,
	ETC2,
	ASTC,

	Count,
};

template<typename TProperties>
class SIMPLYSTREAMTARGETPLATFORMSETTINGS_API FSimplyStreamTargetPlatformSettings : public TTargetPlatformSettingsBase<TProperties>
{
public:
	typedef TTargetPlatformSettingsBase<TProperties> TSuper;
	FSimplyStreamTargetPlatformSettings(const TCHAR* CookFlavor = nullptr, const TCHAR* OverrideIniPlatformName = nullptr)
		: TSuper(CookFlavor, OverrideIniPlatformName)
		, MobileShadingPath(0)
		, bDistanceField(false)
		, bMobileForwardEnableClusteredReflections(false)
		, bVirtualTextures(false)
	{
#if WITH_ENGINE
		TextureLODSettings = nullptr;
		StaticMeshLODSettings.Initialize(this);
		
		TSuper::GetConfigSystem()->GetBool(TEXT("/Script/Engine.RendererSettings"), TEXT("r.DistanceFields"), bDistanceField, GEngineIni);
		TSuper::GetConfigSystem()->GetInt(TEXT("/Script/Engine.RendererSettings"), TEXT("r.Mobile.ShadingPath"), MobileShadingPath, GEngineIni);
		TSuper::GetConfigSystem()->GetBool(TEXT("/Script/Engine.RendererSettings"), TEXT("r.Mobile.Forward.EnableClusteredReflections"), bMobileForwardEnableClusteredReflections, GEngineIni);
		// UE 5.8 made r.VirtualTextures the single cook-relevant capability
		// for both mobile and desktop shader platforms. Keep this deprecated
		// feature query aligned with RenderCore::UseVirtualTexturing(TargetPlatform)
		// so the texture cooker and the SimplyStream target never disagree.
		TSuper::GetConfigSystem()->GetBool(TEXT("/Script/Engine.RendererSettings"), TEXT("r.VirtualTextures"), bVirtualTextures, GEngineIni);
		TSuper::GetConfigSystem()->GetString(TEXT("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings"), TEXT("RenderMode"), RenderMode, GEngineIni);
#endif // WITH_ENGINE
	}
	/**
	 * Destructor
	 */
	virtual ~FSimplyStreamTargetPlatformSettings() {}

	virtual bool SupportsTextureFormatCategory(ESimplyStreamTextureFormatCategory Category) const
	{
		return true;
	}

	bool SupportsFeature(ETargetPlatformFeatures Feature) const override
	{
		switch (Feature)
		{
		case ETargetPlatformFeatures::Packaging:
		case ETargetPlatformFeatures::DeviceOutputLog:
			return true;

		case ETargetPlatformFeatures::ShowAggregateDevice:
			return false;

		case ETargetPlatformFeatures::LowQualityLightmaps:
		case ETargetPlatformFeatures::MobileRendering:
			return SupportsWebGL() || SupportsWebGPUES31();

		case ETargetPlatformFeatures::HighQualityLightmaps:
		case ETargetPlatformFeatures::DeferredRendering:
			return SupportsWebGPUSM5();

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		case ETargetPlatformFeatures::VirtualTextureStreaming:
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
			return bVirtualTextures;

		case ETargetPlatformFeatures::DistanceFieldAO:
			return UsesDistanceFields();
		case ETargetPlatformFeatures::NormalmapLAEncodingMode:
			return false;
#if 0
		case ETargetPlatformFeatures::NormalmapLAEncodingMode:
		{
			static IConsoleVariable* CompressorCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("cook.ASTCTextureCompressor"));
			const bool bUsesARMCompressor = (CompressorCVar ? (CompressorCVar->GetInt() != 0) : false);
			return SupportsTextureFormatCategory(ESimplyStreamTextureFormatCategory::ASTC) && bUsesARMCompressor;
		}
#endif
		default:
			break;
		}

		return TSuper::SupportsFeature(Feature);
	}

	void GetAllPossibleShaderFormats(TArray<FName>& OutFormats) const override
	{
		static FName NAME_GLSL_ES3_WEBGL(TEXT("GLSL_ES3_WEBGL"));
		static FName NAME_WEBGPU_ES31(TEXT("SF_WEBGPU_ES31"));
		static FName NAME_WEBGPU_SM5(TEXT("SF_WEBGPU_SM5"));
		if (SupportsWebGPUES31())
		{
			OutFormats.AddUnique(NAME_WEBGPU_ES31);
		}

		if (SupportsWebGPUSM5())
		{
			OutFormats.AddUnique(NAME_WEBGPU_SM5);
		}

		if (SupportsWebGL())
		{
			OutFormats.AddUnique(NAME_GLSL_ES3_WEBGL);
		}
	}

	void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const override
	{
		GetAllPossibleShaderFormats(OutFormats);
	}

	virtual bool UsesRayTracing() const override
	{
		bool bEnableRayTracing = false;
		GConfig->GetBool(TEXT("/Script/SimplyStreamRuntimeSettings.SimplyStreamRuntimeSettings"), TEXT("bEnableRayTracing"), bEnableRayTracing, GEngineIni);

		return bEnableRayTracing && 0;
	}

#if WITH_ENGINE
	void GetReflectionCaptureFormats(TArray<FName>& OutFormats) const
	{
		const bool bMobileDeferredShading = (MobileShadingPath == 1);

		if (SupportsWebGPUSM5() || bMobileDeferredShading || bMobileForwardEnableClusteredReflections)
		{
			// use Full HDR with SM5 and Mobile Deferred
			OutFormats.Add(FName(TEXT("FullHDR")));
		}

		// always emit encoded
		OutFormats.Add(FName(TEXT("EncodedHDR")));
	}
	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
	}
	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings() const override
	{
		return StaticMeshLODSettings;
	}

	virtual const UTextureLODSettings& GetTextureLODSettings() const override
	{
		return *TextureLODSettings;
	}

	virtual bool UsesDistanceFields() const override
	{
		return bDistanceField;
	}

#endif
	// query for rendering mode support
	bool SupportsWebGL() const
	{
		return RenderMode == TEXT("WebGL");
	}

	bool SupportsWebGPUES31() const
	{
		return RenderMode == TEXT("WebGPUES31") || RenderMode == TEXT("WebGPUALL");
	}

	bool SupportsWebGPUSM5() const
	{
		return RenderMode == TEXT("WebGPUSM5") || RenderMode == TEXT("WebGPUALL");
	}
protected:

	// r.Mobile.ShadingPath value
	int32 MobileShadingPath;

	// true if DistanceField is enabled
	bool bDistanceField;

	// r.Mobile.Forward.EnableClusteredReflections value
	bool bMobileForwardEnableClusteredReflections;

	FString RenderMode;

	// Canonical r.VirtualTextures value shared by ES3_1 and SM5 cooks.
	bool bVirtualTextures;
#if WITH_ENGINE
	// Holds a cache of the target LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;
#endif
};

template<typename TProperties>
class FSimplyStream_DXTTargetPlatformSettings : public FSimplyStreamTargetPlatformSettings<TProperties>
{
public:
	FSimplyStream_DXTTargetPlatformSettings() : FSimplyStreamTargetPlatformSettings<TProperties>(TEXT("DXT"))
	{
	}

	virtual bool SupportsTextureFormatCategory(ESimplyStreamTextureFormatCategory Category) const override
	{
		return Category == ESimplyStreamTextureFormatCategory::DXT;
	}
};
template<typename TProperties>
class FSimplyStream_ASTCTargetPlatformSettings : public FSimplyStreamTargetPlatformSettings<TProperties>
{
public:
	FSimplyStream_ASTCTargetPlatformSettings() : FSimplyStreamTargetPlatformSettings<TProperties>(TEXT("ASTC"))
	{
	}

	virtual bool SupportsTextureFormatCategory(ESimplyStreamTextureFormatCategory Category) const override
	{
		return Category == ESimplyStreamTextureFormatCategory::ASTC;
	}
};
template<typename TProperties>
class FSimplyStream_ETC2TargetPlatformSettings : public FSimplyStreamTargetPlatformSettings<TProperties>
{
public:

	FSimplyStream_ETC2TargetPlatformSettings() : FSimplyStreamTargetPlatformSettings<TProperties>(TEXT("ETC2"))
	{
	}

	virtual bool SupportsTextureFormatCategory(ESimplyStreamTextureFormatCategory Category) const override
	{
		return Category == ESimplyStreamTextureFormatCategory::ETC2;
	}
};
template<typename TProperties>
class FSimplyStream_MultiTargetPlatformSettings : public FSimplyStreamTargetPlatformSettings<TProperties>
{
public:
	FSimplyStream_MultiTargetPlatformSettings() : FSimplyStreamTargetPlatformSettings<TProperties>(TEXT("Multi"))
	{
	}
};
