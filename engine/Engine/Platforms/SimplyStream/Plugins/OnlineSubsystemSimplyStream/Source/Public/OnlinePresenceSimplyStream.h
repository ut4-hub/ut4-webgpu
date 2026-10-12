// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/OnlinePresenceInterface.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "OnlineSubsystemSimplyStreamPackage.h"
#include "OnlineSubsystemSimplyStreamTypes.h"
#include "HAL/CriticalSection.h"

class FJsonObject;

class FOnlineSubsystemSimplyStream;

/**
 * SimplyStream service implementation of the online presence interface
 */
class FOnlinePresenceSimplyStream : public IOnlinePresence
{
public:
	// IOnlinePresence
	virtual void SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) override;
	virtual void QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) override;
	virtual EOnlineCachedResult::Type GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence) override;
	virtual EOnlineCachedResult::Type GetCachedPresenceForApp(const FUniqueNetId& LocalUserId, const FUniqueNetId& User, const FString& AppId, TSharedPtr<FOnlineUserPresence>& OutPresence) override;

	/** Constructor */
	FOnlinePresenceSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
	/** Destructor */
	virtual ~FOnlinePresenceSimplyStream() = default;

	/** Subsystem tick hook */
	void Tick(float DeltaTime);

	/** Register/unregister remote users for automatic refresh */
	void TrackRemoteUser(const FUniqueNetId& UserId);
	void UntrackRemoteUser(const FUniqueNetId& UserId);

private:
	/** Reference to the main SimplyStream subsystem */
	FOnlineSubsystemSimplyStream* Subsystem;
	/** Cached presence information */
	TUniqueNetIdMap<TSharedRef<FOnlineUserPresence>> PresenceCache;
	/** Protects PresenceCache for multi-threaded access */
	mutable FCriticalSection PresenceLock;
	/** Protects auto-refresh subscriptions */
	mutable FCriticalSection SubscriptionLock;

	struct FPresenceAutoRefreshEntry
	{
		FUniqueNetIdSimplyStreamPtr UserId;
		double NextRefreshTime = 0.0;
	};

	TMap<FString, FPresenceAutoRefreshEntry> AutoRefreshEntries;
	float AutoRefreshIntervalSeconds = 30.0f;
	float AutoRefreshRetrySeconds = 10.0f;
	int32 AutoRefreshBatchSize = 25;

	// Helpers
	FString ConvertPresenceStateToBackend(EOnlinePresenceState::Type State) const;
	EOnlinePresenceState::Type ConvertBackendStateToPresence(const FString& State) const;
	FOnlineUserPresenceStatus BuildPresenceStatusFromJson(const TSharedPtr<FJsonObject>& JsonObject) const;
	TSharedPtr<FOnlineUserPresence> UpdatePresenceCache(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, bool bIsJoinable, const FString& LastHeartbeat);
	int32 ResolveLocalUserNumForRequest(const IOnlineIdentityPtr& IdentityInterface, const FUniqueNetId& TargetUser) const;
	int32 GetDefaultLocalUserNum(const IOnlineIdentityPtr& IdentityInterface) const;
	bool SendAutoPresenceQuery(const TArray<FUniqueNetIdSimplyStreamPtr>& Users);
	void RescheduleTrackedUsers(const TArray<FString>& UserIds, double DelaySeconds);
	void HandleSetPresenceResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, TSharedRef<const FUniqueNetId> UserRef, FOnPresenceTaskCompleteDelegate Delegate);
	void HandleQueryPresenceResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, TSharedRef<const FUniqueNetId> TargetUserRef, FOnPresenceTaskCompleteDelegate Delegate);
	void HandleAutoQueryPresenceResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, TArray<FString> RequestedUserIds);
};

typedef TSharedPtr<FOnlinePresenceSimplyStream, ESPMode::ThreadSafe> FOnlinePresenceSimplyStreamPtr;
