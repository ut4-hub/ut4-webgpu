// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystemSimplyStreamPackage.h"

class IHttpRequest;

class FOnlineSubsystemSimplyStream;

/**
 * SimplyStream service implementation of the online identity interface
 */
class FOnlineIdentitySimplyStream : public IOnlineIdentity
{
public:

	// IOnlineIdentity
	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;
	virtual bool Logout(int32 LocalUserNum) override;
	virtual bool AutoLogin(int32 LocalUserNum) override;
	virtual TSharedPtr<FUserOnlineAccount> GetUserAccount(const FUniqueNetId& UserId) const override;
	virtual TArray<TSharedPtr<FUserOnlineAccount> > GetAllUserAccounts() const override;
	virtual FUniqueNetIdPtr GetUniquePlayerId(int32 LocalUserNum) const override;
	virtual FUniqueNetIdPtr CreateUniquePlayerId(uint8* Bytes, int32 Size) override;
	virtual FUniqueNetIdPtr CreateUniquePlayerId(const FString& Str) override;
	virtual ELoginStatus::Type GetLoginStatus(int32 LocalUserNum) const override;
	virtual ELoginStatus::Type GetLoginStatus(const FUniqueNetId& UserId) const override;
	virtual FString GetPlayerNickname(int32 LocalUserNum) const override;
	virtual FString GetPlayerNickname(const FUniqueNetId& UserId) const override;
	virtual FString GetAuthToken(int32 LocalUserNum) const override;
	virtual void GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate, EShowPrivilegeResolveUI ShowResolveUI=EShowPrivilegeResolveUI::Default) override;
	virtual FPlatformUserId GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const override;
	virtual FString GetAuthType() const override;
	virtual void RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate) override;

#if UE_SERVER
	/** Attempt to automatically authenticate a dedicated server using environment credentials */
	void InitDedicatedServerLogin();
#endif

PACKAGE_SCOPE:
	/**
	 * Constructor
	 * @param InSubsystem The owner of this identity interface.
	 */
	FOnlineIdentitySimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);

	/**
	 * Destructor
	 */
	virtual ~FOnlineIdentitySimplyStream() = default;

private:
	/**
	 * Must use the initialization constructor instead
	 */
	FOnlineIdentitySimplyStream() = delete;
	
	/** Users that have been registered */
	TMap<int32, TSharedRef<FUserOnlineAccount>> UserAccounts;
	/** Ids mapped to locally registered users */
	TMap<int32, FUniqueNetIdPtr> UserIds;
	/** Local users with a login HTTP request in flight. A second Login/AutoLogin while one is pending
	 *  coalesces into it so the identity layer emits only one completion for one state change. */
	TSet<int32> LoginsInFlight;

	/** Reference to the main subsystem */
	FOnlineSubsystemSimplyStream* Subsystem;

#if UE_SERVER
	bool bDedicatedServerAuthInProgress = false;
	bool bDedicatedServerAuthSucceeded = false;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> DedicatedServerLoginRequest;
#endif
};

typedef TSharedPtr<FOnlineIdentitySimplyStream, ESPMode::ThreadSafe> FOnlineIdentitySimplyStreamPtr;
