// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "OnlineSubsystemSimplyStreamPackage.h"
#include "OnlineSubsystemSimplyStreamTypes.h"
#include "Http.h"
#include "Misc/Optional.h"

struct FSimplyStreamFriendsQueryOptions
{
	TOptional<FString> Status;
	TOptional<FString> Search;
	TOptional<bool> IncludePresence;
	TOptional<int32> Limit;
};

class FOnlineSubsystemSimplyStream;

class FOnlineFriendSimplyStream : public FOnlineFriend
{
public:
	FOnlineFriendSimplyStream(const FString& InFriendshipId, const FString& InUserId, const FString& InUsername, const FString& InDisplayName, const FString& InPlatform, const FString& InAvatarUrl);

	virtual FUniqueNetIdRef GetUserId() const override;
	virtual FString GetRealName() const override;
	virtual FString GetDisplayName(const FString& InPlatform) const override;
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;
	virtual EInviteStatus::Type GetInviteStatus() const override;
	virtual const FOnlineUserPresence& GetPresence() const override;

	void SetInviteStatus(EInviteStatus::Type NewStatus);
	void SetPresence(const FOnlineUserPresence& NewPresence);

private:
	FString FriendshipId;
	FUniqueNetIdPtr UserId;
	FString Username;
	FString DisplayName;
	FString Platform;
	FString AvatarUrl;
	EInviteStatus::Type InviteStatus = EInviteStatus::Unknown;
	FOnlineUserPresence Presence;
};

class FOnlineBlockedPlayerSimplyStream : public FOnlineBlockedPlayer
{
public:
	FOnlineBlockedPlayerSimplyStream(const FString& InUserId, const FString& InUsername, const FString& InDisplayName);

	virtual FUniqueNetIdRef GetUserId() const override;
	virtual FString GetRealName() const override;
	virtual FString GetDisplayName(const FString& InPlatform) const override;
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;

private:
	FUniqueNetIdPtr UserId;
	FString Username;
	FString DisplayName;
};

class FOnlineRecentPlayerSimplyStream : public FOnlineRecentPlayer
{
public:
	FOnlineRecentPlayerSimplyStream(const FString& InUserId, const FString& InUsername, const FString& InDisplayName, const FString& InPlatform, const FDateTime& InLastSeen, const FString& InPresenceStr, ERecentPlayerEncounterType InEncounterType);

	virtual FUniqueNetIdRef GetUserId() const override { check(UserId.IsValid()); return UserId.ToSharedRef(); }
	virtual FString GetRealName() const override { return DisplayName; }
	virtual FString GetDisplayName(const FString& InPlatform) const override { return DisplayName; }
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;
	virtual FDateTime GetLastSeen() const override { return LastSeen; }

	TSharedRef<FOnlineUserPresence> GetPresence() const { return Presence; }

private:
	FUniqueNetIdPtr UserId;
	FString Username;
	FString DisplayName;
	FString Platform;
	FString PresenceStr;
	ERecentPlayerEncounterType EncounterType;
	FDateTime LastSeen;
	TSharedRef<FOnlineUserPresence> Presence;
};

class FOnlineFriendsSimplyStream : public IOnlineFriends, public TSharedFromThis<FOnlineFriendsSimplyStream, ESPMode::ThreadSafe>
{
public:
	FOnlineFriendsSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
	virtual ~FOnlineFriendsSimplyStream();

	// IOnlineFriends overrides
	virtual bool ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate = FOnReadFriendsListComplete()) override;
	bool ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FSimplyStreamFriendsQueryOptions& Options, const FOnReadFriendsListComplete& Delegate = FOnReadFriendsListComplete());
	virtual bool DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate = FOnDeleteFriendsListComplete()) override;
	virtual bool SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate = FOnSendInviteComplete()) override;
	virtual bool AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate = FOnAcceptInviteComplete()) override;
	virtual bool RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray<TSharedRef<FOnlineFriend>>& OutFriends) override;
	virtual TSharedPtr<FOnlineFriend> GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual void AddRecentPlayers(const FUniqueNetId& UserId, const TArray<FReportPlayedWithUser>& InRecentPlayers, const FString& Namespace, const FOnAddRecentPlayersComplete& Delegate = FOnAddRecentPlayersComplete()) override;
	virtual bool QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace) override;
	virtual bool GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray<TSharedRef<FOnlineRecentPlayer>>& OutRecentPlayers) override;
	virtual void DumpRecentPlayers() const override;
	virtual bool BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool QueryBlockedPlayers(const FUniqueNetId& UserId) override;
	virtual bool GetBlockedPlayers(const FUniqueNetId& UserId, TArray<TSharedRef<FOnlineBlockedPlayer>>& OutBlockedPlayers) override;
	bool IsPlayerBlocked(const FUniqueNetId& LocalUserId, const FUniqueNetId& PeerUserId) const;
	virtual void SetFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FString& Alias, const FOnSetFriendAliasComplete& Delegate = FOnSetFriendAliasComplete()) override;
	virtual void DeleteFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnDeleteFriendAliasComplete& Delegate = FOnDeleteFriendAliasComplete()) override;
	virtual void DumpBlockedPlayers() const override;

	bool Init();

private:
	void OnLoginChanged(int32 LocalUserNum);
	void OnReadFriendsListComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FString ListName, FString LocalUserIdStr, FOnReadFriendsListComplete Delegate);
	void OnSendInviteComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef FriendId, FString ListName, FOnSendInviteComplete Delegate);
	void OnAcceptInviteComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef FriendId, FString ListName, FOnAcceptInviteComplete Delegate);
	void OnRejectInviteComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef FriendId, FString ListName);
	void OnDeleteFriendComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef FriendId, FString ListName);
	void OnBlockPlayerComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef PlayerId);
	void OnUnblockPlayerComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef PlayerId);
	void OnQueryBlockedPlayersComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef UserId);
	void OnAddRecentPlayersComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString Namespace, FOnAddRecentPlayersComplete Delegate);
	void OnQueryRecentPlayersComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef UserId, FString Namespace);
	void RegisterFriendsForPresence(int32 LocalUserNum);
	void UnregisterFriendsForPresence(int32 LocalUserNum);

	FOnlineSubsystemSimplyStream* Subsystem;
	TMap<int32, TMap<FString, TArray<TSharedRef<FOnlineFriend>>>> FriendsLists;
	TMap<int32, TArray<TSharedRef<FOnlineBlockedPlayer>>> BlockedPlayers;
	TMap<int32, TMap<FString, TArray<TSharedRef<FOnlineRecentPlayer>>>> RecentPlayers;
	FDelegateHandle OnLoginChangedHandle;
};

typedef TSharedPtr<FOnlineFriendsSimplyStream, ESPMode::ThreadSafe> FOnlineFriendsSimplyStreamPtr;
