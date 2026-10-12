// Copyright Epic Games, Inc. All Rights Reserved.

#include "IWebTransportNetworkingModule.h"
#include "WebTransportServer.h"
#include "WebTransportNetworkingPrivate.h"
#include "Modules/ModuleManager.h"
#include "WebTransport.h"
#include "Misc/CoreDelegates.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "../../../../../../Source/ThirdParty/wasm_utils/wndr_probe.h"

// Completes the WebTransport->WebSocket async fallback. The net driver, on a failed/timed-out WebTransport
// connect, sets GWebTransportSessionFailed (-> IsAvailable() false) + records the travel URL, then
// BroadcastNetworkFailure tears down the pending connection. This handler catches that and re-travels to the
// recorded URL — which now resolves to the WebSocket driver (the engine's DriverClassNameFallback fires
// synchronously because IsAvailable() is now false). One-shot per failed session.
static void HandleWebTransportNetworkFailure(UWorld* World, UNetDriver* /*NetDriver*/, ENetworkFailure::Type /*FailureType*/, const FString& /*ErrorString*/)
{
	if (!GWebTransportSessionFailed || GWebTransportRetryURL.IsEmpty())
	{
		return;
	}

	UWorld* TravelWorld = GWebTransportRetryWorld.IsValid() ? GWebTransportRetryWorld.Get() : World;
	const FString Url = GWebTransportRetryURL;

	// Consume the retry so we don't loop (a subsequent WebSocket failure is a genuine failure).
	WndrMarkNetTravelStage(WNDR_NET_STAGE_WT_NETWORK_FAILURE_RETRY);
	GWebTransportRetryURL.Reset();
	GWebTransportRetryWorld.Reset();

	if (TravelWorld && GEngine)
	{
		// Defer one tick: the engine's (and Lyra's) own failure handling runs return-to-entry synchronously in
		// this same broadcast and CLOBBERS an immediate SetClientTravel — the fallback re-travel deterministically
		// lost the race and the client was stranded on the frontend (observed on the first real M3 joins). A
		// next-tick travel lands after all failure handlers, whatever their delegate order.
		TWeakObjectPtr<UWorld> WeakWorld = TravelWorld;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld, Url](float) -> bool
		{
			if (GEngine)
			{
				// The failure browse may have swapped worlds; re-resolve a live one.
				UWorld* World = WeakWorld.IsValid() ? WeakWorld.Get() : nullptr;
				if (!World)
				{
					for (const FWorldContext& Context : GEngine->GetWorldContexts())
					{
						if (Context.WorldType == EWorldType::Game && Context.World())
						{
							World = Context.World();
							break;
						}
					}
				}
				if (World)
				{
					UE_LOG(LogWebTransportNetworking, Warning, TEXT("Re-traveling over WebSocket to %s"), *Url);
					GEngine->SetClientTravel(World, *Url, TRAVEL_Absolute);
				}
			}
			return false; // one-shot
		}));
	}
}

class FWebTransportNetworkingPlugin : public IWebTransportNetworkingModule
{
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle PostLoadMapHandle;

	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		// GEngine may not exist yet at module load; bind after engine init.
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FWebTransportNetworkingPlugin::BindNetworkFailure);
	}

	virtual void ShutdownModule() override
	{
		if (GEngine && NetworkFailureHandle.IsValid())
		{
			GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		}
		if (PostLoadMapHandle.IsValid())
		{
			FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		}
	}

	void BindNetworkFailure()
	{
		if (GEngine)
		{
			NetworkFailureHandle = GEngine->OnNetworkFailure().AddStatic(&HandleWebTransportNetworkFailure);
			PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([](UWorld* World)
			{
				if (World && World->WorldType == EWorldType::Game)
				{
					WndrMarkNetTravelStage(WNDR_NET_STAGE_POST_LOAD_MAP);
				}
			});
		}
	}

public:
	TUniquePtr<IWebTransportServer> CreateServer() override
	{
		return MakeUnique<FWebTransportServer>();
	}

	TSharedPtr<INetworkingWebTransport> CreateConnection(const FInternetAddr& ServerAddress) override
	{
		return MakeShared<FWebTransport>(ServerAddress);
	}

};

// Module name MUST match the .Build.cs class name and the .uplugin module entry. The /Script package
// for UWebTransportNetDriver/UWebTransportConnection is keyed off this name → /Script/WebTransportNetworkingSimplyStream.
IMPLEMENT_MODULE(FWebTransportNetworkingPlugin, WebTransportNetworkingSimplyStream)

DEFINE_LOG_CATEGORY(LogWebTransportNetworking);
