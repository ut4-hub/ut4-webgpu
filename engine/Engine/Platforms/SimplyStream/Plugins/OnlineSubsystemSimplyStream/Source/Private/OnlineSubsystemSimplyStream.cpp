// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemSimplyStream.h"
#include "OnlineSubsystemSimplyStreamPrivate.h"
#include "OnlineIdentitySimplyStream.h"
#include "OnlineSessionSimplyStream.h"
#include "OnlineVoiceSimplyStream.h"
#include "OnlineFriendsSimplyStream.h"
#include "OnlinePresenceSimplyStream.h"
#include "OnlineStoreSimplyStream.h"
#include "OnlinePurchaseSimplyStream.h"
#include "OnlineStatsSimplyStream.h"
#include "OnlineChatSimplyStream.h"
#include "OnlineAchievementsSimplyStream.h"
#include "OnlineLeaderboardsSimplyStream.h"
#include "OnlineEntitlementsSimplyStream.h"
#include "OnlineExternalUISimplyStream.h"
#include "OnlineSharingSimplyStream.h"
#include "OnlineUserSimplyStream.h"
#include "OnlineEventsSimplyStream.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"

IOnlineSessionPtr FOnlineSubsystemSimplyStream::GetSessionInterface() const
{
	return SessionInterface;
}

IOnlineFriendsPtr FOnlineSubsystemSimplyStream::GetFriendsInterface() const
{
	return FriendsInterface;
}

IOnlinePartyPtr FOnlineSubsystemSimplyStream::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemSimplyStream::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemSimplyStream::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemSimplyStream::GetUserCloudInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemSimplyStream::GetEntitlementsInterface() const
{
	return EntitlementsInterface;
}

IOnlineLeaderboardsPtr FOnlineSubsystemSimplyStream::GetLeaderboardsInterface() const
{
	return LeaderboardsInterface;
}

IOnlineVoicePtr FOnlineSubsystemSimplyStream::GetVoiceInterface() const
{
	return VoiceInterface;
}

IOnlineExternalUIPtr FOnlineSubsystemSimplyStream::GetExternalUIInterface() const
{
	return ExternalUIInterface;
}

IOnlineTimePtr FOnlineSubsystemSimplyStream::GetTimeInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemSimplyStream::GetIdentityInterface() const
{
	return IdentityInterface;
}

IOnlineTitleFilePtr FOnlineSubsystemSimplyStream::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineStoreV2Ptr FOnlineSubsystemSimplyStream::GetStoreV2Interface() const
{
	return StoreInterface;
}

IOnlinePurchasePtr FOnlineSubsystemSimplyStream::GetPurchaseInterface() const
{
	return PurchaseInterface;
}

IOnlineEventsPtr FOnlineSubsystemSimplyStream::GetEventsInterface() const
{
	return EventsInterface;
}

IOnlineAchievementsPtr FOnlineSubsystemSimplyStream::GetAchievementsInterface() const
{
	return AchievementsInterface;
}

IOnlineSharingPtr FOnlineSubsystemSimplyStream::GetSharingInterface() const
{
	return SharingInterface;
}

IOnlineUserPtr FOnlineSubsystemSimplyStream::GetUserInterface() const
{
	return UserInterface;
}

IOnlineMessagePtr FOnlineSubsystemSimplyStream::GetMessageInterface() const
{
	return nullptr;
}

IOnlinePresencePtr FOnlineSubsystemSimplyStream::GetPresenceInterface() const
{
	return PresenceInterface;
}

IOnlineChatPtr FOnlineSubsystemSimplyStream::GetChatInterface() const
{
	return ChatInterface;
}

IOnlineStatsPtr FOnlineSubsystemSimplyStream::GetStatsInterface() const
{
	return StatsInterface;
}

IOnlineTurnBasedPtr FOnlineSubsystemSimplyStream::GetTurnBasedInterface() const
{
	return nullptr;
}

IOnlineTournamentPtr FOnlineSubsystemSimplyStream::GetTournamentInterface() const
{
	return nullptr;
}

IMessageSanitizerPtr FOnlineSubsystemSimplyStream::GetMessageSanitizer(int32 LocalUserNum, FString& OutAuthTypeToExclude) const
{
	return nullptr;
}

FText FOnlineSubsystemSimplyStream::GetOnlineServiceName() const
{
	return NSLOCTEXT("FOnlineSubsystemSimplyStream", "OnlineServiceName", "SimplyStream");
}

FOnlineSubsystemSimplyStream::FOnlineSubsystemSimplyStream(FName InInstanceName)
	: FOnlineSubsystemImpl(SIMPLYSTREAM_SUBSYSTEM, InInstanceName)
{
}

FOnlineSubsystemSimplyStream::~FOnlineSubsystemSimplyStream()
{
}

bool FOnlineSubsystemSimplyStream::Init()
{
	UE_LOG(LogOnline, Log, TEXT("FOnlineSubsystemSimplyStream::Init()"));

	APIBaseURL = TEXT("https://simplystream.com");

	if (GConfig)
	{
		FString ConfigUrl;
		if (GConfig->GetString(
			TEXT("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings"),
			TEXT("BackendURL"),
			ConfigUrl,
			GEngineIni))
		{
			if (!ConfigUrl.IsEmpty())
			{
				APIBaseURL = ConfigUrl;
			}
		}
	}

	const FString EnvUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("SIMPLYSTREAM_BACKEND_URL"));
	if (!EnvUrl.IsEmpty())
	{
		APIBaseURL = EnvUrl;
	}

	IdentityInterface = MakeShared<FOnlineIdentitySimplyStream, ESPMode::ThreadSafe>(this);
	SessionInterface = MakeShared<FOnlineSessionSimplyStream, ESPMode::ThreadSafe>(this);
	VoiceInterface = MakeShared<FOnlineVoiceImplSimplyStream, ESPMode::ThreadSafe>(this);
	if (!VoiceInterface->Init())
	{
		VoiceInterface.Reset();
	}
	FriendsInterface = MakeShared<FOnlineFriendsSimplyStream, ESPMode::ThreadSafe>(this);
	PresenceInterface = MakeShared<FOnlinePresenceSimplyStream, ESPMode::ThreadSafe>(this);
	StoreInterface = MakeShared<FOnlineStoreV2SimplyStream, ESPMode::ThreadSafe>(this);
	PurchaseInterface = MakeShared<FOnlinePurchaseSimplyStream, ESPMode::ThreadSafe>(this);
	StatsInterface = MakeShared<FOnlineStatsSimplyStream, ESPMode::ThreadSafe>(this);
	AchievementsInterface = MakeShared<FOnlineAchievementsSimplyStream, ESPMode::ThreadSafe>(this);
	LeaderboardsInterface = MakeShared<FOnlineLeaderboardsSimplyStream, ESPMode::ThreadSafe>(this);
	EntitlementsInterface = MakeShared<FOnlineEntitlementsSimplyStream, ESPMode::ThreadSafe>(this);
	ExternalUIInterface = MakeShared<FOnlineExternalUISimplyStream, ESPMode::ThreadSafe>(this);

	SharingInterface = MakeShared<FOnlineSharingSimplyStream, ESPMode::ThreadSafe>(this);
	if (SharingInterface.IsValid() && !SharingInterface->Init())
	{
		UE_LOG(LogOnline, Warning, TEXT("Failed to initialize sharing interface"));
	}
	UserInterface = MakeShared<FOnlineUserInterfaceSimplyStream, ESPMode::ThreadSafe>(this);
	EventsInterface = MakeShared<FOnlineEventsInterfaceSimplyStream, ESPMode::ThreadSafe>(this);
	ChatInterface = MakeShared<FOnlineChatSimplyStream, ESPMode::ThreadSafe>(this);
	if (!ChatInterface->Init())
	{
		UE_LOG(LogOnline, Warning, TEXT("Failed to initialize chat interface"));
	}

#if UE_SERVER
	if (IsRunningDedicatedServer() && IdentityInterface.IsValid())
	{
		IdentityInterface->InitDedicatedServerLogin();
	}
#endif

	return true;
}

bool FOnlineSubsystemSimplyStream::Shutdown()
{
	UE_LOG(LogOnline, Log, TEXT("FOnlineSubsystemSimplyStream::Shutdown()"));

	FOnlineSubsystemImpl::Shutdown();


#define DESTRUCT_INTERFACE(Interface) \
	if (Interface.IsValid()) \
	{ \
		ensure(Interface.IsUnique()); \
		Interface.Reset(); \
	}

	DESTRUCT_INTERFACE(VoiceInterface);
	DESTRUCT_INTERFACE(StatsInterface);
	DESTRUCT_INTERFACE(PurchaseInterface);
	DESTRUCT_INTERFACE(StoreInterface);
	DESTRUCT_INTERFACE(ChatInterface);
	DESTRUCT_INTERFACE(AchievementsInterface);
	DESTRUCT_INTERFACE(LeaderboardsInterface);
	DESTRUCT_INTERFACE(EntitlementsInterface);
	DESTRUCT_INTERFACE(ExternalUIInterface);
	DESTRUCT_INTERFACE(SharingInterface);
	DESTRUCT_INTERFACE(UserInterface);
	DESTRUCT_INTERFACE(EventsInterface);
	DESTRUCT_INTERFACE(PresenceInterface);
	DESTRUCT_INTERFACE(FriendsInterface);
	DESTRUCT_INTERFACE(SessionInterface);
	DESTRUCT_INTERFACE(IdentityInterface);

	return true;
}

bool FOnlineSubsystemSimplyStream::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	if (SessionInterface.IsValid())
	{
		SessionInterface->Tick(DeltaTime);
	}

	if (PresenceInterface.IsValid())
	{
		PresenceInterface->Tick(DeltaTime);
	}

	if (ChatInterface.IsValid())
	{
		ChatInterface->Tick(DeltaTime);
	}

	return true;
}

FString FOnlineSubsystemSimplyStream::GetAppId() const
{
	return TEXT("");
}

bool FOnlineSubsystemSimplyStream::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return false;
}

FString FOnlineSubsystemSimplyStream::GetAPIBaseURL() const
{
	return APIBaseURL;
}
