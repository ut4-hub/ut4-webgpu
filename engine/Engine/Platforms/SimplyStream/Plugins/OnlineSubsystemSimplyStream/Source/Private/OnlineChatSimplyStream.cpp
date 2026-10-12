// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineChatSimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineIdentitySimplyStream.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#ifndef SIMPLYSTREAM_ENABLE_CHAT
#define SIMPLYSTREAM_ENABLE_CHAT 0
#endif

#if SIMPLYSTREAM_ENABLE_CHAT

// Static profanity filter initialization
TSet<FString> FOnlineChatSimplyStream::ProfanityWords;

void FOnlineChatSimplyStream::InitializeProfanityFilter()
{
	if (ProfanityWords.Num() == 0)
	{
		// Basic profanity word list (extend as needed)
		// In production, this should be loaded from a configuration file or backend service
		ProfanityWords.Add(TEXT("badword1"));
		ProfanityWords.Add(TEXT("badword2"));
		ProfanityWords.Add(TEXT("offensive"));
		// Add more words as needed
	}
}

FOnlineChatSimplyStream::FOnlineChatSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
	: Subsystem(InSubsystem)
	, TimeSinceLastPoll(0.0f)
	, MessagePollInterval(5.0f) // Poll every 5 seconds
{
	InitializeProfanityFilter();
}

FOnlineChatSimplyStream::~FOnlineChatSimplyStream()
{
	Shutdown();
}

bool FOnlineChatSimplyStream::Init()
{
	UE_LOG_ONLINE_CHAT(Log, TEXT("SimplyStream chat interface initialized"));
	return true;
}

void FOnlineChatSimplyStream::Shutdown()
{
	FScopeLock ScopeLock(&ChatLock);

	UserRooms.Empty();
	RoomInfoCache.Empty();
	RoomMembersCache.Empty();
	RoomMessagesCache.Empty();
	DirectMessagesCache.Empty();
	ProfanityFilteredUsers.Empty();

	UE_LOG_ONLINE_CHAT(Log, TEXT("SimplyStream chat interface shutdown"));
}

void FOnlineChatSimplyStream::Tick(float DeltaTime)
{
	TimeSinceLastPoll += DeltaTime;

	// Poll for new messages at regular intervals for all logged-in users
	if (TimeSinceLastPoll >= MessagePollInterval)
	{
		TimeSinceLastPoll = 0.0f;

		IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
		if (IdentityInt.IsValid())
		{
			// Poll messages for each logged-in user
			for (int32 LocalUserNum = 0; LocalUserNum < MAX_LOCAL_PLAYERS; ++LocalUserNum)
			{
				if (IdentityInt->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
				{
					FUniqueNetIdPtr UserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
					if (UserId.IsValid())
					{
						PollForNewMessages(*UserId);
					}
				}
			}
		}
	}
}

bool FOnlineChatSimplyStream::CreateRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nick, const FChatRoomConfig& ChatRoomConfig)
{
	FString AuthToken = GetAuthToken(UserId);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_CHAT(Warning, TEXT("CreateRoom: No auth token for user %s"), *UserId.ToDebugString());
		TriggerOnChatRoomCreatedDelegates(UserId, RoomId, false, TEXT("No auth token"));
		return false;
	}

	// Build room creation request
	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	RequestJson->SetStringField(TEXT("roomId"), RoomId);
	RequestJson->SetStringField(TEXT("roomName"), ChatRoomConfig.RoomName);
	RequestJson->SetStringField(TEXT("roomType"), ChatRoomConfig.bIsPrivate ? TEXT("private") : TEXT("public"));
	RequestJson->SetStringField(TEXT("nickname"), Nick);
	RequestJson->SetNumberField(TEXT("maxMembers"), ChatRoomConfig.MaxMembers);
	RequestJson->SetBoolField(TEXT("allowInvites"), true);
	RequestJson->SetStringField(TEXT("password"), ChatRoomConfig.Password);

	FString RequestContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/chat/rooms/create"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestContent);

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineChatSimplyStream::OnCreateRoomComplete,
		UserId.AsShared(), RoomId);

	return HttpRequest->ProcessRequest();
}

bool FOnlineChatSimplyStream::ConfigureRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FChatRoomConfig& ChatRoomConfig)
{
	// TODO: Implement room configuration endpoint
	UE_LOG_ONLINE_CHAT(Warning, TEXT("ConfigureRoom not yet implemented"));
	TriggerOnChatRoomConfiguredDelegates(UserId, RoomId, false, TEXT("Not implemented"));
	return false;
}

bool FOnlineChatSimplyStream::JoinPublicRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nick, const FChatRoomConfig& ChatRoomConfig)
{
	FString AuthToken = GetAuthToken(UserId);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_CHAT(Warning, TEXT("JoinPublicRoom: No auth token for user %s"), *UserId.ToDebugString());
		TriggerOnChatRoomJoinPublicDelegates(UserId, RoomId, false, TEXT("No auth token"));
		return false;
	}

	// Build room join request
	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	RequestJson->SetStringField(TEXT("roomId"), RoomId);
	RequestJson->SetStringField(TEXT("nickname"), Nick);
	RequestJson->SetStringField(TEXT("roomType"), TEXT("public"));

	FString RequestContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/chat/rooms/join"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestContent);

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineChatSimplyStream::OnJoinRoomComplete,
		UserId.AsShared(), RoomId, Nick);

	return HttpRequest->ProcessRequest();
}

bool FOnlineChatSimplyStream::JoinPrivateRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nick, const FChatRoomConfig& ChatRoomConfig)
{
	FString AuthToken = GetAuthToken(UserId);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_CHAT(Warning, TEXT("JoinPrivateRoom: No auth token for user %s"), *UserId.ToDebugString());
		TriggerOnChatRoomJoinPrivateDelegates(UserId, RoomId, false, TEXT("No auth token"));
		return false;
	}

	// Build room join request
	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	RequestJson->SetStringField(TEXT("roomId"), RoomId);
	RequestJson->SetStringField(TEXT("nickname"), Nick);
	RequestJson->SetStringField(TEXT("roomType"), TEXT("private"));
	RequestJson->SetStringField(TEXT("password"), ChatRoomConfig.Password);

	FString RequestContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/chat/rooms/join"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestContent);

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineChatSimplyStream::OnJoinRoomComplete,
		UserId.AsShared(), RoomId, Nick);

	return HttpRequest->ProcessRequest();
}

bool FOnlineChatSimplyStream::ExitRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId)
{
	FString AuthToken = GetAuthToken(UserId);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_CHAT(Warning, TEXT("ExitRoom: No auth token for user %s"), *UserId.ToDebugString());
		TriggerOnChatRoomExitDelegates(UserId, RoomId, false, TEXT("No auth token"));
		return false;
	}

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/chat/rooms/%s/leave"), *RoomId));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineChatSimplyStream::OnExitRoomComplete,
		UserId.AsShared(), RoomId);

	return HttpRequest->ProcessRequest();
}

bool FOnlineChatSimplyStream::SendRoomChat(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& MsgBody)
{
	FString AuthToken = GetAuthToken(UserId);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_CHAT(Warning, TEXT("SendRoomChat: No auth token for user %s"), *UserId.ToDebugString());
		TriggerOnChatRoomMessageSentDelegates(UserId, RoomId, false, TEXT("No auth token"));
		return false;
	}

	// Apply profanity filter if enabled
	FString FilteredMessage = MsgBody;
	if (IsProfanityFilterEnabled(UserId))
	{
		FilteredMessage = FilterProfanity(MsgBody);
		if (ContainsProfanity(MsgBody))
		{
			UE_LOG_ONLINE_CHAT(Log, TEXT("Message filtered for profanity: %s -> %s"), *MsgBody, *FilteredMessage);
		}
	}

	// Build message send request
	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	RequestJson->SetStringField(TEXT("roomId"), RoomId);
	RequestJson->SetStringField(TEXT("message"), FilteredMessage);
	RequestJson->SetStringField(TEXT("messageType"), TEXT("text"));

	FString RequestContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/chat/rooms/message"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestContent);

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineChatSimplyStream::OnSendRoomChatComplete,
		UserId.AsShared(), RoomId, FilteredMessage);

	return HttpRequest->ProcessRequest();
}

bool FOnlineChatSimplyStream::SendPrivateChat(const FUniqueNetId& UserId, const FUniqueNetId& RecipientId, const FString& MsgBody)
{
	FString AuthToken = GetAuthToken(UserId);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_CHAT(Warning, TEXT("SendPrivateChat: No auth token for user %s"), *UserId.ToDebugString());
		return false;
	}

	// Apply profanity filter if enabled
	FString FilteredMessage = MsgBody;
	if (IsProfanityFilterEnabled(UserId))
	{
		FilteredMessage = FilterProfanity(MsgBody);
		if (ContainsProfanity(MsgBody))
		{
			UE_LOG_ONLINE_CHAT(Log, TEXT("Private message filtered for profanity"));
		}
	}

	// Build DM send request
	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	RequestJson->SetStringField(TEXT("recipientId"), RecipientId.ToString());
	RequestJson->SetStringField(TEXT("message"), FilteredMessage);
	RequestJson->SetStringField(TEXT("messageType"), TEXT("text"));

	FString RequestContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/chat/direct/send"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestContent);

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineChatSimplyStream::OnSendPrivateChatComplete,
		UserId.AsShared(), RecipientId.AsShared(), FilteredMessage);

	return HttpRequest->ProcessRequest();
}

bool FOnlineChatSimplyStream::IsChatAllowed(const FUniqueNetId& UserId, const FUniqueNetId& RecipientId) const
{
	// TODO: Check if the recipient has blocked the sender
	// For now, allow all chat
	return true;
}

void FOnlineChatSimplyStream::GetJoinedRooms(const FUniqueNetId& UserId, TArray<FChatRoomId>& OutRooms)
{
	FScopeLock ScopeLock(&ChatLock);

	FUniqueNetIdPtr UserIdPtr = UserId.AsShared();
	if (const TArray<FChatRoomId>* Rooms = UserRooms.Find(UserIdPtr))
	{
		OutRooms = *Rooms;
	}
}

TSharedPtr<FChatRoomInfo> FOnlineChatSimplyStream::GetRoomInfo(const FUniqueNetId& UserId, const FChatRoomId& RoomId)
{
	FScopeLock ScopeLock(&ChatLock);

	if (TSharedPtr<FChatRoomInfo>* RoomInfo = RoomInfoCache.Find(RoomId))
	{
		return *RoomInfo;
	}

	return nullptr;
}

bool FOnlineChatSimplyStream::GetMembers(const FUniqueNetId& UserId, const FChatRoomId& RoomId, TArray<TSharedRef<FChatRoomMember>>& OutMembers)
{
	FScopeLock ScopeLock(&ChatLock);

	if (const TArray<TSharedRef<FChatRoomMember>>* Members = RoomMembersCache.Find(RoomId))
	{
		OutMembers = *Members;
		return true;
	}

	return false;
}

TSharedPtr<FChatRoomMember> FOnlineChatSimplyStream::GetMember(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId)
{
	FScopeLock ScopeLock(&ChatLock);

	if (const TArray<TSharedRef<FChatRoomMember>>* Members = RoomMembersCache.Find(RoomId))
	{
		for (const TSharedRef<FChatRoomMember>& Member : *Members)
		{
			if (*Member->GetUserId() == MemberId)
			{
				return Member;
			}
		}
	}

	return nullptr;
}

bool FOnlineChatSimplyStream::GetLastMessages(const FUniqueNetId& UserId, const FChatRoomId& RoomId, int32 NumMessages, TArray<TSharedRef<FChatMessage>>& OutMessages)
{
	FScopeLock ScopeLock(&ChatLock);

	if (const TArray<TSharedRef<FChatMessage>>* Messages = RoomMessagesCache.Find(RoomId))
	{
		// Return the last N messages
		int32 StartIndex = FMath::Max(0, Messages->Num() - NumMessages);
		for (int32 i = StartIndex; i < Messages->Num(); ++i)
		{
			OutMessages.Add((*Messages)[i]);
		}
		return true;
	}

	return false;
}

bool FOnlineChatSimplyStream::IsMessageFromLocalUser(const FUniqueNetId& UserId, const FChatMessage& Message, const bool bIncludeExternalInstances)
{
	return UserId == *Message.GetUserId();
}

void FOnlineChatSimplyStream::DumpChatState() const
{
	FScopeLock ScopeLock(&ChatLock);

	UE_LOG_ONLINE_CHAT(Log, TEXT("=== SimplyStream Chat State ==="));
	UE_LOG_ONLINE_CHAT(Log, TEXT("Total users in rooms: %d"), UserRooms.Num());
	UE_LOG_ONLINE_CHAT(Log, TEXT("Total rooms cached: %d"), RoomInfoCache.Num());
	UE_LOG_ONLINE_CHAT(Log, TEXT("Total room members cached: %d"), RoomMembersCache.Num());
	UE_LOG_ONLINE_CHAT(Log, TEXT("Total room messages cached: %d"), RoomMessagesCache.Num());
}

void FOnlineChatSimplyStream::QueryChatHistory(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FUniqueNetIdPtr& RecipientId, int32 MaxMessages)
{
	FString AuthToken = GetAuthToken(UserId);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_CHAT(Warning, TEXT("QueryChatHistory: No auth token for user %s"), *UserId.ToDebugString());
		return;
	}

	FString Url;
	if (RecipientId.IsValid())
	{
		// Query direct message history
		Url = GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/chat/direct/history?recipientId=%s&limit=%d"),
			*RecipientId->ToString(), MaxMessages);
	}
	else
	{
		// Query room message history
		Url = GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/chat/rooms/%s/history?limit=%d"),
			*RoomId, MaxMessages);
	}

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineChatSimplyStream::OnQueryChatHistoryComplete,
		UserId.AsShared(), RoomId);

	HttpRequest->ProcessRequest();
}

void FOnlineChatSimplyStream::SetProfanityFilterEnabled(const FUniqueNetId& UserId, bool bEnabled)
{
	FScopeLock ScopeLock(&ChatLock);

	FUniqueNetIdPtr UserIdPtr = UserId.AsShared();
	if (bEnabled)
	{
		ProfanityFilteredUsers.Add(UserIdPtr);
	}
	else
	{
		ProfanityFilteredUsers.Remove(UserIdPtr);
	}

	UE_LOG_ONLINE_CHAT(Log, TEXT("Profanity filter %s for user %s"),
		bEnabled ? TEXT("enabled") : TEXT("disabled"), *UserId.ToDebugString());
}

bool FOnlineChatSimplyStream::IsProfanityFilterEnabled(const FUniqueNetId& UserId) const
{
	FScopeLock ScopeLock(&ChatLock);
	return ProfanityFilteredUsers.Contains(UserId.AsShared());
}

// HTTP completion handlers

void FOnlineChatSimplyStream::OnCreateRoomComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FChatRoomId RoomId)
{
	bool bSuccess = false;
	FString ErrorStr;

	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 201)
	{
		TSharedPtr<FJsonObject> JsonResponse;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

		if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid())
		{
			bSuccess = JsonResponse->GetBoolField(TEXT("success"));
			if (bSuccess)
			{
				FScopeLock ScopeLock(&ChatLock);

				// Add room to user's joined rooms
				TArray<FChatRoomId>& Rooms = UserRooms.FindOrAdd(UserId);
				Rooms.AddUnique(RoomId);

				// Cache room info
				TSharedPtr<FChatRoomInfo> RoomInfo = MakeShared<FChatRoomInfo>();
				RoomInfo->RoomId = RoomId;
				// Populate other fields from response as needed
				RoomInfoCache.Add(RoomId, RoomInfo);

				UE_LOG_ONLINE_CHAT(Log, TEXT("Room created successfully: %s"), *RoomId);
			}
		}
	}
	else
	{
		ErrorStr = FString::Printf(TEXT("HTTP %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
	}

	Subsystem->ExecuteNextTick([this, UserId, bSuccess, RoomId, ErrorStr]()
	{
		TriggerOnChatRoomCreatedDelegates(*UserId, RoomId, bSuccess, ErrorStr);
	});
}

void FOnlineChatSimplyStream::OnJoinRoomComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FChatRoomId RoomId, FString Nick)
{
	bool bSuccess = false;
	FString ErrorStr;

	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
	{
		TSharedPtr<FJsonObject> JsonResponse;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

		if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid())
		{
			bSuccess = JsonResponse->GetBoolField(TEXT("success"));
			if (bSuccess)
			{
				FScopeLock ScopeLock(&ChatLock);

				// Add room to user's joined rooms
				TArray<FChatRoomId>& Rooms = UserRooms.FindOrAdd(UserId);
				Rooms.AddUnique(RoomId);

				UE_LOG_ONLINE_CHAT(Log, TEXT("Joined room successfully: %s"), *RoomId);
			}
		}
	}
	else
	{
		ErrorStr = FString::Printf(TEXT("HTTP %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
	}

	Subsystem->ExecuteNextTick([this, UserId, bSuccess, RoomId, ErrorStr]()
	{
		TriggerOnChatRoomJoinPublicDelegates(*UserId, RoomId, bSuccess, ErrorStr);
	});
}

void FOnlineChatSimplyStream::OnExitRoomComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FChatRoomId RoomId)
{
	bool bSuccess = false;
	FString ErrorStr;

	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
	{
		FScopeLock ScopeLock(&ChatLock);

		// Remove room from user's joined rooms
		if (TArray<FChatRoomId>* Rooms = UserRooms.Find(UserId))
		{
			Rooms->Remove(RoomId);
		}

		bSuccess = true;
		UE_LOG_ONLINE_CHAT(Log, TEXT("Exited room successfully: %s"), *RoomId);
	}
	else
	{
		ErrorStr = FString::Printf(TEXT("HTTP %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
	}

	Subsystem->ExecuteNextTick([this, UserId, bSuccess, RoomId, ErrorStr]()
	{
		TriggerOnChatRoomExitDelegates(*UserId, RoomId, bSuccess, ErrorStr);
	});
}

void FOnlineChatSimplyStream::OnSendRoomChatComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FChatRoomId RoomId, FString Message)
{
	FString ErrorStr;

	if (bWasSuccessful && Response.IsValid() && (Response->GetResponseCode() == 200 || Response->GetResponseCode() == 201))
	{
		UE_LOG_ONLINE_CHAT(Verbose, TEXT("Room message sent successfully to %s"), *RoomId);
	}
	else
	{
		ErrorStr = FString::Printf(TEXT("HTTP %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
		UE_LOG_ONLINE_CHAT(Warning, TEXT("Failed to send room message: %s"), *ErrorStr);
	}

	Subsystem->ExecuteNextTick([this, UserId, RoomId, ErrorStr]()
	{
		TriggerOnChatRoomMessageSentDelegates(*UserId, RoomId, bSuccess, ErrorStr);
	});
}

void FOnlineChatSimplyStream::OnSendPrivateChatComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef SenderId, FUniqueNetIdRef RecipientId, FString Message)
{
	if (bWasSuccessful && Response.IsValid() && (Response->GetResponseCode() == 200 || Response->GetResponseCode() == 201))
	{
		UE_LOG_ONLINE_CHAT(Verbose, TEXT("Private message sent successfully to %s"), *RecipientId->ToDebugString());

		// Store in local cache
		FScopeLock ScopeLock(&ChatLock);
		TMap<FUniqueNetIdPtr, TArray<TSharedRef<FChatMessage>>>& UserDMs = DirectMessagesCache.FindOrAdd(SenderId);
		TArray<TSharedRef<FChatMessage>>& Conversation = UserDMs.FindOrAdd(RecipientId);

		// Create message record
		TSharedRef<FChatMessage> ChatMessage = MakeShared<FChatMessage>();
		// Populate message fields
		Conversation.Add(ChatMessage);
	}
	else
	{
		UE_LOG_ONLINE_CHAT(Warning, TEXT("Failed to send private message: HTTP %d"),
			Response.IsValid() ? Response->GetResponseCode() : 0);
	}
}

void FOnlineChatSimplyStream::OnQueryChatHistoryComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FChatRoomId RoomId)
{
	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
	{
		TSharedPtr<FJsonObject> JsonResponse;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

		if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* MessagesArray;
			if (JsonResponse->TryGetArrayField(TEXT("messages"), MessagesArray))
			{
				FScopeLock ScopeLock(&ChatLock);
				TArray<TSharedRef<FChatMessage>>& Messages = RoomMessagesCache.FindOrAdd(RoomId);

				for (const TSharedPtr<FJsonValue>& MessageValue : *MessagesArray)
				{
					TSharedPtr<FJsonObject> MessageObj = MessageValue->AsObject();
					if (MessageObj.IsValid())
					{
						ProcessIncomingMessage(*UserId, MessageObj);
					}
				}

				UE_LOG_ONLINE_CHAT(Log, TEXT("Retrieved %d messages for room %s"), MessagesArray->Num(), *RoomId);
			}
		}
	}
}

void FOnlineChatSimplyStream::OnPollMessagesComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId)
{
	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
	{
		TSharedPtr<FJsonObject> JsonResponse;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

		if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* NewMessagesArray;
			if (JsonResponse->TryGetArrayField(TEXT("newMessages"), NewMessagesArray))
			{
				for (const TSharedPtr<FJsonValue>& MessageValue : *NewMessagesArray)
				{
					TSharedPtr<FJsonObject> MessageObj = MessageValue->AsObject();
					if (MessageObj.IsValid())
					{
						ProcessIncomingMessage(*UserId, MessageObj);
					}
				}

				if (NewMessagesArray->Num() > 0)
				{
					UE_LOG_ONLINE_CHAT(Verbose, TEXT("Received %d new messages"), NewMessagesArray->Num());
				}
			}
		}
	}
}

// Helper methods

FString FOnlineChatSimplyStream::GetAPIBaseURL() const
{
	return Subsystem->GetAPIBaseURL();
}

FString FOnlineChatSimplyStream::GetAuthToken(const FUniqueNetId& UserId) const
{
	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (IdentityInt.IsValid())
	{
		int32 LocalUserNum = IdentityInt->GetPlatformUserIdFromUniqueNetId(UserId);
		if (LocalUserNum != INDEX_NONE)
		{
			return IdentityInt->GetAuthToken(LocalUserNum);
		}
	}
	return FString();
}

FString FOnlineChatSimplyStream::FilterProfanity(const FString& Message) const
{
	FString FilteredMessage = Message;

	for (const FString& BadWord : ProfanityWords)
	{
		// Simple replacement with asterisks
		FString Replacement = FString::ChrN(BadWord.Len(), TEXT('*'));
		FilteredMessage = FilteredMessage.Replace(*BadWord, *Replacement, ESearchCase::IgnoreCase);
	}

	return FilteredMessage;
}

bool FOnlineChatSimplyStream::ContainsProfanity(const FString& Message) const
{
	FString LowerMessage = Message.ToLower();

	for (const FString& BadWord : ProfanityWords)
	{
		if (LowerMessage.Contains(BadWord.ToLower()))
		{
			return true;
		}
	}

	return false;
}

void FOnlineChatSimplyStream::ProcessIncomingMessage(const FUniqueNetId& UserId, const TSharedPtr<FJsonObject>& MessageJson)
{
	if (!MessageJson.IsValid())
	{
		return;
	}

	// Extract message details
	FString MessageId;
	FString SenderId;
	FString RoomId;
	FString MessageBody;
	FString MessageType;
	FString Timestamp;

	MessageJson->TryGetStringField(TEXT("messageId"), MessageId);
	MessageJson->TryGetStringField(TEXT("senderId"), SenderId);
	MessageJson->TryGetStringField(TEXT("roomId"), RoomId);
	MessageJson->TryGetStringField(TEXT("message"), MessageBody);
	MessageJson->TryGetStringField(TEXT("messageType"), MessageType);
	MessageJson->TryGetStringField(TEXT("timestamp"), Timestamp);

	// Create chat message
	TSharedRef<FChatMessage> ChatMessage = MakeShared<FChatMessage>();
	// TODO: Populate FChatMessage fields properly
	// This is a simplified version - extend as needed

	FScopeLock ScopeLock(&ChatLock);

	if (!RoomId.IsEmpty())
	{
		// Room message
		TArray<TSharedRef<FChatMessage>>& Messages = RoomMessagesCache.FindOrAdd(RoomId);
		Messages.Add(ChatMessage);

		// Trigger delegate on game thread
		Subsystem->ExecuteNextTick([this, UserId, RoomId, ChatMessage]()
		{
			TriggerOnChatRoomMessageReceivedDelegates(*UserId.AsShared(), RoomId, ChatMessage);
		});
	}
	else
	{
		// Direct message
		FUniqueNetIdPtr SenderIdPtr = FUniqueNetIdSimplyStream::Create(SenderId);
		TMap<FUniqueNetIdPtr, TArray<TSharedRef<FChatMessage>>>& UserDMs = DirectMessagesCache.FindOrAdd(UserId.AsShared());
		TArray<TSharedRef<FChatMessage>>& Conversation = UserDMs.FindOrAdd(SenderIdPtr);
		Conversation.Add(ChatMessage);

		// Trigger delegate on game thread
		Subsystem->ExecuteNextTick([this, UserId, ChatMessage]()
		{
			TriggerOnChatPrivateMessageReceivedDelegates(*UserId.AsShared(), ChatMessage);
		});
	}
}

void FOnlineChatSimplyStream::PollForNewMessages(const FUniqueNetId& UserId)
{
	FString AuthToken = GetAuthToken(UserId);
	if (AuthToken.IsEmpty())
	{
		return;
	}

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/chat/poll"));
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));

HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineChatSimplyStream::OnPollMessagesComplete,
		UserId.AsShared());

	HttpRequest->ProcessRequest();
}

#else // SIMPLYSTREAM_ENABLE_CHAT

namespace
{
bool LogChatDisabled(const TCHAR* MethodName)
{
	UE_LOG_ONLINE_CHAT(Verbose, TEXT("%s is disabled (SIMPLYSTREAM_ENABLE_CHAT=0)"), MethodName);
	return false;
}
}

FOnlineChatSimplyStream::FOnlineChatSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
	: Subsystem(InSubsystem)
	, TimeSinceLastPoll(0.0f)
	, MessagePollInterval(5.0f)
{
}

FOnlineChatSimplyStream::~FOnlineChatSimplyStream() = default;

bool FOnlineChatSimplyStream::Init()
{
	UE_LOG_ONLINE_CHAT(Log, TEXT("Online chat is disabled (SIMPLYSTREAM_ENABLE_CHAT=0)"));
	return true;
}

void FOnlineChatSimplyStream::Shutdown()
{
}

void FOnlineChatSimplyStream::Tick(float DeltaTime)
{
}

bool FOnlineChatSimplyStream::CreateRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nick, const FChatRoomConfig& ChatRoomConfig)
{
	return LogChatDisabled(TEXT("CreateRoom"));
}

bool FOnlineChatSimplyStream::ConfigureRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FChatRoomConfig& ChatRoomConfig)
{
	return LogChatDisabled(TEXT("ConfigureRoom"));
}

bool FOnlineChatSimplyStream::JoinPublicRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nick, const FChatRoomConfig& ChatRoomConfig)
{
	return LogChatDisabled(TEXT("JoinPublicRoom"));
}

bool FOnlineChatSimplyStream::JoinPrivateRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& Nick, const FChatRoomConfig& ChatRoomConfig)
{
	return LogChatDisabled(TEXT("JoinPrivateRoom"));
}

bool FOnlineChatSimplyStream::ExitRoom(const FUniqueNetId& UserId, const FChatRoomId& RoomId)
{
	return LogChatDisabled(TEXT("ExitRoom"));
}

bool FOnlineChatSimplyStream::SendRoomChat(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FString& MsgBody)
{
	return LogChatDisabled(TEXT("SendRoomChat"));
}

bool FOnlineChatSimplyStream::SendPrivateChat(const FUniqueNetId& UserId, const FUniqueNetId& RecipientId, const FString& MsgBody)
{
	return LogChatDisabled(TEXT("SendPrivateChat"));
}

bool FOnlineChatSimplyStream::IsChatAllowed(const FUniqueNetId& UserId, const FUniqueNetId& RecipientId) const
{
	return false;
}

void FOnlineChatSimplyStream::GetJoinedRooms(const FUniqueNetId& UserId, TArray<FChatRoomId>& OutRooms)
{
	OutRooms.Reset();
}

TSharedPtr<FChatRoomInfo> FOnlineChatSimplyStream::GetRoomInfo(const FUniqueNetId& UserId, const FChatRoomId& RoomId)
{
	LogChatDisabled(TEXT("GetRoomInfo"));
	return nullptr;
}

bool FOnlineChatSimplyStream::GetMembers(const FUniqueNetId& UserId, const FChatRoomId& RoomId, TArray<TSharedRef<FChatRoomMember>>& OutMembers)
{
	OutMembers.Reset();
	return LogChatDisabled(TEXT("GetMembers"));
}

TSharedPtr<FChatRoomMember> FOnlineChatSimplyStream::GetMember(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId)
{
	LogChatDisabled(TEXT("GetMember"));
	return nullptr;
}

bool FOnlineChatSimplyStream::GetLastMessages(const FUniqueNetId& UserId, const FChatRoomId& RoomId, int32 NumMessages, TArray<TSharedRef<FChatMessage>>& OutMessages)
{
	OutMessages.Reset();
	return LogChatDisabled(TEXT("GetLastMessages"));
}

bool FOnlineChatSimplyStream::IsMessageFromLocalUser(const FUniqueNetId& UserId, const FChatMessage& Message, const bool bIncludeExternalInstances)
{
	return false;
}

void FOnlineChatSimplyStream::DumpChatState() const
{
}

void FOnlineChatSimplyStream::QueryChatHistory(const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FUniqueNetIdPtr& RecipientId, int32 MaxMessages)
{
	LogChatDisabled(TEXT("QueryChatHistory"));
}

void FOnlineChatSimplyStream::SetProfanityFilterEnabled(const FUniqueNetId& UserId, bool bEnabled)
{
}

bool FOnlineChatSimplyStream::IsProfanityFilterEnabled(const FUniqueNetId& UserId) const
{
	return false;
}

FString FOnlineChatSimplyStream::GetAPIBaseURL() const
{
	return Subsystem ? Subsystem->GetAPIBaseURL() : FString();
}

FString FOnlineChatSimplyStream::GetAuthToken(const FUniqueNetId& UserId) const
{
	return FString();
}

FString FOnlineChatSimplyStream::FilterProfanity(const FString& Message) const
{
	return Message;
}

bool FOnlineChatSimplyStream::ContainsProfanity(const FString& Message) const
{
	return false;
}

void FOnlineChatSimplyStream::ProcessIncomingMessage(const FUniqueNetId& UserId, const TSharedPtr<FJsonObject>& MessageJson)
{
}

void FOnlineChatSimplyStream::PollForNewMessages(const FUniqueNetId& UserId)
{
}

#endif // SIMPLYSTREAM_ENABLE_CHAT
