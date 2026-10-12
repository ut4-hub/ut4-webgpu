// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	SimplyStreamProperties.h - Basic static properties of a platform
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformProperties.h"


/**
 * Implements SimplyStream platform properties.
 */
template<bool IS_DEDICATED_SERVER, bool IS_CLIENT_ONLY>
struct FSimplyStreamPlatformProperties : public FGenericPlatformProperties
{
	static constexpr FORCEINLINE bool HasEditorOnlyData( )
	{
		return false;
	}

	static constexpr FORCEINLINE const char* PlatformName()
	{
		if (IS_DEDICATED_SERVER)
		{
			return "SimplyStreamServer";
		}

		if (IS_CLIENT_ONLY)
		{
			return "SimplyStreamClient";
		}

		return "SimplyStream";
	}

	static constexpr FORCEINLINE const char* IniPlatformName()
	{
		return "SimplyStream";
	}

	static constexpr FORCEINLINE const TCHAR* GetRuntimeSettingsClassName()
	{
		return TEXT("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings");
	}

	static constexpr FORCEINLINE bool RequiresCookedData( )
	{
		return true;
	}

	static constexpr FORCEINLINE bool HasSecurePackageFormat()
	{
		return IS_DEDICATED_SERVER;
	}

	static constexpr FORCEINLINE bool IsServerOnly()
	{
		return IS_DEDICATED_SERVER;
	}

	static constexpr FORCEINLINE bool IsClientOnly()
	{
		return IS_CLIENT_ONLY;
	}

	static FORCEINLINE bool SupportsBuildTarget(EBuildTargetType BuildTarget)
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsHighQualityLightmaps()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsTextureStreaming()
	{
		return true;
	}

	static constexpr FORCEINLINE bool SupportsMeshLODStreaming()
	{
		return true;
	}

	static constexpr FORCEINLINE bool HasFixedResolution()
	{
		return false;
	}

	static constexpr FORCEINLINE bool AllowsFramerateSmoothing()
	{
		return false;
	}

	static constexpr FORCEINLINE bool SupportsWindowedMode()
	{
		return false;
	}

	static constexpr FORCEINLINE bool SupportsAudioStreaming()
	{
		return !IsServerOnly();
	}

	static constexpr FORCEINLINE float GetVariantPriority()
	{
		if (IS_DEDICATED_SERVER)
		{
			return 0.0f;
		}

		if (IS_CLIENT_ONLY)
		{
			return 0.0f;
		}

		return 1.0f;
	}
};

#ifdef PROPERTY_HEADER_SHOULD_DEFINE_TYPE
typedef FSimplyStreamPlatformProperties<UE_SERVER, !WITH_SERVER_CODE && !WITH_EDITOR> FPlatformProperties;
#endif
