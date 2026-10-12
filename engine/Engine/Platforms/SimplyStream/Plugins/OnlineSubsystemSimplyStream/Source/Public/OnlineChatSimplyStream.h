// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineChatInterface.h"
#include "OnlineSubsystemSimplyStreamTypes.h"
#include "OnlineSubsystemSimplyStreamPackage.h"

class FOnlineSubsystemSimplyStream;

/**
 * SimplyStream implementation of chat/messaging functionality
 */
class FOnlineChatSimplyStream : public IOnlineChat
{
public:
	/**
	 * Constructor
	 * @param InSubsystem The owner of this chat interface
	 */
	FOnlineChatSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);

	/**
	 * Destructor
	 */
	virtual ~FOnlineChatSimplyStream();

	// IOnlineChat interface
	virtual bool CreateRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nick, const FChatRoomConfig& ChatRoomConfig) override;
	virtual bool ConfigureRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FChatRoomConfig& ChatRoomConfig) override;
	virtual bool JoinPublicRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nick, const FChatRoomConfig& ChatRoomConfig) override;
	virtual bool JoinPrivateRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nick, const FChatRoomConfig& ChatRoomConfig) override;
	virtual bool ExitRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId) override;
	virtual bool SendRoomChat(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& MsgBody) override;
	virtual bool SendPrivateChat(const FUniqueNetId& UserId, const FUniqueNetId& RecipientId, const FString& MsgBody) override;
	virtual bool IsChatAllowed(const FUniqueNetId& UserId, const FUniqueNetId& RecipientId) const override;
	virtual void GetJoinedRooms(const FUniqueNetId& UserId, TArray<FChatRoomId>& OutRooms) override;
	virtual TSharedPtr<FChatRoomInfo> GetRoomInfo(const FUniqueNetId& UserId, const FChatRoomId& RoomId) override;
	virtual bool GetMembers(const FUniqueNetId& UserId, const FChatRoomId& RoomId, TArray<TSharedRef<FChatRoomMember>>& OutMembers) override;
	virtual TSharedPtr<FChatRoomMember> GetMember(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId) override;
	virtual bool GetLastMessages(const FUniqueNetId& UserId, const FChatRoomId& RoomId, int32 NumMessages, TArray<TSharedRef<FChatMessage>>& OutMessages) override;
	virtual bool IsMessageFromLocalUser(const FUniqueNetId& UserId, const FChatMessage& Message, const bool bIncludeExternalInstances) override;
	virtual void DumpChatState() const override;

	/**
	 * SimplyStream-specific methods for retrieving chat history and managing filters
	 */

	/**
	 * Retrieve chat history from the backend
	 * @param UserId The user requesting the history
	 * @param RoomId The room to retrieve history for (or empty for DMs)
	 * @param RecipientId Optional recipient ID for direct messages
	 * @param MaxMessages Maximum number of messages to retrieve
	 */
	void QueryChatHistory(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FUniqueNetIdPtr& RecipientId, int32 MaxMessages = 50);

	/**
	 * Set whether profanity filtering is enabled for a user
	 * @param UserId The user to configure
	 * @param bEnabled Whether filtering should be enabled
	 */
	void SetProfanityFilterEnabled(const FUniqueNetId& UserId, bool bEnabled);

	/**
	 * Check if profanity filtering is enabled for a user
	 * @param UserId The user to check
	 * @return true if filtering is enabled
	 */
	bool IsProfanityFilterEnabled(const FUniqueNetId& UserId) const;

	/**
	 * Initialize the chat interface
	 */
	bool Init();

	/**
	 * Shutdown the chat interface
	 */
	void Shutdown();

	/**
	 * Tick the chat interface to handle polling for new messages
	 */
	void Tick(float DeltaTime);

private:
	/** Reference to the owning subsystem */
	FOnlineSubsystemSimplyStream* Subsystem;

	/** Map of user IDs to rooms they've joined */
	TMap<FUniqueNetIdPtr, TArray<FChatRoomId>> UserRooms;

	/** Map of room IDs to room info */
	TMap<FChatRoomId, TSharedPtr<FChatRoomInfo>> RoomInfoCache;

	/** Map of room IDs to their members */
	TMap<FChatRoomId, TArray<TSharedRef<FChatRoomMember>>> RoomMembersCache;

	/** Map of room IDs to their messages */
	TMap<FChatRoomId, TArray<TSharedRef<FChatMessage>>> RoomMessagesCache;

	/** Map of user IDs to their direct message conversations */
	TMap<FUniqueNetIdPtr, TMap<FUniqueNetIdPtr, TArray<TSharedRef<FChatMessage>>>> DirectMessagesCache;

	/** Users with profanity filtering enabled */
	TSet<FUniqueNetIdPtr> ProfanityFilteredUsers;

	/** Critical section for thread safety */
	mutable FCriticalSection ChatLock;

	/** Time since last message poll */
	float TimeSinceLastPoll;

	/** Interval between message polls (in seconds) */
	float MessagePollInterval;

	/**
	 * HTTP completion handlers
	 */
	void OnCreateRoomComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FChatRoomId RoomId);
	void OnJoinRoomComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FChatRoomId RoomId, FString Nick);
	void OnExitRoomComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FChatRoomId RoomId);
	void OnSendRoomChatComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FChatRoomId RoomId, FString Message);
	void OnSendPrivateChatComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef SenderId, FUniqueNetIdRef RecipientId, FString Message);
	void OnQueryChatHistoryComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FChatRoomId RoomId);
	void OnPollMessagesComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId);

	/**
	 * Helper methods
	 */
	FString GetAPIBaseURL() const;
	FString GetAuthToken(const FUniqueNetId& UserId) const;
	FString FilterProfanity(const FString& Message) const;
	bool ContainsProfanity(const FString& Message) const;
	void ProcessIncomingMessage(const FUniqueNetId& UserId, const TSharedPtr<FJsonObject>& MessageJson);
	void PollForNewMessages(const FUniqueNetId& UserId);

	/** Profanity word list (basic implementation) */
	static TSet<FString> ProfanityWords;
	static void InitializeProfanityFilter();
};

typedef TSharedPtr<FOnlineChatSimplyStream, ESPMode::ThreadSafe> FOnlineChatSimplyStreamPtr;
