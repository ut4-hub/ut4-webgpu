// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemSimplyStreamPackage.h"

/** Forward declarations of all interface classes */
typedef TSharedPtr<class FOnlineSessionSimplyStream, ESPMode::ThreadSafe> FOnlineSessionSimplyStreamPtr;
typedef TSharedPtr<class FOnlineIdentitySimplyStream, ESPMode::ThreadSafe> FOnlineIdentitySimplyStreamPtr;
typedef TSharedPtr<class FOnlineFriendsSimplyStream, ESPMode::ThreadSafe> FOnlineFriendsSimplyStreamPtr;
typedef TSharedPtr<class FOnlinePresenceSimplyStream, ESPMode::ThreadSafe> FOnlinePresenceSimplyStreamPtr;
typedef TSharedPtr<class FOnlineVoiceImplSimplyStream, ESPMode::ThreadSafe> FOnlineVoiceImplSimplyStreamPtr;
typedef TSharedPtr<class FOnlineStoreV2SimplyStream, ESPMode::ThreadSafe> FOnlineStoreV2SimplyStreamPtr;
typedef TSharedPtr<class FOnlinePurchaseSimplyStream, ESPMode::ThreadSafe> FOnlinePurchaseSimplyStreamPtr;
typedef TSharedPtr<class FOnlineStatsSimplyStream, ESPMode::ThreadSafe> FOnlineStatsSimplyStreamPtr;
typedef TSharedPtr<class FOnlineChatSimplyStream, ESPMode::ThreadSafe> FOnlineChatSimplyStreamPtr;
typedef TSharedPtr<class FOnlineAchievementsSimplyStream, ESPMode::ThreadSafe> FOnlineAchievementsSimplyStreamPtr;
typedef TSharedPtr<class FOnlineLeaderboardsSimplyStream, ESPMode::ThreadSafe> FOnlineLeaderboardsSimplyStreamPtr;
typedef TSharedPtr<class FOnlineEntitlementsSimplyStream, ESPMode::ThreadSafe> FOnlineEntitlementsSimplyStreamPtr;
typedef TSharedPtr<class FOnlineExternalUISimplyStream, ESPMode::ThreadSafe> FOnlineExternalUISimplyStreamPtr;
typedef TSharedPtr<class FOnlineSharingSimplyStream, ESPMode::ThreadSafe> FOnlineSharingSimplyStreamPtr;
typedef TSharedPtr<class FOnlineUserInterfaceSimplyStream, ESPMode::ThreadSafe> FOnlineUserInterfaceSimplyStreamPtr;
typedef TSharedPtr<class FOnlineEventsInterfaceSimplyStream, ESPMode::ThreadSafe> FOnlineEventsInterfaceSimplyStreamPtr;

/**
 * SimplyStream backend services
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlineSubsystemSimplyStream
	: public FOnlineSubsystemImpl
{
public:
	// IOnlineSubsystem
	virtual IOnlineSessionPtr GetSessionInterface() const override;
	virtual IOnlineFriendsPtr GetFriendsInterface() const override;
	virtual IOnlinePartyPtr GetPartyInterface() const override;
	virtual IOnlineGroupsPtr GetGroupsInterface() const override;
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const override;
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const override;
	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const override;
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override;
	virtual IOnlineVoicePtr GetVoiceInterface() const override;
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const override;
	virtual IOnlineTimePtr GetTimeInterface() const override;
	virtual IOnlineIdentityPtr GetIdentityInterface() const override;
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const override;
	virtual IOnlineStoreV2Ptr GetStoreV2Interface() const override;
	virtual IOnlinePurchasePtr GetPurchaseInterface() const override;
	virtual IOnlineEventsPtr GetEventsInterface() const override;
	virtual IOnlineAchievementsPtr GetAchievementsInterface() const override;
	virtual IOnlineSharingPtr GetSharingInterface() const override;
	virtual IOnlineUserPtr GetUserInterface() const override;
	virtual IOnlineMessagePtr GetMessageInterface() const override;
	virtual IOnlinePresencePtr GetPresenceInterface() const override;
	virtual IOnlineChatPtr GetChatInterface() const override;
	virtual IOnlineStatsPtr GetStatsInterface() const override;
	virtual IOnlineTurnBasedPtr GetTurnBasedInterface() const override;
	virtual IOnlineTournamentPtr GetTournamentInterface() const override;
	virtual IMessageSanitizerPtr GetMessageSanitizer(int32 LocalUserNum, FString& OutAuthTypeToExclude) const override;
	virtual FText GetOnlineServiceName() const override;

	virtual bool Init() override;
	virtual bool Shutdown() override;
	virtual FString GetAppId() const override;
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	FString GetAPIBaseURL() const;

	// FTSTickerObjectBase
	virtual bool Tick(float DeltaTime) override;

	// FOnlineSubsystemSimplyStream
	virtual ~FOnlineSubsystemSimplyStream();

PACKAGE_SCOPE:
	/** Only the factory makes instances */
	FOnlineSubsystemSimplyStream() = delete;
	explicit FOnlineSubsystemSimplyStream(FName InInstanceName);

private:
	/** Interface to the identity registration/auth services */
	FOnlineIdentitySimplyStreamPtr IdentityInterface;

	/** Interface to the session services */
	FOnlineSessionSimplyStreamPtr SessionInterface;

	/** Interface to the friends services */
	FOnlineFriendsSimplyStreamPtr FriendsInterface;

	/** Interface to the presence services */
	FOnlinePresenceSimplyStreamPtr PresenceInterface;

	/** Interface to the voice services */
	FOnlineVoiceImplSimplyStreamPtr VoiceInterface;

	/** Interface to the store services */
	FOnlineStoreV2SimplyStreamPtr StoreInterface;

	/** Interface to the purchase services */
	FOnlinePurchaseSimplyStreamPtr PurchaseInterface;

	/** Interface to the stats services */
	FOnlineStatsSimplyStreamPtr StatsInterface;

	/** Interface to the chat services */
	FOnlineChatSimplyStreamPtr ChatInterface;

	/** Interface to the achievements services */
	FOnlineAchievementsSimplyStreamPtr AchievementsInterface;

	/** Interface to the leaderboards services */
	FOnlineLeaderboardsSimplyStreamPtr LeaderboardsInterface;

	/** Interface to the entitlements services */
	FOnlineEntitlementsSimplyStreamPtr EntitlementsInterface;

	/** Interface to the external UI services */
	FOnlineExternalUISimplyStreamPtr ExternalUIInterface;

	/** Interface to the sharing services */
	FOnlineSharingSimplyStreamPtr SharingInterface;

	/** Interface to the user services */
	FOnlineUserInterfaceSimplyStreamPtr UserInterface;

	/** Interface to the events services */
	FOnlineEventsInterfaceSimplyStreamPtr EventsInterface;

	/** Base URL for SimplyStream backend requests */
	FString APIBaseURL;
};

typedef TSharedPtr<FOnlineSubsystemSimplyStream, ESPMode::ThreadSafe> FOnlineSubsystemSimplyStreamPtr;
