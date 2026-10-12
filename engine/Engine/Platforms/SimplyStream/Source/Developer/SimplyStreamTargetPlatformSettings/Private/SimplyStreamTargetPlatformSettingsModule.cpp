// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimplyStreamTargetPlatformSettingsModule.h"
#include "CoreMinimal.h"
#include "SimplyStreamTargetPlatformSettings.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformSettingsModule.h"
#include "Interfaces/ITargetPlatformControlsModule.h"
#include "../../../Runtime/Core/Public/SimplyStreamPlatformProperties.h"

#if WITH_EDITOR && WITH_ENGINE
#include "Components/ReflectionCaptureComponent.h"
#include "Engine/MapBuildDataRegistry.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimplyStreamTargetPlatformSettings, Log, All);
#endif

/**
 * Module for the SimplyStreams target platform settings.
 */
class FSimplyStreamTargetPlatformSettingsModule
	: public ISimplyStreamTargetPlatformSettingsModule
{
public:

	/** Destructor. */
	~FSimplyStreamTargetPlatformSettingsModule()
	{
	}

public:
#if WITH_EDITOR && WITH_ENGINE
	virtual void StartupModule() override
	{
		ObjectPreSaveHandle = FCoreUObjectDelegates::OnObjectPreSave.AddRaw(this, &FSimplyStreamTargetPlatformSettingsModule::OnObjectPreSave);
	}

	virtual void ShutdownModule() override
	{
		FCoreUObjectDelegates::OnObjectPreSave.Remove(ObjectPreSaveHandle);
		ObjectPreSaveHandle.Reset();
	}
#endif

	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms)
	{

	}
	void GetSinglePlatformSettings(TMap<FString, ITargetPlatformSettings*>& OutMap)
	{
		OutMap = SinglePlatformSettings;
	}

	ITargetPlatformSettings* GetMultiPlatformSettings()
	{
		return MultiTargetPlatformSettings;
	}

protected:
	virtual void GetTargetPlatformSettings(TArray<ITargetPlatformSettings*>& TargetPlatforms)
	{
		ITargetPlatformSettings* VanillaTP = new FSimplyStreamTargetPlatformSettings<FSimplyStreamPlatformProperties<false, false>>();
		TargetPlatforms.Add(VanillaTP);
		SinglePlatformSettings.Add(FSimplyStreamPlatformProperties<false, false>::PlatformName(), VanillaTP);

		ITargetPlatformSettings* DXTTP = new FSimplyStream_DXTTargetPlatformSettings<FSimplyStreamPlatformProperties<false, false>>();
		TargetPlatforms.Add(DXTTP);
		SinglePlatformSettings.Add(FSimplyStreamPlatformProperties<false, false>::PlatformName() + FString("DXT"), DXTTP);

		ITargetPlatformSettings* ETC2TP = new FSimplyStream_ETC2TargetPlatformSettings<FSimplyStreamPlatformProperties<false, false>>();
		TargetPlatforms.Add(ETC2TP);
		SinglePlatformSettings.Add(FSimplyStreamPlatformProperties<false, false>::PlatformName() + FString("ETC2"), ETC2TP);

		ITargetPlatformSettings* ASTCTP = new FSimplyStream_ASTCTargetPlatformSettings<FSimplyStreamPlatformProperties<false, false>>();
		TargetPlatforms.Add(ASTCTP);
		SinglePlatformSettings.Add(FSimplyStreamPlatformProperties<false, false>::PlatformName() + FString("ASTC"), ASTCTP);

		ITargetPlatformSettings* MultiTP = new FSimplyStreamTargetPlatformSettings<FSimplyStreamPlatformProperties<false, false>>();
		TargetPlatforms.Add(MultiTP);
		MultiTargetPlatformSettings = MultiTP;

		ITargetPlatformSettings* ServerTP = new FSimplyStreamTargetPlatformSettings<FSimplyStreamPlatformProperties<true, false>>();
		TargetPlatforms.Add(ServerTP);
		SinglePlatformSettings.Add(FSimplyStreamPlatformProperties<true, false>::PlatformName() + FString("Server"), ServerTP);
	}
private:
#if WITH_EDITOR && WITH_ENGINE
	struct FReflectionCaptureCookStats
	{
		int32 Components = 0;
		int32 BuildData = 0;
		int32 FullHDR = 0;
		int32 EncodedHDR = 0;
		int32 Convertible = 0;
		int32 Empty = 0;
	};

	struct FCachedReflectionCaptureBuildData
	{
		FString ComponentPath;
		FReflectionCaptureMapBuildData BuildData;
	};

	struct FReflectionCaptureRegistryCookCache
	{
		FString RegistryPath;
		TMap<FGuid, FCachedReflectionCaptureBuildData> Captures;
	};

	static bool IsSimplyStreamCookingTarget(const ITargetPlatform* TargetPlatform)
	{
		return TargetPlatform
			&& (TargetPlatform->IniPlatformName() == TEXT("SimplyStream")
				|| TargetPlatform->PlatformName().StartsWith(TEXT("SimplyStream")));
	}

	static bool WantsEncodedReflectionCaptures(const ITargetPlatform* TargetPlatform)
	{
		TArray<FName> Formats;
		TargetPlatform->GetReflectionCaptureFormats(Formats);
		return Formats.Contains(FName(TEXT("EncodedHDR")));
	}

	static bool WantsFullReflectionCaptures(const ITargetPlatform* TargetPlatform)
	{
		TArray<FName> Formats;
		TargetPlatform->GetReflectionCaptureFormats(Formats);
		return Formats.Num() == 0 || Formats.Contains(FName(TEXT("FullHDR")));
	}

	static bool ReflectionDiagnosticsEnabled()
	{
		const FString Value = FPlatformMisc::GetEnvironmentVariable(TEXT("SS_WEBGPU_REFLECTION_DIAGNOSTICS"));
		return !Value.IsEmpty() && Value != TEXT("0");
	}

	static bool EnsureEncodedReflectionCaptureData(FReflectionCaptureMapBuildData& BuildData)
	{
		if (BuildData.EncodedHDRCapturedData.Num() > 0)
		{
			return false;
		}

		if (BuildData.FullHDRCapturedData.Num() == 0 || BuildData.CubemapSize <= 0)
		{
			return false;
		}

		GenerateEncodedHDRData(BuildData.FullHDRCapturedData, BuildData.CubemapSize, BuildData.EncodedHDRCapturedData);
		return BuildData.EncodedHDRCapturedData.Num() > 0;
	}

	static FReflectionCaptureCookStats CollectReflectionCaptureStats(UMapBuildDataRegistry* Registry)
	{
		FReflectionCaptureCookStats Stats;
		for (TObjectIterator<UReflectionCaptureComponent> It; It; ++It)
		{
			UReflectionCaptureComponent* CaptureComponent = *It;
			AActor* Owner = CaptureComponent ? CaptureComponent->GetOwner() : nullptr;
			ULevel* OwnerLevel = Owner ? Owner->GetLevel() : nullptr;
			if (!CaptureComponent
				|| CaptureComponent->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
				|| !OwnerLevel
				|| !OwnerLevel->GetWorld()
				|| UMapBuildDataRegistry::Get(CaptureComponent) != Registry)
			{
				continue;
			}

			++Stats.Components;
			FReflectionCaptureMapBuildData* CaptureBuildData = Registry->GetReflectionCaptureBuildData(CaptureComponent->MapBuildDataId);
			if (!CaptureBuildData)
			{
				continue;
			}

			++Stats.BuildData;
			const bool bHasFullHDR = CaptureBuildData->FullHDRCapturedData.Num() > 0;
			const bool bHasEncodedHDR = CaptureBuildData->EncodedHDRCapturedData.Num() > 0;
			Stats.FullHDR += bHasFullHDR ? 1 : 0;
			Stats.EncodedHDR += bHasEncodedHDR ? 1 : 0;
			Stats.Convertible += (bHasFullHDR && !bHasEncodedHDR) ? 1 : 0;
			Stats.Empty += (!bHasFullHDR && !bHasEncodedHDR) ? 1 : 0;
		}
		return Stats;
	}

	static void LogReflectionCaptureStats(const TCHAR* Phase, UMapBuildDataRegistry* Registry, const ITargetPlatform* TargetPlatform)
	{
		const FReflectionCaptureCookStats Stats = CollectReflectionCaptureStats(Registry);
		UE_LOG(LogSimplyStreamTargetPlatformSettings, Display,
			TEXT("Reflection cook %s target=%s registry=%s components=%d buildData=%d full=%d encoded=%d convertible=%d empty=%d"),
			Phase,
			*TargetPlatform->PlatformName(),
			*Registry->GetOutermost()->GetName(),
			Stats.Components,
			Stats.BuildData,
			Stats.FullHDR,
			Stats.EncodedHDR,
			Stats.Convertible,
			Stats.Empty);
	}

	static void LogReflectionCaptureComponent(
		UReflectionCaptureComponent* CaptureComponent,
		UMapBuildDataRegistry* Registry,
		const ITargetPlatform* TargetPlatform,
		bool bGeneratedEncodedData)
	{
		FReflectionCaptureMapBuildData* CaptureBuildData = CaptureComponent->GetMapBuildData();
		UE_LOG(LogSimplyStreamTargetPlatformSettings, Display,
			TEXT("Reflection component cook target=%s registry=%s component=%s buildData=%s cubemap=%d fullBytes=%d encodedBytes=%d generatedEncoded=%s"),
			*TargetPlatform->PlatformName(),
			Registry ? *Registry->GetOutermost()->GetName() : TEXT("<none>"),
			*CaptureComponent->GetPathName(),
			CaptureBuildData ? TEXT("yes") : TEXT("no"),
			CaptureBuildData ? CaptureBuildData->CubemapSize : 0,
			CaptureBuildData ? CaptureBuildData->FullHDRCapturedData.Num() : 0,
			CaptureBuildData ? CaptureBuildData->EncodedHDRCapturedData.Num() : 0,
			bGeneratedEncodedData ? TEXT("yes") : TEXT("no"));
	}

	void CacheReflectionCaptureBuildData(UReflectionCaptureComponent* CaptureComponent, const ITargetPlatform* TargetPlatform, bool bDiagnostics)
	{
		if (!CaptureComponent || CaptureComponent->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			return;
		}

		UMapBuildDataRegistry* Registry = UMapBuildDataRegistry::Get(CaptureComponent);
		FReflectionCaptureMapBuildData* CaptureBuildData = CaptureComponent->GetMapBuildData();
		if (!Registry || !CaptureBuildData || (CaptureBuildData->FullHDRCapturedData.Num() == 0 && CaptureBuildData->EncodedHDRCapturedData.Num() == 0))
		{
			if (bDiagnostics)
			{
				LogReflectionCaptureComponent(CaptureComponent, Registry, TargetPlatform, false);
			}
			return;
		}

		FCachedReflectionCaptureBuildData CachedCapture;
		CachedCapture.ComponentPath = CaptureComponent->GetPathName();
		CachedCapture.BuildData = *CaptureBuildData;
		const bool bGeneratedEncodedData = EnsureEncodedReflectionCaptureData(CachedCapture.BuildData);

		FReflectionCaptureRegistryCookCache& RegistryCache = ReflectionCaptureCookCache.FindOrAdd(Registry);
		RegistryCache.RegistryPath = Registry->GetOutermost()->GetName();
		RegistryCache.Captures.Add(CaptureComponent->MapBuildDataId, MoveTemp(CachedCapture));

		if (bDiagnostics)
		{
			LogReflectionCaptureComponent(CaptureComponent, Registry, TargetPlatform, bGeneratedEncodedData);
		}
	}

	void ApplyCachedReflectionCaptureBuildData(UMapBuildDataRegistry* Registry, const ITargetPlatform* TargetPlatform, bool bDiagnostics)
	{
		FReflectionCaptureRegistryCookCache* RegistryCache = ReflectionCaptureCookCache.Find(Registry);
		if (!RegistryCache)
		{
			return;
		}

		const bool bWantsFullHDR = WantsFullReflectionCaptures(TargetPlatform);
		TSet<FGuid> ResourcesToKeep;
		ResourcesToKeep.Reserve(RegistryCache->Captures.Num());
		for (const TPair<FGuid, FCachedReflectionCaptureBuildData>& CachedPair : RegistryCache->Captures)
		{
			ResourcesToKeep.Add(CachedPair.Key);
		}

		// Legacy maps can carry orphan reflection-capture build records. Those records pass the
		// editor save check with FullHDR, but an ES3_1 cook serializes only EncodedHDR and leaves
		// empty runtime entries. Keep only captures that exist in the loaded world for this registry.
		Registry->InvalidateReflectionCaptures(&ResourcesToKeep);

		int32 AddedCaptures = 0;
		int32 UpdatedCaptures = 0;
		int32 GeneratedEncodedCaptures = 0;

		for (const TPair<FGuid, FCachedReflectionCaptureBuildData>& CachedPair : RegistryCache->Captures)
		{
			const FCachedReflectionCaptureBuildData& CachedCapture = CachedPair.Value;
			FReflectionCaptureMapBuildData* ExistingBuildData = Registry->GetReflectionCaptureBuildData(CachedPair.Key);
			FReflectionCaptureMapBuildData& TargetBuildData = ExistingBuildData
				? *ExistingBuildData
				: Registry->AllocateReflectionCaptureBuildData(CachedPair.Key, false);

			bool bUpdated = false;
			if (!ExistingBuildData)
			{
				TargetBuildData = CachedCapture.BuildData;
				++AddedCaptures;
				bUpdated = true;
			}
			else
			{
				if (TargetBuildData.CubemapSize <= 0 && CachedCapture.BuildData.CubemapSize > 0)
				{
					TargetBuildData.CubemapSize = CachedCapture.BuildData.CubemapSize;
					bUpdated = true;
				}

				if (TargetBuildData.AverageBrightness == 0.0f && CachedCapture.BuildData.AverageBrightness != 0.0f)
				{
					TargetBuildData.AverageBrightness = CachedCapture.BuildData.AverageBrightness;
					bUpdated = true;
				}

				if (bWantsFullHDR && TargetBuildData.FullHDRCapturedData.Num() == 0 && CachedCapture.BuildData.FullHDRCapturedData.Num() > 0)
				{
					TargetBuildData.FullHDRCapturedData = CachedCapture.BuildData.FullHDRCapturedData;
					bUpdated = true;
				}

				if (TargetBuildData.EncodedHDRCapturedData.Num() == 0 && CachedCapture.BuildData.EncodedHDRCapturedData.Num() > 0)
				{
					TargetBuildData.EncodedHDRCapturedData = CachedCapture.BuildData.EncodedHDRCapturedData;
					bUpdated = true;
				}
			}

			if (EnsureEncodedReflectionCaptureData(TargetBuildData))
			{
				++GeneratedEncodedCaptures;
				bUpdated = true;
			}

			if (bUpdated)
			{
				++UpdatedCaptures;
			}
		}

		if (bDiagnostics || AddedCaptures > 0 || UpdatedCaptures > 0 || GeneratedEncodedCaptures > 0)
		{
			UE_LOG(LogSimplyStreamTargetPlatformSettings, Display,
				TEXT("Reflection registry cook target=%s registry=%s cached=%d kept=%d added=%d updated=%d generatedEncoded=%d"),
				*TargetPlatform->PlatformName(),
				*Registry->GetOutermost()->GetName(),
				RegistryCache->Captures.Num(),
				ResourcesToKeep.Num(),
				AddedCaptures,
				UpdatedCaptures,
				GeneratedEncodedCaptures);
		}
	}

	void OnObjectPreSave(UObject* Object, FObjectPreSaveContext SaveContext)
	{
		const ITargetPlatform* TargetPlatform = SaveContext.GetTargetPlatform();
		if (!SaveContext.IsCooking() || !IsSimplyStreamCookingTarget(TargetPlatform) || !WantsEncodedReflectionCaptures(TargetPlatform))
		{
			return;
		}

		const bool bDiagnostics = ReflectionDiagnosticsEnabled();
		if (UReflectionCaptureComponent* CaptureComponent = Cast<UReflectionCaptureComponent>(Object))
		{
			CacheReflectionCaptureBuildData(CaptureComponent, TargetPlatform, bDiagnostics);
			return;
		}

		UMapBuildDataRegistry* Registry = Cast<UMapBuildDataRegistry>(Object);
		if (!Registry)
		{
			return;
		}

		if (bDiagnostics)
		{
			LogReflectionCaptureStats(TEXT("pre"), Registry, TargetPlatform);
		}

		ApplyCachedReflectionCaptureBuildData(Registry, TargetPlatform, bDiagnostics);

		// ES3_1 cooked runtime strips FullHDR and requires EncodedHDR. Run the stock
		// legacy conversion immediately before SimplyStream cook serialization too.
		Registry->HandleLegacyEncodedCubemapData();

		if (bDiagnostics)
		{
			LogReflectionCaptureStats(TEXT("post"), Registry, TargetPlatform);
		}
	}

	FDelegateHandle ObjectPreSaveHandle;
	TMap<UMapBuildDataRegistry*, FReflectionCaptureRegistryCookCache> ReflectionCaptureCookCache;
#endif

	ITargetPlatformSettings* MultiTargetPlatformSettings;
	TMap<FString, ITargetPlatformSettings*> SinglePlatformSettings;
};

IMPLEMENT_MODULE(FSimplyStreamTargetPlatformSettingsModule, SimplyStreamTargetPlatformSettings);
