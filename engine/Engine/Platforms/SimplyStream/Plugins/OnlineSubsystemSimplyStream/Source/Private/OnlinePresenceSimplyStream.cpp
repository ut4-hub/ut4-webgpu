// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlinePresenceSimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Math/UnrealMathUtility.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/DateTime.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr bool IsPlayingState(EOnlinePresenceState::Type State)
	{
		return State == EOnlinePresenceState::Online ||
			   State == EOnlinePresenceState::Chat ||
			   State == EOnlinePresenceState::DoNotDisturb;
	}

	static const TCHAR* RichPresencePropertyKey = TEXT("richPresenceJson");
}

FOnlinePresenceSimplyStream::FOnlinePresenceSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
	: Subsystem(InSubsystem)
{
}

void FOnlinePresenceSimplyStream::SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	if (Subsystem == nullptr)
	{
		UE_LOG_ONLINE_PRESENCE(Warning, TEXT("SetPresence failed: subsystem not initialized"));
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		UE_LOG_ONLINE_PRESENCE(Warning, TEXT("SetPresence failed: no identity interface"));
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, User);
	if (LocalUserNum == INDEX_NONE)
	{
		UE_LOG_ONLINE_PRESENCE(Warning, TEXT("SetPresence failed: user %s is not a local player"), *User.ToString());
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	const FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_PRESENCE(Warning, TEXT("SetPresence failed: missing auth token for user %d"), LocalUserNum);
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	RequestJson->SetStringField(TEXT("status"), ConvertPresenceStateToBackend(Status.State));

	if (!Status.StatusStr.IsEmpty())
	{
		RequestJson->SetStringField(TEXT("statusMessage"), Status.StatusStr);
	}

	const FVariantData* GameModeData = Status.Properties.Find(TEXT("currentGameMode"));
	if (GameModeData != nullptr)
	{
		RequestJson->SetStringField(TEXT("currentGameMode"), GameModeData->ToString());
	}

	if (Status.Properties.Contains(DefaultSessionIdKey))
	{
		RequestJson->SetBoolField(TEXT("isJoinable"), true);
	}

	const FVariantData* SessionIdData = Status.Properties.Find(DefaultSessionIdKey);
	if (SessionIdData && SessionIdData->GetType() == EOnlineKeyValuePairDataType::String)
	{
		FString SessionIdValue;
		SessionIdData->GetValue(SessionIdValue);
		if (!SessionIdValue.IsEmpty())
		{
			RequestJson->SetStringField(TEXT("sessionId"), SessionIdValue);
		}
	}

	const FVariantData* RichPresenceData = Status.Properties.Find(RichPresencePropertyKey);
	if (RichPresenceData && RichPresenceData->GetType() == EOnlineKeyValuePairDataType::String)
	{
		FString RichPresenceRaw;
		RichPresenceData->GetValue(RichPresenceRaw);
		if (!RichPresenceRaw.IsEmpty())
		{
			TSharedPtr<FJsonObject> RichPresenceObject;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RichPresenceRaw);
			if (FJsonSerializer::Deserialize(Reader, RichPresenceObject) && RichPresenceObject.IsValid())
			{
				RequestJson->SetObjectField(TEXT("richPresence"), RichPresenceObject);
			}
		}
	}

	FString SerializedBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedBody);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/presence"));
	HttpRequest->SetVerb(TEXT("PATCH"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetContentAsString(SerializedBody);

	TSharedRef<const FUniqueNetId> UserRef = User.AsShared();
	FOnPresenceTaskCompleteDelegate DelegateCopy = Delegate;
	HttpRequest->OnProcessRequestComplete().BindRaw(this, &FOnlinePresenceSimplyStream::HandleSetPresenceResponse, UserRef, DelegateCopy);

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG_ONLINE_PRESENCE(Warning, TEXT("SetPresence failed: HTTP request could not be processed"));
		Delegate.ExecuteIfBound(User, false);
	}
}

void FOnlinePresenceSimplyStream::QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	if (Subsystem == nullptr)
	{
		UE_LOG_ONLINE_PRESENCE(Warning, TEXT("QueryPresence failed: subsystem not initialized"));
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		UE_LOG_ONLINE_PRESENCE(Warning, TEXT("QueryPresence failed: no identity interface"));
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	const int32 LocalUserNum = ResolveLocalUserNumForRequest(IdentityInt, User);
	if (LocalUserNum == INDEX_NONE)
	{
		UE_LOG_ONLINE_PRESENCE(Warning, TEXT("QueryPresence failed: no logged in local user available"));
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	const FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_PRESENCE(Warning, TEXT("QueryPresence failed: missing auth token for user %d"), LocalUserNum);
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> UserArray;
	UserArray.Add(MakeShared<FJsonValueString>(User.ToString()));
	RequestJson->SetArrayField(TEXT("userIds"), UserArray);

	FString SerializedBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedBody);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/presence/bulk"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetContentAsString(SerializedBody);

	TSharedRef<const FUniqueNetId> UserRef = User.AsShared();
	FOnPresenceTaskCompleteDelegate DelegateCopy = Delegate;
	HttpRequest->OnProcessRequestComplete().BindRaw(this, &FOnlinePresenceSimplyStream::HandleQueryPresenceResponse, UserRef, DelegateCopy);

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG_ONLINE_PRESENCE(Warning, TEXT("QueryPresence failed: HTTP request could not be processed"));
		Delegate.ExecuteIfBound(User, false);
	}
}

EOnlineCachedResult::Type FOnlinePresenceSimplyStream::GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	FScopeLock Lock(&PresenceLock);

	if (TSharedRef<FOnlineUserPresence>* PresencePtr = PresenceCache.Find(User.AsShared()))
	{
		OutPresence = *PresencePtr;
		return EOnlineCachedResult::Success;
	}

	OutPresence.Reset();
	return EOnlineCachedResult::NotFound;
}

EOnlineCachedResult::Type FOnlinePresenceSimplyStream::GetCachedPresenceForApp(const FUniqueNetId& LocalUserId, const FUniqueNetId& User, const FString& AppId, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	const EOnlineCachedResult::Type Result = GetCachedPresence(User, OutPresence);
	if (Result != EOnlineCachedResult::Success || !OutPresence.IsValid())
	{
		return Result;
	}

	const FString PresenceAppId = OutPresence->GetAppId();
	if (!AppId.IsEmpty() && !PresenceAppId.IsEmpty() && PresenceAppId != AppId)
	{
		OutPresence.Reset();
		return EOnlineCachedResult::NotFound;
	}

	return Result;
}

FString FOnlinePresenceSimplyStream::ConvertPresenceStateToBackend(EOnlinePresenceState::Type State) const
{
	switch (State)
	{
	case EOnlinePresenceState::Away:
	case EOnlinePresenceState::ExtendedAway:
		return TEXT("away");
	case EOnlinePresenceState::DoNotDisturb:
		return TEXT("busy");
	case EOnlinePresenceState::Offline:
		return TEXT("offline");
	default:
		return TEXT("online");
	}
}

EOnlinePresenceState::Type FOnlinePresenceSimplyStream::ConvertBackendStateToPresence(const FString& State) const
{
	if (State.Equals(TEXT("away"), ESearchCase::IgnoreCase))
	{
		return EOnlinePresenceState::Away;
	}
	if (State.Equals(TEXT("busy"), ESearchCase::IgnoreCase))
	{
		return EOnlinePresenceState::DoNotDisturb;
	}
	if (State.Equals(TEXT("offline"), ESearchCase::IgnoreCase) || State.Equals(TEXT("invisible"), ESearchCase::IgnoreCase))
	{
		return EOnlinePresenceState::Offline;
	}
	return EOnlinePresenceState::Online;
}

FOnlineUserPresenceStatus FOnlinePresenceSimplyStream::BuildPresenceStatusFromJson(const TSharedPtr<FJsonObject>& JsonObject) const
{
	FOnlineUserPresenceStatus Status;

	FString BackendStatus;
	if (JsonObject->TryGetStringField(TEXT("status"), BackendStatus))
	{
		Status.State = ConvertBackendStateToPresence(BackendStatus);
	}
	else
	{
		Status.State = EOnlinePresenceState::Online;
	}

	FString StatusMessage;
	if (JsonObject->TryGetStringField(TEXT("statusMessage"), StatusMessage))
	{
		Status.StatusStr = StatusMessage;
	}
	else
	{
		Status.StatusStr.Reset();
	}

	FString GameMode;
	if (JsonObject->TryGetStringField(TEXT("currentGameMode"), GameMode) && !GameMode.IsEmpty())
	{
		Status.Properties.Add(TEXT("currentGameMode"), FVariantData(GameMode));
	}

	FString SessionId;
	if (JsonObject->TryGetStringField(TEXT("sessionId"), SessionId) && !SessionId.IsEmpty())
	{
		Status.Properties.Add(DefaultSessionIdKey, FVariantData(SessionId));
	}

	const TSharedPtr<FJsonObject>* RichPresenceJson = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("richPresence"), RichPresenceJson) && RichPresenceJson && (*RichPresenceJson).IsValid())
	{
		FString RichPresenceString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RichPresenceString);
		FJsonSerializer::Serialize((*RichPresenceJson).ToSharedRef(), Writer);
		if (!RichPresenceString.IsEmpty())
		{
			Status.Properties.Add(RichPresencePropertyKey, FVariantData(RichPresenceString));
		}
	}

	return Status;
}

TSharedPtr<FOnlineUserPresence> FOnlinePresenceSimplyStream::UpdatePresenceCache(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, bool bIsJoinable, const FString& LastHeartbeat)
{
	TSharedRef<const FUniqueNetId> UserRef = User.AsShared();
	TSharedPtr<FOnlineUserPresence> UpdatedPresence;

	{
		FScopeLock Lock(&PresenceLock);
		TSharedRef<FOnlineUserPresence>* PresencePtr = PresenceCache.Find(UserRef);
		if (PresencePtr == nullptr)
		{
			PresencePtr = &PresenceCache.Add(UserRef, MakeShared<FOnlineUserPresence>());
		}

		FOnlineUserPresence& Presence = PresencePtr->Get();
		Presence.Status = Status;
		Presence.bIsOnline = Status.State != EOnlinePresenceState::Offline;
		Presence.bIsPlaying = IsPlayingState(Status.State);
		Presence.bIsPlayingThisGame = true;
		Presence.bIsJoinable = bIsJoinable;
		Presence.bHasVoiceSupport = Status.Properties.Contains(TEXT("voiceSupported"));

		if (const FVariantData* SessionIdProp = Status.Properties.Find(DefaultSessionIdKey))
		{
			if (SessionIdProp->GetType() == EOnlineKeyValuePairDataType::String)
			{
				FString SessionIdValue;
				SessionIdProp->GetValue(SessionIdValue);
				if (!SessionIdValue.IsEmpty())
				{
					Presence.SessionId = FUniqueNetIdSimplyStream::Create(SessionIdValue);
				}
				else
				{
					Presence.SessionId.Reset();
				}
			}
			else
			{
				Presence.SessionId.Reset();
			}
		}
		else
		{
			Presence.SessionId.Reset();
		}

		FDateTime ParsedHeartbeat;
		if (!LastHeartbeat.IsEmpty() && FDateTime::ParseIso8601(*LastHeartbeat, ParsedHeartbeat))
		{
			Presence.LastOnline = ParsedHeartbeat;
		}
		else
		{
			Presence.LastOnline = FDateTime::UtcNow();
		}

		UpdatedPresence = *PresencePtr;
	}

	return UpdatedPresence;
}

int32 FOnlinePresenceSimplyStream::ResolveLocalUserNumForRequest(const IOnlineIdentityPtr& IdentityInterface, const FUniqueNetId& TargetUser) const
{
	const int32 DirectMatch = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInterface, TargetUser);
	if (DirectMatch != INDEX_NONE)
	{
		return DirectMatch;
	}

	constexpr int32 MaxLocalUsers = 4;
	for (int32 UserIdx = 0; UserIdx < MaxLocalUsers; ++UserIdx)
	{
		if (IdentityInterface->GetLoginStatus(UserIdx) == ELoginStatus::LoggedIn)
		{
			return UserIdx;
		}
	}

	return INDEX_NONE;
}

void FOnlinePresenceSimplyStream::HandleSetPresenceResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, TSharedRef<const FUniqueNetId> UserRef, FOnPresenceTaskCompleteDelegate Delegate)
{
	bool bSuccess = false;

	if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		TSharedPtr<FJsonObject> JsonObject;
		const FString Content = Response->GetContentAsString();
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			bool bResponseSuccess = false;
			JsonObject->TryGetBoolField(TEXT("success"), bResponseSuccess);
			const TSharedPtr<FJsonObject>* ResultObject = nullptr;
			if (bResponseSuccess && JsonObject->TryGetObjectField(TEXT("result"), ResultObject) && ResultObject != nullptr)
			{
				const bool bIsJoinable = (*ResultObject)->GetBoolField(TEXT("isJoinable"));
				const FString LastHeartbeat = (*ResultObject)->GetStringField(TEXT("lastHeartbeat"));
				const FOnlineUserPresenceStatus Status = BuildPresenceStatusFromJson(*ResultObject);
				bSuccess = UpdatePresenceCache(*UserRef, Status, bIsJoinable, LastHeartbeat).IsValid();
			}
		}
	}

	if (!bSuccess)
	{
		UE_LOG_ONLINE_PRESENCE(Warning, TEXT("SetPresence HTTP request failed: %s"), Response.IsValid() ? *Response->GetContentAsString() : TEXT("no response"));
	}

	TSharedPtr<FOnlineUserPresence> PresenceCopy;
	if (bSuccess)
	{
		FScopeLock Lock(&PresenceLock);
		if (TSharedRef<FOnlineUserPresence>* PresencePtr = PresenceCache.Find(UserRef))
		{
			PresenceCopy = *PresencePtr;
		}
	}

	Subsystem->ExecuteNextTick([this, UserRef, Delegate, bSuccess, PresenceCopy]()
	{
		Delegate.ExecuteIfBound(*UserRef, bSuccess);
		if (bSuccess && PresenceCopy.IsValid())
		{
			TriggerOnPresenceReceivedDelegates(*UserRef, PresenceCopy.ToSharedRef());
		}
	});
}

void FOnlinePresenceSimplyStream::HandleQueryPresenceResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, TSharedRef<const FUniqueNetId> TargetUserRef, FOnPresenceTaskCompleteDelegate Delegate)
{
	bool bSuccess = false;
	TSharedPtr<FOnlineUserPresence> PresenceCopy;

	if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		TSharedPtr<FJsonObject> JsonObject;
		const FString Content = Response->GetContentAsString();
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			bool bResponseSuccess = false;
			JsonObject->TryGetBoolField(TEXT("success"), bResponseSuccess);
			const TArray<TSharedPtr<FJsonValue>>* ResultsArray = nullptr;
			if (bResponseSuccess && JsonObject->TryGetArrayField(TEXT("results"), ResultsArray) && ResultsArray != nullptr)
			{
				for (const TSharedPtr<FJsonValue>& ResultValue : *ResultsArray)
				{
					const TSharedPtr<FJsonObject>* ResultObject = nullptr;
					if (ResultValue.IsValid() && ResultValue->TryGetObject(ResultObject) && ResultObject != nullptr)
					{
						FString ResultUserId;
						if ((*ResultObject)->TryGetStringField(TEXT("userId"), ResultUserId) && ResultUserId == TargetUserRef->ToString())
						{
							const bool bIsJoinable = (*ResultObject)->GetBoolField(TEXT("isJoinable"));
							const FString LastHeartbeat = (*ResultObject)->GetStringField(TEXT("lastHeartbeat"));
							const FOnlineUserPresenceStatus Status = BuildPresenceStatusFromJson(*ResultObject);
							PresenceCopy = UpdatePresenceCache(*TargetUserRef, Status, bIsJoinable, LastHeartbeat);
							bSuccess = PresenceCopy.IsValid();
							break;
						}
					}
				}
			}
		}
	}

	if (!bSuccess)
	{
		UE_LOG_ONLINE_PRESENCE(Verbose, TEXT("QueryPresence: no presence data returned for %s"), *TargetUserRef->ToString());
	}

	Subsystem->ExecuteNextTick([this, TargetUserRef, Delegate, bSuccess, PresenceCopy]()
	{
		Delegate.ExecuteIfBound(*TargetUserRef, bSuccess);
		if (bSuccess && PresenceCopy.IsValid())
		{
			TriggerOnPresenceReceivedDelegates(*TargetUserRef, PresenceCopy.ToSharedRef());
		}
	});
}

void FOnlinePresenceSimplyStream::Tick(float DeltaTime)
{
	const double Now = FPlatformTime::Seconds();
	TArray<FUniqueNetIdSimplyStreamPtr> DueUsers;
	{
		FScopeLock Lock(&SubscriptionLock);
		for (auto& Pair : AutoRefreshEntries)
		{
			if (Pair.Value.UserId.IsValid() && Pair.Value.NextRefreshTime <= Now)
			{
				DueUsers.Add(Pair.Value.UserId);
				Pair.Value.NextRefreshTime = Now + AutoRefreshIntervalSeconds;
			}
		}
	}

	for (int32 Index = 0; Index < DueUsers.Num(); Index += AutoRefreshBatchSize)
	{
		const int32 BatchCount = FMath::Min(AutoRefreshBatchSize, DueUsers.Num() - Index);
		TArray<FUniqueNetIdSimplyStreamPtr> Batch;
		Batch.Reserve(BatchCount);
		for (int32 Offset = 0; Offset < BatchCount; ++Offset)
		{
			Batch.Add(DueUsers[Index + Offset]);
		}

		if (!SendAutoPresenceQuery(Batch))
		{
			TArray<FString> FailedIds;
			for (const FUniqueNetIdSimplyStreamPtr& UserId : Batch)
			{
				FailedIds.Add(UserId->ToString());
			}
			RescheduleTrackedUsers(FailedIds, AutoRefreshRetrySeconds);
		}
	}
}

void FOnlinePresenceSimplyStream::TrackRemoteUser(const FUniqueNetId& UserId)
{
	const FString UserKey = UserId.ToString();
	FScopeLock Lock(&SubscriptionLock);
	FPresenceAutoRefreshEntry& Entry = AutoRefreshEntries.FindOrAdd(UserKey);
	Entry.UserId = FUniqueNetIdSimplyStream::CreateTyped(UserKey);
	Entry.NextRefreshTime = 0.0;
}

void FOnlinePresenceSimplyStream::UntrackRemoteUser(const FUniqueNetId& UserId)
{
	const FString UserKey = UserId.ToString();
	FScopeLock Lock(&SubscriptionLock);
	AutoRefreshEntries.Remove(UserKey);
}

int32 FOnlinePresenceSimplyStream::GetDefaultLocalUserNum(const IOnlineIdentityPtr& IdentityInterface) const
{
	constexpr int32 MaxLocalUsers = 4;
	for (int32 LocalUserIdx = 0; LocalUserIdx < MaxLocalUsers; ++LocalUserIdx)
	{
		if (IdentityInterface->GetLoginStatus(LocalUserIdx) == ELoginStatus::LoggedIn)
		{
			return LocalUserIdx;
		}
	}
	return INDEX_NONE;
}

bool FOnlinePresenceSimplyStream::SendAutoPresenceQuery(const TArray<FUniqueNetIdSimplyStreamPtr>& Users)
{
	if (Users.Num() == 0)
	{
		return true;
	}

	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		return false;
	}

	const int32 LocalUserNum = GetDefaultLocalUserNum(IdentityInt);
	if (LocalUserNum == INDEX_NONE)
	{
		return false;
	}

	const FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
	if (AuthToken.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> UserArray;
	TArray<FString> RequestedIds;
	for (const FUniqueNetIdSimplyStreamPtr& UserId : Users)
	{
		UserArray.Add(MakeShared<FJsonValueString>(UserId->ToString()));
		RequestedIds.Add(UserId->ToString());
	}
	RequestJson->SetArrayField(TEXT("userIds"), UserArray);

	FString SerializedBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedBody);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/presence/bulk"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetContentAsString(SerializedBody);

	HttpRequest->OnProcessRequestComplete().BindRaw(this, &FOnlinePresenceSimplyStream::HandleAutoQueryPresenceResponse, RequestedIds);

	if (!HttpRequest->ProcessRequest())
	{
		return false;
	}

	return true;
}

void FOnlinePresenceSimplyStream::RescheduleTrackedUsers(const TArray<FString>& UserIds, double DelaySeconds)
{
	const double NewTime = FPlatformTime::Seconds() + DelaySeconds;
	FScopeLock Lock(&SubscriptionLock);
	for (const FString& UserKey : UserIds)
	{
		if (FPresenceAutoRefreshEntry* Entry = AutoRefreshEntries.Find(UserKey))
		{
			Entry->NextRefreshTime = NewTime;
		}
	}
}

void FOnlinePresenceSimplyStream::HandleAutoQueryPresenceResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, TArray<FString> RequestedUserIds)
{
	bool bSuccess = false;
	if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		TSharedPtr<FJsonObject> JsonObject;
		const FString Content = Response->GetContentAsString();
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			bool bApiSuccess = false;
			JsonObject->TryGetBoolField(TEXT("success"), bApiSuccess);
			if (bApiSuccess)
			{
				const TArray<TSharedPtr<FJsonValue>>* ResultsArray = nullptr;
				if (JsonObject->TryGetArrayField(TEXT("results"), ResultsArray) && ResultsArray != nullptr)
				{
					for (const TSharedPtr<FJsonValue>& ResultValue : *ResultsArray)
					{
						const TSharedPtr<FJsonObject>* ResultObject = nullptr;
						if (ResultValue.IsValid() && ResultValue->TryGetObject(ResultObject) && ResultObject != nullptr)
						{
							FString ResultUserId;
							if ((*ResultObject)->TryGetStringField(TEXT("userId"), ResultUserId))
							{
								const bool bIsJoinable = (*ResultObject)->GetBoolField(TEXT("isJoinable"));
								const FString LastHeartbeat = (*ResultObject)->GetStringField(TEXT("lastHeartbeat"));
								const FOnlineUserPresenceStatus Status = BuildPresenceStatusFromJson(*ResultObject);
								UpdatePresenceCache(*FUniqueNetIdSimplyStream::Create(ResultUserId), Status, bIsJoinable, LastHeartbeat);
							}
						}
					}
					bSuccess = true;
				}
			}
		}
	}

	if (!bSuccess)
	{
		RescheduleTrackedUsers(RequestedUserIds, AutoRefreshRetrySeconds);
	}
}
