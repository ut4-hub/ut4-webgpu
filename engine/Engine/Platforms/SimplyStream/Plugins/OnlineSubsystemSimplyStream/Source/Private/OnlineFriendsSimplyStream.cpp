// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineFriendsSimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlinePresenceSimplyStream.h"
#include "OnlineIdentitySimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"
#include "OnlineSubsystemSimplyStreamTypes.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	FString NormalizeListName(const FString& InListName)
	{
		FString Normalized = InListName;
		Normalized.TrimStartAndEndInline();
		if (Normalized.IsEmpty())
		{
			return TEXT("default");
		}
		return Normalized.ToLower();
	}

	FString StatusParamForList(const FString& NormalizedListName)
	{
		if (NormalizedListName == TEXT("all"))
		{
			return TEXT("all");
		}
		if (NormalizedListName == TEXT("pending_inbound"))
		{
			return TEXT("pending_inbound");
		}
		if (NormalizedListName == TEXT("pending_outbound"))
		{
			return TEXT("pending_outbound");
		}
		if (NormalizedListName == TEXT("pending"))
		{
			return TEXT("pending");
		}
		return TEXT("accepted");
	}

	bool ShouldIncludePresence(const FString& NormalizedListName)
	{
		return NormalizedListName == TEXT("default");
	}

	FString EncounterTypeToString(ERecentPlayerEncounterType Type)
	{
		switch (Type)
		{
		case ERecentPlayerEncounterType::Teammate:
			return TEXT("Teammate");
		case ERecentPlayerEncounterType::Opponent:
			return TEXT("Opponent");
		default:
			return TEXT("Default");
		}
	}

	ERecentPlayerEncounterType EncounterTypeFromString(const FString& Type)
	{
		if (Type.Equals(TEXT("Teammate"), ESearchCase::IgnoreCase))
		{
			return ERecentPlayerEncounterType::Teammate;
		}
		if (Type.Equals(TEXT("Opponent"), ESearchCase::IgnoreCase))
		{
			return ERecentPlayerEncounterType::Opponent;
		}
		return ERecentPlayerEncounterType::Default;
	}

	EOnlinePresenceState::Type PresenceStateFromString(const FString& State)
	{
		if (State.Equals(TEXT("away"), ESearchCase::IgnoreCase))
		{
			return EOnlinePresenceState::Away;
		}
		if (State.Equals(TEXT("dnd"), ESearchCase::IgnoreCase) || State.Equals(TEXT("busy"), ESearchCase::IgnoreCase))
		{
			return EOnlinePresenceState::DoNotDisturb;
		}
		if (State.Equals(TEXT("offline"), ESearchCase::IgnoreCase) || State.Equals(TEXT("invisible"), ESearchCase::IgnoreCase))
		{
			return EOnlinePresenceState::Offline;
		}
		return EOnlinePresenceState::Online;
	}
}

FOnlineFriendSimplyStream::FOnlineFriendSimplyStream(const FString& InFriendshipId, const FString& InUserId, const FString& InUsername, const FString& InDisplayName, const FString& InPlatform, const FString& InAvatarUrl)
    : FriendshipId(InFriendshipId)
    , UserId(FUniqueNetIdSimplyStream::Create(InUserId, FString(), InPlatform))
    , Username(InUsername)
    , DisplayName(InDisplayName)
    , Platform(InPlatform)
    , AvatarUrl(InAvatarUrl)
{
    Presence.Status.State = EOnlinePresenceState::Offline;
}

FUniqueNetIdRef FOnlineFriendSimplyStream::GetUserId() const
{
	check(UserId.IsValid());
	return UserId.ToSharedRef();
}

FString FOnlineFriendSimplyStream::GetRealName() const
{
    return DisplayName;
}

FString FOnlineFriendSimplyStream::GetDisplayName(const FString& InPlatform) const
{
    return DisplayName;
}

bool FOnlineFriendSimplyStream::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	if (AttrName == USER_ATTR_DISPLAYNAME || AttrName == USER_ATTR_REALNAME)
	{
		OutAttrValue = DisplayName;
		return true;
	}
	if (AttrName == TEXT("platform"))
	{
		OutAttrValue = Platform;
		return true;
	}
	if (AttrName == TEXT("avatar_url"))
	{
		OutAttrValue = AvatarUrl;
		return true;
	}
	if (AttrName == TEXT("friendship_id"))
	{
		OutAttrValue = FriendshipId;
		return true;
	}
	return false;
}

EInviteStatus::Type FOnlineFriendSimplyStream::GetInviteStatus() const
{
    return InviteStatus;
}

const FOnlineUserPresence& FOnlineFriendSimplyStream::GetPresence() const
{
    return Presence;
}

void FOnlineFriendSimplyStream::SetInviteStatus(EInviteStatus::Type NewStatus)
{
    InviteStatus = NewStatus;
}

void FOnlineFriendSimplyStream::SetPresence(const FOnlineUserPresence& NewPresence)
{
    Presence = NewPresence;
}

FOnlineBlockedPlayerSimplyStream::FOnlineBlockedPlayerSimplyStream(const FString& InUserId, const FString& InUsername, const FString& InDisplayName)
    : UserId(FUniqueNetIdSimplyStream::Create(InUserId))
    , Username(InUsername)
    , DisplayName(InDisplayName)
{
}

FUniqueNetIdRef FOnlineBlockedPlayerSimplyStream::GetUserId() const
{
	check(UserId.IsValid());
	return UserId.ToSharedRef();
}

FString FOnlineBlockedPlayerSimplyStream::GetRealName() const
{
    return DisplayName;
}

FString FOnlineBlockedPlayerSimplyStream::GetDisplayName(const FString& InPlatform) const
{
    return DisplayName;
}

bool FOnlineBlockedPlayerSimplyStream::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
    if (AttrName == USER_ATTR_DISPLAYNAME || AttrName == USER_ATTR_REALNAME)
    {
        OutAttrValue = DisplayName;
        return true;
    }
    return false;
}

FOnlineRecentPlayerSimplyStream::FOnlineRecentPlayerSimplyStream(const FString& InUserId, const FString& InUsername, const FString& InDisplayName, const FString& InPlatform, const FDateTime& InLastSeen, const FString& InPresenceStr, ERecentPlayerEncounterType InEncounterType)
    : UserId(FUniqueNetIdSimplyStream::Create(InUserId, FString(), InPlatform))
    , Username(InUsername)
    , DisplayName(InDisplayName)
    , Platform(InPlatform)
    , PresenceStr(InPresenceStr)
    , EncounterType(InEncounterType)
    , LastSeen(InLastSeen)
    , Presence(MakeShared<FOnlineUserPresence>())
{
    Presence->Status.StatusStr = PresenceStr;
    Presence->Status.State = EOnlinePresenceState::Offline;
}

bool FOnlineRecentPlayerSimplyStream::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
    if (AttrName == USER_ATTR_DISPLAYNAME || AttrName == USER_ATTR_REALNAME)
    {
        OutAttrValue = DisplayName;
        return true;
    }
    if (AttrName == TEXT("platform"))
    {
        OutAttrValue = Platform;
        return true;
    }
    if (AttrName == TEXT("recent_presence"))
    {
        OutAttrValue = PresenceStr;
        return true;
    }
    if (AttrName == TEXT("recent_encounter"))
    {
        OutAttrValue = EncounterTypeToString(EncounterType);
        return true;
    }
    return false;
}
FOnlineFriendsSimplyStream::FOnlineFriendsSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
    : Subsystem(InSubsystem)
{
}

FOnlineFriendsSimplyStream::~FOnlineFriendsSimplyStream()
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (IdentityInt.IsValid())
    {
        IdentityInt->ClearOnLoginChangedDelegate_Handle(OnLoginChangedHandle);
    }
}

bool FOnlineFriendsSimplyStream::Init()
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (IdentityInt.IsValid())
    {
        OnLoginChangedHandle = IdentityInt->AddOnLoginChangedDelegate_Handle(FOnLoginChangedDelegate::CreateThreadSafeSP(this, &FOnlineFriendsSimplyStream::OnLoginChanged));
        return true;
    }
    return false;
}

void FOnlineFriendsSimplyStream::OnLoginChanged(int32 LocalUserNum)
{
	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (IdentityInt.IsValid())
	{
        ELoginStatus::Type LoginStatus = IdentityInt->GetLoginStatus(LocalUserNum);
		if (LoginStatus == ELoginStatus::NotLoggedIn)
		{
			UnregisterFriendsForPresence(LocalUserNum);
			FriendsLists.Remove(LocalUserNum);
			BlockedPlayers.Remove(LocalUserNum);
		}
	}
}

bool FOnlineFriendsSimplyStream::ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate)
{
    FSimplyStreamFriendsQueryOptions ParsedOptions;
    FString BaseListName = ListName;
    FString RawQuery;
    if (ListName.Split(TEXT("?"), &BaseListName, &RawQuery))
    {
        TArray<FString> Pairs;
        RawQuery.ParseIntoArray(Pairs, TEXT("&"), true);
        for (const FString& Pair : Pairs)
        {
            FString Key;
            FString Value;
            if (!Pair.Split(TEXT("="), &Key, &Value))
            {
                Key = Pair;
                Value.Reset();
            }

            if (Key.Equals(TEXT("status"), ESearchCase::IgnoreCase))
            {
                ParsedOptions.Status = Value;
            }
            else if (Key.Equals(TEXT("search"), ESearchCase::IgnoreCase))
            {
                ParsedOptions.Search = Value;
            }
            else if (Key.Equals(TEXT("includepresence"), ESearchCase::IgnoreCase))
            {
                ParsedOptions.IncludePresence = Value.Equals(TEXT("1")) || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase);
            }
            else if (Key.Equals(TEXT("limit"), ESearchCase::IgnoreCase))
            {
                ParsedOptions.Limit = FCString::Atoi(*Value);
            }
        }
    }

    return ReadFriendsList(LocalUserNum, BaseListName, ParsedOptions, Delegate);
}

bool FOnlineFriendsSimplyStream::ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FSimplyStreamFriendsQueryOptions& Options, const FOnReadFriendsListComplete& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid() || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("ReadFriendsList: User not logged in"));
        Subsystem->ExecuteNextTick([Delegate, LocalUserNum, ListName]()
        {
            Delegate.ExecuteIfBound(LocalUserNum, false, ListName, TEXT("User not logged in"));
        });
        return false;
    }

    FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("ReadFriendsList: No auth token"));
        Subsystem->ExecuteNextTick([Delegate, LocalUserNum, ListName]()
        {
            Delegate.ExecuteIfBound(LocalUserNum, false, ListName, TEXT("No auth token"));
        });
        return false;
    }

	const FString NormalizedListName = NormalizeListName(ListName);
	const FString StatusParam = Options.Status.IsSet() && !Options.Status.GetValue().IsEmpty()
		? Options.Status.GetValue()
		: StatusParamForList(NormalizedListName);
	const bool bIncludePresence = Options.IncludePresence.IsSet()
		? Options.IncludePresence.GetValue()
		: ShouldIncludePresence(NormalizedListName);
	const int32 MaxResults = Options.Limit.IsSet() ? Options.Limit.GetValue() : (bIncludePresence ? 200 : 100);
	const FString SearchParam = Options.Search.IsSet() ? Options.Search.GetValue() : FString();

	FString LocalUserIdStr;
	if (FUniqueNetIdPtr LocalUserId = IdentityInt->GetUniquePlayerId(LocalUserNum))
	{
		LocalUserIdStr = LocalUserId->ToString();
	}

	TArray<FString> QueryParams;
	QueryParams.Add(FString::Printf(TEXT("status=%s"), *FGenericPlatformHttp::UrlEncode(StatusParam)));
	QueryParams.Add(FString::Printf(TEXT("includePresence=%s"), bIncludePresence ? TEXT("true") : TEXT("false")));
	QueryParams.Add(FString::Printf(TEXT("limit=%d"), MaxResults));
	if (!SearchParam.IsEmpty())
	{
		QueryParams.Add(FString::Printf(TEXT("search=%s"), *FGenericPlatformHttp::UrlEncode(SearchParam)));
	}

	const FString QueryString = FString::Printf(TEXT("?%s"), *FString::Join(QueryParams, TEXT("&")));

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/friends") + QueryString);
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineFriendsSimplyStream::OnReadFriendsListComplete, LocalUserNum, ListName, LocalUserIdStr, Delegate);
    
    if (HttpRequest->ProcessRequest())
    {
        UE_LOG_ONLINE_FRIEND(Verbose, TEXT("ReadFriendsList: HTTP request sent"));
        return true;
    }
    else
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("ReadFriendsList: Failed to send HTTP request"));
        Subsystem->ExecuteNextTick([Delegate, LocalUserNum, ListName]()
        {
            Delegate.ExecuteIfBound(LocalUserNum, false, ListName, TEXT("Failed to send request"));
        });
        return false;
    }
}

void FOnlineFriendsSimplyStream::OnReadFriendsListComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FString ListName, FString LocalUserIdStr, FOnReadFriendsListComplete Delegate)
{
    bool bSuccess = false;
    FString ErrorStr;
	const FString NormalizedListName = NormalizeListName(ListName);

    if (bWasSuccessful && Response.IsValid())
    {
        int32 ResponseCode = Response->GetResponseCode();
        FString ResponseContent = Response->GetContentAsString();

        if (ResponseCode == 200)
        {
            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

            if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
            {
                bool bApiSuccess = JsonObject->GetBoolField(TEXT("success"));
                if (bApiSuccess)
                {
                    TMap<FString, TArray<TSharedRef<FOnlineFriend>>>& ListMap = FriendsLists.FindOrAdd(LocalUserNum);
                    TArray<TSharedRef<FOnlineFriend>>& UserFriendsList = ListMap.FindOrAdd(NormalizedListName);
                    UserFriendsList.Empty();

                    const TArray<TSharedPtr<FJsonValue>>* FriendsArray;
                    if (JsonObject->TryGetArrayField(TEXT("results"), FriendsArray))
                    {
                        for (const TSharedPtr<FJsonValue>& FriendValue : *FriendsArray)
                        {
                            TSharedPtr<FJsonObject> FriendObject = FriendValue->AsObject();
                            if (FriendObject.IsValid())
                            {
                                FString FriendshipId = FriendObject->GetStringField(TEXT("friendshipId"));
                                FString FriendUserId = FriendObject->GetStringField(TEXT("userId"));
                                FString Username = FriendObject->GetStringField(TEXT("username"));
                                FString DisplayName = FriendObject->GetStringField(TEXT("displayName"));
                                FString Platform = FriendObject->GetStringField(TEXT("platform"));
                                FString AvatarUrl;
                                FriendObject->TryGetStringField(TEXT("avatarUrl"), AvatarUrl);

                                TSharedRef<FOnlineFriendSimplyStream> NewFriend = MakeShared<FOnlineFriendSimplyStream>(FriendshipId, FriendUserId, Username, DisplayName, Platform, AvatarUrl);

                                FString Status = FriendObject->GetStringField(TEXT("status"));
                                if (Status.Equals(TEXT("pending"), ESearchCase::IgnoreCase))
                                {
                                    FString InitiatedBy = FriendObject->GetStringField(TEXT("initiatedBy"));
                                    if (InitiatedBy == LocalUserIdStr)
                                    {
                                        NewFriend->SetInviteStatus(EInviteStatus::PendingOutbound);
                                    }
                                    else
                                    {
                                        NewFriend->SetInviteStatus(EInviteStatus::PendingInbound);
                                    }
                                }
                                else if (Status.Equals(TEXT("blocked"), ESearchCase::IgnoreCase))
                                {
                                    NewFriend->SetInviteStatus(EInviteStatus::Blocked);
                                }
                                else
                                {
                                    NewFriend->SetInviteStatus(EInviteStatus::Accepted);
                                }

                                const TSharedPtr<FJsonObject>* PresenceObject = nullptr;
                                if (FriendObject->TryGetObjectField(TEXT("presence"), PresenceObject) && PresenceObject && (*PresenceObject).IsValid())
                                {
                                    FOnlineUserPresence PresenceData;
                                    FString PresenceStateStr;
                                    if ((*PresenceObject)->TryGetStringField(TEXT("status"), PresenceStateStr))
                                    {
                                        PresenceData.Status.State = PresenceStateFromString(PresenceStateStr);
                                    }
                                    FString StatusMessage;
                                    if ((*PresenceObject)->TryGetStringField(TEXT("statusMessage"), StatusMessage))
                                    {
                                        PresenceData.Status.StatusStr = StatusMessage;
                                    }
                                    FString SessionIdStr;
                                    if ((*PresenceObject)->TryGetStringField(TEXT("currentSessionId"), SessionIdStr) && !SessionIdStr.IsEmpty())
                                    {
                                        PresenceData.SessionId = FUniqueNetIdSimplyStream::Create(SessionIdStr);
                                    }
                                    FString MatchIdStr;
                                    if ((*PresenceObject)->TryGetStringField(TEXT("currentMatchId"), MatchIdStr) && !MatchIdStr.IsEmpty())
                                    {
                                        PresenceData.Status.Properties.Add(TEXT("currentMatchId"), FVariantData(MatchIdStr));
                                    }
                                    bool bIsJoinable = false;
                                    (*PresenceObject)->TryGetBoolField(TEXT("isJoinable"), bIsJoinable);
                                    PresenceData.bIsPlayingThisGame = bIsJoinable;
                                    NewFriend->SetPresence(PresenceData);
                                }

                                UserFriendsList.Add(NewFriend);
                            }
                        }
                    }

                    bSuccess = true;
                    UE_LOG_ONLINE_FRIEND(Verbose, TEXT("ReadFriendsList: Retrieved %d friends"), UserFriendsList.Num());
                }
                else
                {
                    ErrorStr = JsonObject->GetStringField(TEXT("error"));
                }
            }
            else
            {
                ErrorStr = TEXT("Failed to parse response JSON");
            }
        }
        else
        {
            ErrorStr = FString::Printf(TEXT("HTTP error %d"), ResponseCode);
        }
    }
    else
    {
        ErrorStr = TEXT("HTTP request failed");
    }

    if (!bSuccess)
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("ReadFriendsList failed: %s"), *ErrorStr);
    }

    Delegate.ExecuteIfBound(LocalUserNum, bSuccess, ListName, ErrorStr);
    
    if (bSuccess)
    {
        TriggerOnFriendsChangeDelegates(LocalUserNum);
        RegisterFriendsForPresence(LocalUserNum);
    }
}

bool FOnlineFriendsSimplyStream::DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate)
{
    const FString NormalizedListName = NormalizeListName(ListName);
    if (TMap<FString, TArray<TSharedRef<FOnlineFriend>>>* ListMap = FriendsLists.Find(LocalUserNum))
    {
        ListMap->Remove(NormalizedListName);
        if (ListMap->Num() == 0)
        {
            FriendsLists.Remove(LocalUserNum);
        }
    }

    Subsystem->ExecuteNextTick([Delegate, LocalUserNum, ListName]()
    {
        Delegate.ExecuteIfBound(LocalUserNum, true, ListName, FString());
    });
    return true;
}

bool FOnlineFriendsSimplyStream::SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid() || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("SendInvite: User not logged in"));
        Subsystem->ExecuteNextTick([Delegate, LocalUserNum, FriendId = FriendId.AsShared(), ListName]()
        {
            Delegate.ExecuteIfBound(LocalUserNum, false, *FriendId, ListName, TEXT("User not logged in"));
        });
        return false;
    }

    FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("SendInvite: No auth token"));
        Subsystem->ExecuteNextTick([Delegate, LocalUserNum, FriendId = FriendId.AsShared(), ListName]()
        {
            Delegate.ExecuteIfBound(LocalUserNum, false, *FriendId, ListName, TEXT("No auth token"));
        });
        return false;
    }

    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("friendUserId"), FriendId.ToString());

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/friends/request"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineFriendsSimplyStream::OnSendInviteComplete, LocalUserNum, FriendId.AsShared(), ListName, Delegate);
    
    return HttpRequest->ProcessRequest();
}

void FOnlineFriendsSimplyStream::OnSendInviteComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef FriendId, FString ListName, FOnSendInviteComplete Delegate)
{
    bool bSuccess = false;
    FString ErrorStr;

    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 201)
    {
        FString ResponseContent = Response->GetContentAsString();
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            bSuccess = JsonObject->GetBoolField(TEXT("success"));
            if (!bSuccess)
            {
                ErrorStr = JsonObject->GetStringField(TEXT("error"));
            }
        }
    }
    else
    {
        ErrorStr = FString::Printf(TEXT("HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }

    UE_LOG_ONLINE_FRIEND(Verbose, TEXT("SendInvite completed: %s"), bSuccess ? TEXT("Success") : *ErrorStr);
    Delegate.ExecuteIfBound(LocalUserNum, bSuccess, *FriendId, ListName, ErrorStr);
}

bool FOnlineFriendsSimplyStream::AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid() || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("AcceptInvite: User not logged in"));
        Subsystem->ExecuteNextTick([Delegate, LocalUserNum, FriendId = FriendId.AsShared(), ListName]()
        {
            Delegate.ExecuteIfBound(LocalUserNum, false, *FriendId, ListName, TEXT("User not logged in"));
        });
        return false;
    }

    FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("AcceptInvite: No auth token"));
        Subsystem->ExecuteNextTick([Delegate, LocalUserNum, FriendId = FriendId.AsShared(), ListName]()
        {
            Delegate.ExecuteIfBound(LocalUserNum, false, *FriendId, ListName, TEXT("No auth token"));
        });
        return false;
    }

    // Note: We need the friendship ID, not the friend's user ID. This would need to be stored somewhere.
    // For now, we'll implement a simple approach that finds the friendship by friend ID
    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/friends/%s/accept"), *FriendId.ToString()));
    HttpRequest->SetVerb(TEXT("PATCH"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineFriendsSimplyStream::OnAcceptInviteComplete, LocalUserNum, FriendId.AsShared(), ListName, Delegate);
    
    return HttpRequest->ProcessRequest();
}

void FOnlineFriendsSimplyStream::OnAcceptInviteComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef FriendId, FString ListName, FOnAcceptInviteComplete Delegate)
{
    bool bSuccess = false;
    FString ErrorStr;

    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        FString ResponseContent = Response->GetContentAsString();
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            bSuccess = JsonObject->GetBoolField(TEXT("success"));
            if (bSuccess)
            {
                // Refresh friends list after accepting invite
                ReadFriendsList(LocalUserNum, ListName, FOnReadFriendsListComplete());
            }
            else
            {
                ErrorStr = JsonObject->GetStringField(TEXT("error"));
            }
        }
    }
    else
    {
        ErrorStr = FString::Printf(TEXT("HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }

    UE_LOG_ONLINE_FRIEND(Verbose, TEXT("AcceptInvite completed: %s"), bSuccess ? TEXT("Success") : *ErrorStr);
    Delegate.ExecuteIfBound(LocalUserNum, bSuccess, *FriendId, ListName, ErrorStr);
}

bool FOnlineFriendsSimplyStream::RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid() || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("RejectInvite: User not logged in"));
        Subsystem->ExecuteNextTick([this, LocalUserNum, FriendId = FriendId.AsShared(), ListName]()
        {
            TriggerOnRejectInviteCompleteDelegates(LocalUserNum, false, *FriendId, ListName, TEXT("User not logged in"));
        });
        return false;
    }

    FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("RejectInvite: No auth token"));
        Subsystem->ExecuteNextTick([this, LocalUserNum, FriendId = FriendId.AsShared(), ListName]()
        {
            TriggerOnRejectInviteCompleteDelegates(LocalUserNum, false, *FriendId, ListName, TEXT("No auth token"));
        });
        return false;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/friends/%s/reject"), *FriendId.ToString()));
    HttpRequest->SetVerb(TEXT("DELETE"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineFriendsSimplyStream::OnRejectInviteComplete, LocalUserNum, FriendId.AsShared(), ListName);
    
    return HttpRequest->ProcessRequest();
}

void FOnlineFriendsSimplyStream::OnRejectInviteComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef FriendId, FString ListName)
{
    bool bSuccess = false;
    FString ErrorStr;

    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        FString ResponseContent = Response->GetContentAsString();
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            bSuccess = JsonObject->GetBoolField(TEXT("success"));
            if (!bSuccess)
            {
                ErrorStr = JsonObject->GetStringField(TEXT("error"));
            }
        }
    }
    else
    {
        ErrorStr = FString::Printf(TEXT("HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }

    UE_LOG_ONLINE_FRIEND(Verbose, TEXT("RejectInvite completed: %s"), bSuccess ? TEXT("Success") : *ErrorStr);
    TriggerOnRejectInviteCompleteDelegates(LocalUserNum, bSuccess, *FriendId, ListName, ErrorStr);
}

bool FOnlineFriendsSimplyStream::DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid() || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("DeleteFriend: User not logged in"));
        Subsystem->ExecuteNextTick([this, LocalUserNum, FriendId = FriendId.AsShared(), ListName]()
        {
            TriggerOnDeleteFriendCompleteDelegates(LocalUserNum, false, *FriendId, ListName, TEXT("User not logged in"));
        });
        return false;
    }

    FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("DeleteFriend: No auth token"));
        Subsystem->ExecuteNextTick([this, LocalUserNum, FriendId = FriendId.AsShared(), ListName]()
        {
            TriggerOnDeleteFriendCompleteDelegates(LocalUserNum, false, *FriendId, ListName, TEXT("No auth token"));
        });
        return false;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/friends/%s"), *FriendId.ToString()));
    HttpRequest->SetVerb(TEXT("DELETE"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineFriendsSimplyStream::OnDeleteFriendComplete, LocalUserNum, FriendId.AsShared(), ListName);
    
    return HttpRequest->ProcessRequest();
}

void FOnlineFriendsSimplyStream::OnDeleteFriendComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef FriendId, FString ListName)
{
    bool bSuccess = false;
    FString ErrorStr;

    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        FString ResponseContent = Response->GetContentAsString();
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            bSuccess = JsonObject->GetBoolField(TEXT("success"));
            if (bSuccess)
            {
                if (TMap<FString, TArray<TSharedRef<FOnlineFriend>>>* ListMap = FriendsLists.Find(LocalUserNum))
                {
                    for (TPair<FString, TArray<TSharedRef<FOnlineFriend>>>& Pair : *ListMap)
                    {
                        Pair.Value.RemoveAll([&FriendId](const TSharedRef<FOnlineFriend>& Friend)
                        {
                            return *Friend->GetUserId() == *FriendId;
                        });
                    }
                }
            }
            else
            {
                ErrorStr = JsonObject->GetStringField(TEXT("error"));
            }
        }
    }
    else
    {
        ErrorStr = FString::Printf(TEXT("HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }

    UE_LOG_ONLINE_FRIEND(Verbose, TEXT("DeleteFriend completed: %s"), bSuccess ? TEXT("Success") : *ErrorStr);
    TriggerOnDeleteFriendCompleteDelegates(LocalUserNum, bSuccess, *FriendId, ListName, ErrorStr);
    
    if (bSuccess)
    {
        TriggerOnFriendsChangeDelegates(LocalUserNum);
    }
}

bool FOnlineFriendsSimplyStream::GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray<TSharedRef<FOnlineFriend>>& OutFriends)
{
	const FString NormalizedListName = NormalizeListName(ListName);
	if (TMap<FString, TArray<TSharedRef<FOnlineFriend>>>* ListMap = FriendsLists.Find(LocalUserNum))
	{
		if (TArray<TSharedRef<FOnlineFriend>>* Friends = ListMap->Find(NormalizedListName))
		{
			OutFriends = *Friends;
			return true;
		}
	}

	OutFriends.Empty();
	return false;
}

TSharedPtr<FOnlineFriend> FOnlineFriendsSimplyStream::GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	const FString NormalizedListName = NormalizeListName(ListName);
	if (TMap<FString, TArray<TSharedRef<FOnlineFriend>>>* ListMap = FriendsLists.Find(LocalUserNum))
	{
		if (TArray<TSharedRef<FOnlineFriend>>* Friends = ListMap->Find(NormalizedListName))
		{
			for (const TSharedRef<FOnlineFriend>& Friend : *Friends)
			{
				if (*Friend->GetUserId() == FriendId)
				{
					return Friend;
				}
			}
		}
	}
	return nullptr;
}

bool FOnlineFriendsSimplyStream::IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
    TSharedPtr<FOnlineFriend> Friend = GetFriend(LocalUserNum, FriendId, ListName);
    return Friend.IsValid() && Friend->GetInviteStatus() == EInviteStatus::Accepted;
}

void FOnlineFriendsSimplyStream::AddRecentPlayers(const FUniqueNetId& UserId, const TArray<FReportPlayedWithUser>& InRecentPlayers, const FString& Namespace, const FOnAddRecentPlayersComplete& InCompletionDelegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        FOnlineError Error;
        Error.SetFromErrorCode(TEXT("invalid_identity"));
        Subsystem->ExecuteNextTick([InCompletionDelegate, UserId = UserId.AsShared(), Error]()
        {
            InCompletionDelegate.ExecuteIfBound(*UserId, Error);
        });
        return;
    }

    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
    if (LocalUserNum == INDEX_NONE || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        FOnlineError Error;
        Error.SetFromErrorCode(TEXT("not_logged_in"));
        Subsystem->ExecuteNextTick([InCompletionDelegate, UserId = UserId.AsShared(), Error]()
        {
            InCompletionDelegate.ExecuteIfBound(*UserId, Error);
        });
        return;
    }

    FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        FOnlineError Error;
        Error.SetFromErrorCode(TEXT("missing_auth_token"));
        Subsystem->ExecuteNextTick([InCompletionDelegate, UserId = UserId.AsShared(), Error]()
        {
            InCompletionDelegate.ExecuteIfBound(*UserId, Error);
        });
        return;
    }

    if (InRecentPlayers.Num() == 0)
    {
        FOnlineError Success = FOnlineError::Success();
        Subsystem->ExecuteNextTick([InCompletionDelegate, UserId = UserId.AsShared(), Success]()
        {
            InCompletionDelegate.ExecuteIfBound(*UserId, Success);
        });
        return;
    }

    TArray<TSharedPtr<FJsonValue>> RecentPlayersArray;
    for (const FReportPlayedWithUser& Report : InRecentPlayers)
    {
        if (!Report.UserId->IsValid())
        {
            continue;
        }

        TSharedPtr<FJsonObject> PlayerJson = MakeShared<FJsonObject>();
        PlayerJson->SetStringField(TEXT("userId"), Report.UserId->ToString());
        if (!Report.PresenceStr.IsEmpty())
        {
            PlayerJson->SetStringField(TEXT("presenceStr"), Report.PresenceStr);
        }
        PlayerJson->SetStringField(TEXT("encounterType"), EncounterTypeToString(Report.EncounterType));
        RecentPlayersArray.Add(MakeShared<FJsonValueObject>(PlayerJson));
    }

    if (RecentPlayersArray.Num() == 0)
    {
        FOnlineError Success = FOnlineError::Success();
        Subsystem->ExecuteNextTick([InCompletionDelegate, UserId = UserId.AsShared(), Success]()
        {
            InCompletionDelegate.ExecuteIfBound(*UserId, Success);
        });
        return;
    }

    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetArrayField(TEXT("recentPlayers"), RecentPlayersArray);

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/recent"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineFriendsSimplyStream::OnAddRecentPlayersComplete, UserId.AsShared(), Namespace, InCompletionDelegate);

    if (!HttpRequest->ProcessRequest())
    {
        FOnlineError Error;
        Error.SetFromErrorCode(TEXT("http_request_failed"));
        Subsystem->ExecuteNextTick([InCompletionDelegate, UserId = UserId.AsShared(), Error]()
        {
            InCompletionDelegate.ExecuteIfBound(*UserId, Error);
        });
    }
}

bool FOnlineFriendsSimplyStream::QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        Subsystem->ExecuteNextTick([this, UserId = UserId.AsShared(), Namespace]()
        {
            TriggerOnQueryRecentPlayersCompleteDelegates(*UserId, Namespace, false, TEXT("Identity interface unavailable"));
        });
        return false;
    }

    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
    if (LocalUserNum == INDEX_NONE || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        Subsystem->ExecuteNextTick([this, UserId = UserId.AsShared(), Namespace]()
        {
            TriggerOnQueryRecentPlayersCompleteDelegates(*UserId, Namespace, false, TEXT("User not logged in"));
        });
        return false;
    }

    FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        Subsystem->ExecuteNextTick([this, UserId = UserId.AsShared(), Namespace]()
        {
            TriggerOnQueryRecentPlayersCompleteDelegates(*UserId, Namespace, false, TEXT("Missing auth token"));
        });
        return false;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/recent?limit=50"));
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineFriendsSimplyStream::OnQueryRecentPlayersComplete, LocalUserNum, UserId.AsShared(), Namespace);

    if (!HttpRequest->ProcessRequest())
    {
        Subsystem->ExecuteNextTick([this, UserId = UserId.AsShared(), Namespace]()
        {
            TriggerOnQueryRecentPlayersCompleteDelegates(*UserId, Namespace, false, TEXT("Failed to send request"));
        });
        return false;
    }

    return true;
}

bool FOnlineFriendsSimplyStream::GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray<TSharedRef<FOnlineRecentPlayer>>& OutRecentPlayers)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        OutRecentPlayers.Empty();
        return false;
    }

    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
    if (const TMap<FString, TArray<TSharedRef<FOnlineRecentPlayer>>>* NamespaceMap = RecentPlayers.Find(LocalUserNum))
    {
        if (const TArray<TSharedRef<FOnlineRecentPlayer>>* Players = NamespaceMap->Find(Namespace))
        {
            OutRecentPlayers = *Players;
            return true;
        }
    }

    OutRecentPlayers.Empty();
    return false;
}

void FOnlineFriendsSimplyStream::DumpRecentPlayers() const
{
    UE_LOG_ONLINE_FRIEND(Display, TEXT("Recent Players:"));
    for (const TPair<int32, TMap<FString, TArray<TSharedRef<FOnlineRecentPlayer>>>>& LocalUserEntry : RecentPlayers)
    {
        for (const TPair<FString, TArray<TSharedRef<FOnlineRecentPlayer>>>& NamespaceEntry : LocalUserEntry.Value)
        {
            UE_LOG_ONLINE_FRIEND(Display, TEXT("  LocalUser %d Namespace %s (%d entries)"),
                LocalUserEntry.Key,
                *NamespaceEntry.Key,
                NamespaceEntry.Value.Num());
            for (const TSharedRef<FOnlineRecentPlayer>& Player : NamespaceEntry.Value)
            {
                UE_LOG_ONLINE_FRIEND(Display, TEXT("    %s (%s) LastSeen=%s"),
                    *Player->GetUserId()->ToString(),
                    *Player->GetDisplayName(Player->GetUserId()->ToString()),
                    *Player->GetLastSeen().ToString());
            }
        }
    }
}

bool FOnlineFriendsSimplyStream::BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid() || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("BlockPlayer: User not logged in"));
        Subsystem->ExecuteNextTick([this, LocalUserNum, PlayerId = PlayerId.AsShared()]()
        {
            TriggerOnBlockedPlayerCompleteDelegates(LocalUserNum, false, *PlayerId, TEXT("blocked"), TEXT("User not logged in"));
        });
        return false;
    }

    FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("BlockPlayer: No auth token"));
        Subsystem->ExecuteNextTick([this, LocalUserNum, PlayerId = PlayerId.AsShared()]()
        {
            TriggerOnBlockedPlayerCompleteDelegates(LocalUserNum, false, *PlayerId, TEXT("blocked"), TEXT("No auth token"));
        });
        return false;
    }

    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("blockedUserId"), PlayerId.ToString());

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/blocked"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineFriendsSimplyStream::OnBlockPlayerComplete, LocalUserNum, PlayerId.AsShared());
    
    return HttpRequest->ProcessRequest();
}

void FOnlineFriendsSimplyStream::OnBlockPlayerComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef PlayerId)
{
    bool bSuccess = false;
    FString ErrorStr;

    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 201)
    {
        FString ResponseContent = Response->GetContentAsString();
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            bSuccess = JsonObject->GetBoolField(TEXT("success"));
            if (!bSuccess)
            {
                ErrorStr = JsonObject->GetStringField(TEXT("error"));
            }
        }
    }
    else
    {
        ErrorStr = FString::Printf(TEXT("HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }

    UE_LOG_ONLINE_FRIEND(Verbose, TEXT("BlockPlayer completed: %s"), bSuccess ? TEXT("Success") : *ErrorStr);

    if (bSuccess)
    {
        if (FOnlinePresenceSimplyStreamPtr Presence = StaticCastSharedPtr<FOnlinePresenceSimplyStream>(Subsystem->GetPresenceInterface()))
        {
            Presence->UntrackRemoteUser(*PlayerId);
        }

        IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
        if (IdentityInt.IsValid())
        {
            FUniqueNetIdPtr LocalUserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
            if (LocalUserId.IsValid())
            {
                QueryBlockedPlayers(*LocalUserId);
            }
        }
    }

    TriggerOnBlockedPlayerCompleteDelegates(LocalUserNum, bSuccess, *PlayerId, TEXT("blocked"), ErrorStr);
}

void FOnlineFriendsSimplyStream::OnUnblockPlayerComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef PlayerId)
{
    bool bSuccess = false;
    FString ErrorStr;

    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        TSharedPtr<FJsonObject> JsonObject;
        const FString Content = Response->GetContentAsString();
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            bSuccess = JsonObject->GetBoolField(TEXT("success"));
            if (!bSuccess)
            {
                JsonObject->TryGetStringField(TEXT("error"), ErrorStr);
            }
        }
    }
    else
    {
        ErrorStr = FString::Printf(TEXT("HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }

	if (bSuccess)
	{
		if (TArray<TSharedRef<FOnlineBlockedPlayer>>* UserBlockedList = BlockedPlayers.Find(LocalUserNum))
		{
			UserBlockedList->RemoveAll([&PlayerId](const TSharedRef<FOnlineBlockedPlayer>& Entry)
			{
				return *Entry->GetUserId() == *PlayerId;
			});
		}

		if (FOnlinePresenceSimplyStreamPtr Presence = StaticCastSharedPtr<FOnlinePresenceSimplyStream>(Subsystem->GetPresenceInterface()))
		{
			Presence->TrackRemoteUser(*PlayerId);
		}

		RegisterFriendsForPresence(LocalUserNum);
	}

	UE_LOG_ONLINE_FRIEND(Verbose, TEXT("UnblockPlayer completed: %s"), bSuccess ? TEXT("Success") : *ErrorStr);
    TriggerOnUnblockedPlayerCompleteDelegates(LocalUserNum, bSuccess, *PlayerId, TEXT("blocked"), ErrorStr);
}

void FOnlineFriendsSimplyStream::OnQueryBlockedPlayersComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef UserId)
{
    bool bSuccess = false;
    FString ErrorStr;

    if (bWasSuccessful && Response.IsValid())
    {
        const int32 ResponseCode = Response->GetResponseCode();
        const FString Content = Response->GetContentAsString();
        if (ResponseCode == 200)
        {
            TSharedPtr<FJsonObject> JsonObject;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
            if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
            {
                bool bApiSuccess = false;
                JsonObject->TryGetBoolField(TEXT("success"), bApiSuccess);
                if (bApiSuccess)
                {
                    TArray<TSharedRef<FOnlineBlockedPlayer>>& UserBlockedList = BlockedPlayers.FindOrAdd(LocalUserNum);
                    UserBlockedList.Empty();

                    const TArray<TSharedPtr<FJsonValue>>* ResultsArray = nullptr;
                    if (JsonObject->TryGetArrayField(TEXT("results"), ResultsArray) && ResultsArray != nullptr)
                    {
                        for (const TSharedPtr<FJsonValue>& ResultValue : *ResultsArray)
                        {
                            const TSharedPtr<FJsonObject>* ResultObject = nullptr;
                            if (ResultValue.IsValid() && ResultValue->TryGetObject(ResultObject) && ResultObject != nullptr)
                            {
                                const FString BlockedUserId = (*ResultObject)->GetStringField(TEXT("userId"));
                                const FString Username = (*ResultObject)->GetStringField(TEXT("username"));
                                const FString DisplayName = (*ResultObject)->GetStringField(TEXT("displayName"));

                                UserBlockedList.Add(MakeShared<FOnlineBlockedPlayerSimplyStream>(BlockedUserId, Username, DisplayName));
                            }
                        }
                    }

                    bSuccess = true;
	UE_LOG_ONLINE_FRIEND(Verbose, TEXT("QueryBlockedPlayers: Retrieved %d blocked users"), UserBlockedList.Num());
}
                else
                {
                    JsonObject->TryGetStringField(TEXT("error"), ErrorStr);
                }
            }
            else
            {
                ErrorStr = TEXT("Failed to parse response JSON");
            }
        }
        else
        {
            ErrorStr = FString::Printf(TEXT("HTTP error %d"), ResponseCode);
        }
    }
    else
    {
        ErrorStr = TEXT("HTTP request failed");
    }

	TriggerOnQueryBlockedPlayersCompleteDelegates(*UserId, bSuccess, ErrorStr);
	if (bSuccess)
	{
		RegisterFriendsForPresence(LocalUserNum);
	}
}

void FOnlineFriendsSimplyStream::OnAddRecentPlayersComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString Namespace, FOnAddRecentPlayersComplete Delegate)
{
	FOnlineError ResultError;
	bool bSuccess = false;

	if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		TSharedPtr<FJsonObject> JsonObject;
		const FString Content = Response->GetContentAsString();
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			JsonObject->TryGetBoolField(TEXT("success"), bSuccess);
			if (!bSuccess)
			{
				FString ErrorStr;
				if (JsonObject->TryGetStringField(TEXT("error"), ErrorStr))
				{
					ResultError.SetFromErrorCode(ErrorStr);
				}
			}
		}
	}

	if (!bSuccess)
	{
		if (!ResultError.WasSuccessful())
		{
			ResultError.SetFromErrorCode(TEXT("recent_players_failed"));
		}
	}
	else
	{
		ResultError = FOnlineError::Success();
		QueryRecentPlayers(*UserId, Namespace);
	}

	Subsystem->ExecuteNextTick([Delegate, UserId, ResultError]()
	{
		Delegate.ExecuteIfBound(*UserId, ResultError);
	});
}

void FOnlineFriendsSimplyStream::OnQueryRecentPlayersComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FUniqueNetIdRef UserId, FString Namespace)
{
	bool bSuccess = false;
	FString ErrorStr;
	TArray<TSharedRef<FOnlineRecentPlayer>> RecentList;

	if (bWasSuccessful && Response.IsValid())
	{
		const int32 ResponseCode = Response->GetResponseCode();
		const FString Content = Response->GetContentAsString();
		if (ResponseCode == EHttpResponseCodes::Ok)
		{
			TSharedPtr<FJsonObject> JsonObject;
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
							if (!ResultValue.IsValid() || !ResultValue->TryGetObject(ResultObject) || ResultObject == nullptr)
							{
								continue;
							}

							FString RecentUserId;
							if (!(*ResultObject)->TryGetStringField(TEXT("userId"), RecentUserId))
							{
								continue;
							}

							FString Username;
							(*ResultObject)->TryGetStringField(TEXT("username"), Username);

							FString DisplayName;
							(*ResultObject)->TryGetStringField(TEXT("displayName"), DisplayName);

							FString Platform;
							(*ResultObject)->TryGetStringField(TEXT("platform"), Platform);

							FString LastSeenStr;
							(*ResultObject)->TryGetStringField(TEXT("lastSeenAt"), LastSeenStr);

							FDateTime LastSeen = FDateTime::UtcNow();
							FDateTime::ParseIso8601(*LastSeenStr, LastSeen);

							FString PresenceStr;
							FString EncounterTypeStr;
							const TSharedPtr<FJsonObject>* MetadataObject = nullptr;
							if ((*ResultObject)->TryGetObjectField(TEXT("metadata"), MetadataObject) && MetadataObject && (*MetadataObject).IsValid())
							{
								(*MetadataObject)->TryGetStringField(TEXT("presenceStr"), PresenceStr);
								(*MetadataObject)->TryGetStringField(TEXT("encounterType"), EncounterTypeStr);
							}

							const ERecentPlayerEncounterType EncounterType = EncounterTypeFromString(EncounterTypeStr);

							TSharedRef<FOnlineRecentPlayerSimplyStream> RecentPlayer = MakeShared<FOnlineRecentPlayerSimplyStream>(
								RecentUserId,
								Username,
								DisplayName,
								Platform,
								LastSeen,
								PresenceStr,
								EncounterType);
							RecentList.Add(RecentPlayer);
						}
					}

					bSuccess = true;
				}
				else
				{
					JsonObject->TryGetStringField(TEXT("error"), ErrorStr);
				}
			}
		}
		else
		{
			ErrorStr = FString::Printf(TEXT("HTTP error %d"), ResponseCode);
		}
	}
	else
	{
		ErrorStr = TEXT("HTTP request failed");
	}

	if (bSuccess)
	{
		FOnlinePresenceSimplyStreamPtr PresenceInt = StaticCastSharedPtr<FOnlinePresenceSimplyStream>(Subsystem->GetPresenceInterface());

		if (PresenceInt.IsValid())
		{
			if (const TMap<FString, TArray<TSharedRef<FOnlineRecentPlayer>>>* ExistingNamespaceMap = RecentPlayers.Find(LocalUserNum))
			{
				if (const TArray<TSharedRef<FOnlineRecentPlayer>>* ExistingList = ExistingNamespaceMap->Find(Namespace))
				{
					for (const TSharedRef<FOnlineRecentPlayer>& ExistingPlayer : *ExistingList)
					{
						PresenceInt->UntrackRemoteUser(*ExistingPlayer->GetUserId());
					}
				}
			}
		}

		TMap<FString, TArray<TSharedRef<FOnlineRecentPlayer>>>& NamespaceMap = RecentPlayers.FindOrAdd(LocalUserNum);
		NamespaceMap.Add(Namespace, RecentList);

		if (PresenceInt.IsValid())
		{
			for (const TSharedRef<FOnlineRecentPlayer>& RecentPlayer : RecentList)
			{
				PresenceInt->TrackRemoteUser(*RecentPlayer->GetUserId());
			}
		}

		TriggerOnRecentPlayersAddedDelegates(*UserId, RecentList);
	}

	TriggerOnQueryRecentPlayersCompleteDelegates(*UserId, Namespace, bSuccess, ErrorStr);
}

bool FOnlineFriendsSimplyStream::UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid() || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("UnblockPlayer: User not logged in"));
        Subsystem->ExecuteNextTick([this, LocalUserNum, PlayerId = PlayerId.AsShared()]()
        {
            TriggerOnUnblockedPlayerCompleteDelegates(LocalUserNum, false, *PlayerId, TEXT("blocked"), TEXT("User not logged in"));
        });
        return false;
    }

    FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("UnblockPlayer: No auth token"));
        Subsystem->ExecuteNextTick([this, LocalUserNum, PlayerId = PlayerId.AsShared()]()
        {
            TriggerOnUnblockedPlayerCompleteDelegates(LocalUserNum, false, *PlayerId, TEXT("blocked"), TEXT("No auth token"));
        });
        return false;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/blocked/") + PlayerId.ToString());
    HttpRequest->SetVerb(TEXT("DELETE"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineFriendsSimplyStream::OnUnblockPlayerComplete, LocalUserNum, PlayerId.AsShared());

    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("UnblockPlayer: Failed to send HTTP request"));
        Subsystem->ExecuteNextTick([this, LocalUserNum, PlayerId = PlayerId.AsShared()]()
        {
            TriggerOnUnblockedPlayerCompleteDelegates(LocalUserNum, false, *PlayerId, TEXT("blocked"), TEXT("Failed to send request"));
        });
        return false;
    }

    return true;
}

bool FOnlineFriendsSimplyStream::QueryBlockedPlayers(const FUniqueNetId& UserId)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("QueryBlockedPlayers: No identity interface"));
        Subsystem->ExecuteNextTick([this, UserId = UserId.AsShared()]()
        {
            TriggerOnQueryBlockedPlayersCompleteDelegates(*UserId, false, TEXT("Identity interface unavailable"));
        });
        return false;
    }

    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
    if (LocalUserNum == INDEX_NONE || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("QueryBlockedPlayers: User not logged in"));
        Subsystem->ExecuteNextTick([this, UserId = UserId.AsShared()]()
        {
            TriggerOnQueryBlockedPlayersCompleteDelegates(*UserId, false, TEXT("User not logged in"));
        });
        return false;
    }

    const FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("QueryBlockedPlayers: Missing auth token"));
        Subsystem->ExecuteNextTick([this, UserId = UserId.AsShared()]()
        {
            TriggerOnQueryBlockedPlayersCompleteDelegates(*UserId, false, TEXT("Missing auth token"));
        });
        return false;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/blocked"));
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineFriendsSimplyStream::OnQueryBlockedPlayersComplete, LocalUserNum, UserId.AsShared());

    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("QueryBlockedPlayers: Failed to send HTTP request"));
        Subsystem->ExecuteNextTick([this, UserId = UserId.AsShared()]()
        {
            TriggerOnQueryBlockedPlayersCompleteDelegates(*UserId, false, TEXT("Failed to send request"));
        });
        return false;
    }

    return true;
}

bool FOnlineFriendsSimplyStream::GetBlockedPlayers(const FUniqueNetId& UserId, TArray<TSharedRef<FOnlineBlockedPlayer>>& OutBlockedPlayers)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        OutBlockedPlayers.Empty();
        return false;
    }

    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
    if (BlockedPlayers.Contains(LocalUserNum))
    {
        OutBlockedPlayers = BlockedPlayers[LocalUserNum];
        return true;
    }
    
    OutBlockedPlayers.Empty();
    return false;
}

bool FOnlineFriendsSimplyStream::IsPlayerBlocked(const FUniqueNetId& LocalUserId, const FUniqueNetId& PeerUserId) const
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, LocalUserId);
    if (BlockedPlayers.Contains(LocalUserNum))
    {
        const TArray<TSharedRef<FOnlineBlockedPlayer>>& UserBlockedList = BlockedPlayers[LocalUserNum];
        for (const TSharedRef<FOnlineBlockedPlayer>& BlockedPlayer : UserBlockedList)
        {
            if (*BlockedPlayer->GetUserId() == PeerUserId)
            {
                return true;
            }
        }
    }
    return false;
}

void FOnlineFriendsSimplyStream::SetFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FString& Alias, const FOnSetFriendAliasComplete& Delegate)
{
    // Not implemented
    Subsystem->ExecuteNextTick([Delegate, LocalUserNum, FriendId = FriendId.AsShared(), ListName]()
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("SetFriendAlias is not implemented"));
        Delegate.ExecuteIfBound(LocalUserNum, *FriendId, ListName, FOnlineError(EOnlineErrorResult::NotImplemented));
    });
}

void FOnlineFriendsSimplyStream::DeleteFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnDeleteFriendAliasComplete& Delegate)
{
    // Not implemented
    Subsystem->ExecuteNextTick([Delegate, LocalUserNum, FriendId = FriendId.AsShared(), ListName]()
    {
        UE_LOG_ONLINE_FRIEND(Warning, TEXT("DeleteFriendAlias is not implemented"));
        Delegate.ExecuteIfBound(LocalUserNum, *FriendId, ListName, FOnlineError(EOnlineErrorResult::NotImplemented));
    });
}

void FOnlineFriendsSimplyStream::DumpBlockedPlayers() const
{
	UE_LOG_ONLINE_FRIEND(Display, TEXT("Blocked Players:"));
    for (const TPair<int32, TArray<TSharedRef<FOnlineBlockedPlayer>>>& UserBlockedList : BlockedPlayers)
    {
        UE_LOG_ONLINE_FRIEND(Display, TEXT("  LocalUser %d has %d blocked players"), UserBlockedList.Key, UserBlockedList.Value.Num());
        for (const TSharedRef<FOnlineBlockedPlayer>& BlockedPlayer : UserBlockedList.Value)
        {
            UE_LOG_ONLINE_FRIEND(Display, TEXT("    %s"), *BlockedPlayer->GetUserId()->ToDebugString());
		}
	}
}

void FOnlineFriendsSimplyStream::RegisterFriendsForPresence(int32 LocalUserNum)
{
	FOnlinePresenceSimplyStreamPtr Presence = StaticCastSharedPtr<FOnlinePresenceSimplyStream>(Subsystem->GetPresenceInterface());
	if (!Presence.IsValid())
	{
		return;
	}

	if (const TMap<FString, TArray<TSharedRef<FOnlineFriend>>>* ListMap = FriendsLists.Find(LocalUserNum))
	{
		if (const TArray<TSharedRef<FOnlineFriend>>* Friends = ListMap->Find(TEXT("default")))
		{
			for (const TSharedRef<FOnlineFriend>& FriendEntry : *Friends)
			{
				if (FriendEntry->GetInviteStatus() == EInviteStatus::Accepted)
				{
					Presence->TrackRemoteUser(*FriendEntry->GetUserId());
				}
			}
		}
	}

	if (const TArray<TSharedRef<FOnlineBlockedPlayer>>* BlockedList = BlockedPlayers.Find(LocalUserNum))
	{
		for (const TSharedRef<FOnlineBlockedPlayer>& BlockedEntry : *BlockedList)
		{
			Presence->UntrackRemoteUser(*BlockedEntry->GetUserId());
		}
	}
}

void FOnlineFriendsSimplyStream::UnregisterFriendsForPresence(int32 LocalUserNum)
{
	FOnlinePresenceSimplyStreamPtr Presence = StaticCastSharedPtr<FOnlinePresenceSimplyStream>(Subsystem->GetPresenceInterface());
	if (!Presence.IsValid())
	{
		return;
	}

	if (const TMap<FString, TArray<TSharedRef<FOnlineFriend>>>* ListMap = FriendsLists.Find(LocalUserNum))
	{
		if (const TArray<TSharedRef<FOnlineFriend>>* Friends = ListMap->Find(TEXT("default")))
		{
			for (const TSharedRef<FOnlineFriend>& FriendEntry : *Friends)
			{
				Presence->UntrackRemoteUser(*FriendEntry->GetUserId());
			}
		}
	}

	if (TArray<TSharedRef<FOnlineBlockedPlayer>>* BlockedList = BlockedPlayers.Find(LocalUserNum))
	{
		for (const TSharedRef<FOnlineBlockedPlayer>& BlockedEntry : *BlockedList)
		{
			Presence->UntrackRemoteUser(*BlockedEntry->GetUserId());
		}
	}
}
