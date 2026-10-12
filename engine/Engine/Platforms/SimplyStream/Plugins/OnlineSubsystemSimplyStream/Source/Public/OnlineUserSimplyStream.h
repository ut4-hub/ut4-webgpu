// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineUserInterface.h"
#include "OnlineSubsystemSimplyStreamPackage.h"
#include "OnlineSubsystemSimplyStreamTypes.h"
#include "Http.h"

class FOnlineSubsystemSimplyStream;

/**
 * User info data class for SimplyStream platform
 */
class FOnlineUserSimplyStream : public FOnlineUser
{
public:
	FOnlineUserSimplyStream(const FString& InUserId, const FString& InUsername, const FString& InDisplayName, const FString& InPlatform, const FString& InAvatarUrl, bool bInIsGuest);

	virtual FUniqueNetIdRef GetUserId() const override;
	virtual FString GetRealName() const override;
	virtual FString GetDisplayName(const FString& InPlatform = FString()) const override;
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;
	virtual bool SetUserLocalAttribute(const FString& AttrName, const FString& InAttrValue) override;

private:
	FUniqueNetIdPtr UserId;
	FString Username;
	FString DisplayName;
	FString Platform;
	FString AvatarUrl;
	bool bIsGuest;
	TMap<FString, FString> UserAttributes;
};

/**
 * User interface for SimplyStream platform
 * Provides user profile querying and caching
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlineUserInterfaceSimplyStream : public IOnlineUser, public TSharedFromThis<FOnlineUserInterfaceSimplyStream, ESPMode::ThreadSafe>
{
public:
	FOnlineUserInterfaceSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
	virtual ~FOnlineUserInterfaceSimplyStream() = default;

	// IOnlineUser interface
	virtual bool QueryUserInfo(int32 LocalUserNum, const TArray<FUniqueNetIdRef>& UserIds) override;
	virtual bool GetAllUserInfo(int32 LocalUserNum, TArray<TSharedRef<FOnlineUser>>& OutUsers) override;
	virtual TSharedPtr<FOnlineUser> GetUserInfo(int32 LocalUserNum, const FUniqueNetId& UserId) override;
	virtual bool QueryUserIdMapping(const FUniqueNetId& UserId, const FString& DisplayNameOrEmail, const FOnQueryUserMappingComplete& Delegate = FOnQueryUserMappingComplete()) override;
	virtual bool QueryExternalIdMappings(const FUniqueNetId& UserId, const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, const FOnQueryExternalIdMappingsComplete& Delegate = FOnQueryExternalIdMappingsComplete()) override;
	virtual void GetExternalIdMappings(const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, TArray<FUniqueNetIdPtr>& OutIds) override;
	virtual FUniqueNetIdPtr GetExternalIdMapping(const FExternalIdQueryOptions& QueryOptions, const FString& ExternalId) override;

private:
	/** Reference to the main subsystem */
	FOnlineSubsystemSimplyStream* Subsystem;

	/** Cached user info per local user */
	TMap<int32, TUniqueNetIdMap<TSharedRef<FOnlineUser>>> CachedUsers;

	/** Critical section for thread-safe cache access */
	mutable FCriticalSection CachedUsersLock;

	/** HTTP completion handlers */
	void OnQueryUserInfoComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, TArray<FUniqueNetIdRef> UserIds);
	void OnQueryUserIdMappingComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef RequestingUserId, FString SearchQuery, FOnQueryUserMappingComplete Delegate);

	/** Helper methods */
	FString GetAPIBaseURL() const;
	FString GetJWTToken(int32 LocalUserNum) const;
	FUniqueNetIdPtr GetUniquePlayerId(int32 LocalUserNum) const;
};

typedef TSharedPtr<FOnlineUserInterfaceSimplyStream, ESPMode::ThreadSafe> FOnlineUserInterfaceSimplyStreamPtr;
