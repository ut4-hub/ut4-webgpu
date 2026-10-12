// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineEntitlementsInterface.h"
#include "OnlineSubsystemSimplyStreamTypes.h"
#include "Http.h"

class FOnlineSubsystemSimplyStream;

/**
 * Entitlements interface for SimplyStream platform
 * Maps inventory items to Unreal Engine entitlements format
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlineEntitlementsSimplyStream : public IOnlineEntitlements, public TSharedFromThis<FOnlineEntitlementsSimplyStream, ESPMode::ThreadSafe>
{
public:
	FOnlineEntitlementsSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
	virtual ~FOnlineEntitlementsSimplyStream() = default;

	// IOnlineEntitlements interface
	virtual TSharedPtr<FOnlineEntitlement> GetEntitlement(const FUniqueNetId& UserId, const FUniqueEntitlementId& EntitlementId) override;
	virtual TSharedPtr<FOnlineEntitlement> GetItemEntitlement(const FUniqueNetId& UserId, const FString& ItemId) override;
	virtual void GetAllEntitlements(const FUniqueNetId& UserId, const FString& Namespace, TArray<TSharedRef<FOnlineEntitlement>>& OutUserEntitlements) override;
	virtual bool QueryEntitlements(const FUniqueNetId& UserId, const FString& Namespace, const FPagedQuery& Page = FPagedQuery()) override;

private:
	/** Reference to the main subsystem */
	FOnlineSubsystemSimplyStream* Subsystem;

	/** Cached entitlements per user */
	TMap<FUniqueNetIdRef, TArray<TSharedRef<FOnlineEntitlement>>> CachedEntitlements;

	/** Critical section for thread-safe access to cached entitlements */
	mutable FCriticalSection EntitlementsCacheLock;

	/** HTTP completion handler */
	void OnQueryEntitlementsComplete(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		FUniqueNetIdRef UserId,
		FString Namespace,
		FOnQueryEntitlementsCompleteDelegate Delegate);

	/** Helper method to create entitlement from JSON */
	TSharedRef<FOnlineEntitlement> CreateEntitlementFromJson(const TSharedPtr<FJsonObject>& JsonEntitlement) const;

	/** Helper method to cache entitlements for a user */
	void CacheEntitlements(const FUniqueNetIdRef& UserId, const TArray<TSharedRef<FOnlineEntitlement>>& Entitlements);
};

typedef TSharedPtr<FOnlineEntitlementsSimplyStream, ESPMode::ThreadSafe> FOnlineEntitlementsSimplyStreamPtr;
