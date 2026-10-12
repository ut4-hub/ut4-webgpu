// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineSessionSimplyStream.h"
#include "Engine/Engine.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineIdentitySimplyStream.h"
#include "OnlinePresenceSimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"
#include "Online/OnlineSessionNames.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Interfaces/VoiceInterface.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Algo/AllOf.h"
#include "../../../../Source/ThirdParty/wasm_utils/wndr_probe.h"
#include <cstdio>

namespace SimplyStreamSessions
{
	static constexpr double FindSessionsCacheTtlSeconds = 3.0;

	static void EmitServerReadinessMarker(const ANSICHAR* Message)
	{
#if UE_SERVER
		if (IsRunningDedicatedServer())
		{
			fprintf(stderr, "[SimplyStreamServer] %s\n", Message);
			fflush(stderr);
		}
#else
		(void)Message;
#endif
	}

	static const FName SettingSessionTypeKey(TEXT("SIMPLYSTREAM_SESSION_TYPE"));
	static const FName SettingMinPlayersKey(TEXT("SIMPLYSTREAM_MIN_PLAYERS"));
	static const FName SettingKeywordsKey(TEXT("SIMPLYSTREAM_KEYWORDS"));
	static const FName SettingInviteIdKey(TEXT("SIMPLYSTREAM_INVITE_ID"));
	static const FName SettingInvitedByKey(TEXT("SIMPLYSTREAM_INVITED_BY"));
	static const FName SettingMatchIdKey(TEXT("SIMPLYSTREAM_MATCH_ID"));
	static const FName SettingFriendUserIdKey(TEXT("SIMPLYSTREAM_FRIEND_ID"));
	static const FName SettingIncludeFullKey(TEXT("SIMPLYSTREAM_INCLUDE_FULL"));
	static const FName SettingIncludeInvitesKey(TEXT("SIMPLYSTREAM_INCLUDE_INVITES"));
	static const FName SettingConfigIdKey(TEXT("SIMPLYSTREAM_CONFIG_ID"));
	static const FName SettingAllocationPolicyKey(TEXT("SIMPLYSTREAM_ALLOCATION_POLICY"));

	inline const FUniqueNetIdSimplyStream* AsSimplyStreamId(const FUniqueNetId& Id)
	{
		if (Id.GetType() == SIMPLYSTREAM_SUBSYSTEM)
		{
			return static_cast<const FUniqueNetIdSimplyStream*>(&Id);
		}
		return nullptr;
	}

	template<typename TValue>
	bool GetSessionSettingValue(const FOnlineSessionSettings& Settings, const FName& Key, TValue& OutValue)
	{
		if (const FOnlineSessionSetting* FoundSetting = Settings.Settings.Find(Key))
		{
			FoundSetting->Data.GetValue(OutValue);
			return true;
		}
		return false;
	}

	inline FString GetSessionSettingString(const FOnlineSessionSettings& Settings, const FName& Key)
	{
		FString Value;
		GetSessionSettingValue(Settings, Key, Value);
		return Value;
	}

	inline FString GetSessionSettingString(const FOnlineSearchSettings& Settings, const FName& Key)
	{
		FString Value;
		if (const FOnlineSessionSearchParam* Param = Settings.SearchParams.Find(Key))
		{
			Value = Param->Data.ToString();
		}
		return Value;
	}

	inline FString MakeBackendMembershipKey(const FString& SessionId, const FUniqueNetId& PlayerId)
	{
		return FString::Printf(TEXT("%s|%s"), *SessionId, *PlayerId.ToString());
	}

	inline bool SessionHasRegisteredPlayer(const FNamedOnlineSession& Session, const FUniqueNetId& PlayerId)
	{
		const FString PlayerIdString = PlayerId.ToString();
		return Session.RegisteredPlayers.ContainsByPredicate(
			[&PlayerIdString](const FUniqueNetIdRef& RegisteredId)
			{
				return RegisteredId->ToString() == PlayerIdString;
			});
	}

	template<typename TValue>
	bool GetSessionSettingValue(const FOnlineSearchSettings& Settings, const FName& Key, TValue& OutValue)
	{
		if (const FOnlineSessionSearchParam* Param = Settings.SearchParams.Find(Key))
		{
			Param->Data.GetValue(OutValue);
			return true;
		}
		return false;
	}

	inline FString ExtractProjectId(const FUniqueNetId& UserId)
	{
		if (const FUniqueNetIdSimplyStream* SimplyStreamId = AsSimplyStreamId(UserId))
		{
			return SimplyStreamId->GetProjectId();
		}
		return FString();
	}

	inline TArray<FString> ParseKeywords(const FString& KeywordStr)
	{
		TArray<FString> Keywords;
		if (!KeywordStr.IsEmpty())
		{
			KeywordStr.ParseIntoArray(Keywords, TEXT(","), true);
			for (FString& Entry : Keywords)
			{
				Entry = Entry.TrimStartAndEnd();
			}
			Keywords.RemoveAll([](const FString& Entry) { return Entry.IsEmpty(); });
		}
		return Keywords;
	}

	inline TSharedPtr<FJsonObject> BuildSessionSettingsPayload(const FOnlineSessionSettings& Settings)
	{
		TSharedPtr<FJsonObject> SessionSettingsJson = MakeShared<FJsonObject>();
		for (const TPair<FName, FOnlineSessionSetting>& Pair : Settings.Settings)
		{
			const FString ValueStr = Pair.Value.Data.ToString();
			if (!ValueStr.IsEmpty())
			{
				SessionSettingsJson->SetStringField(Pair.Key.ToString(), ValueStr);
			}
		}
		return SessionSettingsJson;
	}

	inline FString SessionTypeFromSettings(const FOnlineSessionSettings& Settings)
	{
		FString SessionType;
		if (GetSessionSettingValue(Settings, SettingSessionTypeKey, SessionType))
		{
			SessionType.TrimStartAndEndInline();
			SessionType.ToLowerInline();
			if (SessionType == TEXT("public") || SessionType == TEXT("private") ||
				SessionType == TEXT("friends_only") || SessionType == TEXT("invite_only"))
			{
				return SessionType;
			}
		}
		// Fallback to access flags
		if (!Settings.bAllowJoinInProgress && !Settings.bAllowInvites)
		{
			return TEXT("invite_only");
		}
		if (!Settings.bAllowJoinViaPresenceFriendsOnly && Settings.bAllowJoinViaPresence)
		{
			return TEXT("public");
		}
		if (Settings.bAllowJoinViaPresenceFriendsOnly)
		{
			return TEXT("friends_only");
		}
		return TEXT("public");
	}

	inline FString BuildQueryParam(const FString& Key, const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return FString();
		}
		return FString::Printf(TEXT("%s=%s"), *Key, *FGenericPlatformHttp::UrlEncode(Value));
	}

	inline void AppendQueryParam(FString& Query, const FString& Param)
	{
		if (Param.IsEmpty())
		{
			return;
		}
		if (Query.IsEmpty())
		{
			Query = TEXT("?");
			Query += Param;
		}
		else
		{
			Query += TEXT("&");
			Query += Param;
		}
	}

	inline void AddStringSetting(FOnlineSessionSettings& Settings, const FName& Key, const FString& Value)
	{
		if (!Value.IsEmpty())
		{
			Settings.Settings.Add(Key, FOnlineSessionSetting(Value));
		}
	}

	inline bool TryGetJsonString(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, FString& OutValue)
	{
		return JsonObject.IsValid() && JsonObject->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty();
	}

	inline int32 GetJsonIntegerOrDefault(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, int32 DefaultValue)
	{
		double NumberValue = 0.0;
		if (JsonObject.IsValid() && JsonObject->TryGetNumberField(FieldName, NumberValue))
		{
			return static_cast<int32>(NumberValue);
		}
		return DefaultValue;
	}

	inline bool ApplyConnectionInfoToSession(const TSharedPtr<FJsonObject>& ConnectionInfo, FOnlineSession& Session)
	{
		if (!ConnectionInfo.IsValid())
		{
			return false;
		}

		bool bHaveEndpoint = false;
		FString Ip;
		int32 Port = 0;
		ConnectionInfo->TryGetStringField(TEXT("ip"), Ip);
		ConnectionInfo->TryGetNumberField(TEXT("port"), Port);
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSubsystem && !Ip.IsEmpty() && Port > 0)
		{
			bool bIsValid = false;
			TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
			Addr->SetIp(*Ip, bIsValid);
			Addr->SetPort(Port);
			if (bIsValid)
			{
				if (!Session.SessionInfo.IsValid())
				{
					Session.SessionInfo = MakeShareable(new FOnlineSessionInfoSimplyStream());
				}
				StaticCastSharedPtr<FOnlineSessionInfoSimplyStream>(Session.SessionInfo)->HostAddr = Addr;
				bHaveEndpoint = true;
			}
		}

		FString ServerUrl;
		if (ConnectionInfo->TryGetStringField(TEXT("serverUrl"), ServerUrl) && !ServerUrl.IsEmpty())
		{
			Session.SessionSettings.Settings.Add(SETTING_CUSTOM_JOIN_INFO, FOnlineSessionSetting(ServerUrl));
			bHaveEndpoint = true;
		}

		return bHaveEndpoint;
	}

	inline bool BuildSearchResultFromSessionJson(const TSharedPtr<FJsonObject>& Entry, const FOnlineSessionSettings* DefaultSettings, FOnlineSessionSearchResult& OutSearchResult)
	{
		if (!Entry.IsValid())
		{
			return false;
		}

		FString SessionId;
		if (!Entry->TryGetStringField(TEXT("sessionId"), SessionId) || SessionId.IsEmpty())
		{
			Entry->TryGetStringField(TEXT("id"), SessionId);
		}
		if (SessionId.IsEmpty())
		{
			return false;
		}

		OutSearchResult = FOnlineSessionSearchResult();
		OutSearchResult.Session.SessionInfo = MakeShareable(new FOnlineSessionInfoSimplyStream());
		StaticCastSharedPtr<FOnlineSessionInfoSimplyStream>(OutSearchResult.Session.SessionInfo)->SessionId = FUniqueNetIdSimplyStream::Create(SessionId);

		FString OwnerName;
		if (Entry->TryGetStringField(TEXT("ownerName"), OwnerName) || Entry->TryGetStringField(TEXT("ownerDisplayName"), OwnerName))
		{
			OutSearchResult.Session.OwningUserName = OwnerName;
		}

		FString OwnerId;
		if (Entry->TryGetStringField(TEXT("ownerId"), OwnerId) && !OwnerId.IsEmpty())
		{
			OutSearchResult.Session.OwningUserId = FUniqueNetIdSimplyStream::Create(OwnerId);
		}

		const int32 MaxPlayers = FMath::Max(1, GetJsonIntegerOrDefault(Entry, TEXT("maxPlayers"), 1));
		const int32 CurrentPlayers = FMath::Max(0, GetJsonIntegerOrDefault(Entry, TEXT("currentPlayers"), 0));
		OutSearchResult.Session.NumOpenPublicConnections = FMath::Max(0, MaxPlayers - CurrentPlayers);
		OutSearchResult.Session.SessionSettings = DefaultSettings ? *DefaultSettings : FOnlineSessionSettings();
		OutSearchResult.Session.SessionSettings.NumPublicConnections = MaxPlayers;

		bool bIsJoinable = true;
		if (Entry->TryGetBoolField(TEXT("isJoinable"), bIsJoinable))
		{
			OutSearchResult.Session.SessionSettings.bAllowJoinInProgress = bIsJoinable;
		}

		FString GameMode;
		if (TryGetJsonString(Entry, TEXT("gameMode"), GameMode))
		{
			AddStringSetting(OutSearchResult.Session.SessionSettings, SETTING_GAMEMODE, GameMode);
		}

		FString MapName;
		if (TryGetJsonString(Entry, TEXT("mapName"), MapName))
		{
			AddStringSetting(OutSearchResult.Session.SessionSettings, SETTING_MAPNAME, MapName);
		}

		FString Region;
		if (TryGetJsonString(Entry, TEXT("region"), Region))
		{
			AddStringSetting(OutSearchResult.Session.SessionSettings, SETTING_REGION, Region);
		}

		FString SessionType;
		if (TryGetJsonString(Entry, TEXT("sessionType"), SessionType))
		{
			AddStringSetting(OutSearchResult.Session.SessionSettings, SettingSessionTypeKey, SessionType);
		}

		FString InviteId;
		if (TryGetJsonString(Entry, TEXT("inviteId"), InviteId))
		{
			AddStringSetting(OutSearchResult.Session.SessionSettings, SettingInviteIdKey, InviteId);
		}

		FString InvitedBy;
		if (TryGetJsonString(Entry, TEXT("invitedBy"), InvitedBy) || TryGetJsonString(Entry, TEXT("invitedByUserId"), InvitedBy))
		{
			AddStringSetting(OutSearchResult.Session.SessionSettings, SettingInvitedByKey, InvitedBy);
		}

		FString MatchId;
		if (TryGetJsonString(Entry, TEXT("matchId"), MatchId))
		{
			AddStringSetting(OutSearchResult.Session.SessionSettings, SettingMatchIdKey, MatchId);
		}

		const TSharedPtr<FJsonObject>* SessionSettingsJson = nullptr;
		if (Entry->TryGetObjectField(TEXT("sessionSettings"), SessionSettingsJson) && SessionSettingsJson && (*SessionSettingsJson).IsValid())
		{
			for (const auto& Pair : (*SessionSettingsJson)->Values)
			{
				if (Pair.Value.IsValid() && Pair.Value->Type == EJson::String)
				{
					OutSearchResult.Session.SessionSettings.Settings.Add(FName(*Pair.Key), FOnlineSessionSetting(Pair.Value->AsString()));
				}
			}
		}

		const TSharedPtr<FJsonObject>* ConnectionInfo = nullptr;
		if (Entry->TryGetObjectField(TEXT("connectionInfo"), ConnectionInfo) && ConnectionInfo && (*ConnectionInfo).IsValid())
		{
			ApplyConnectionInfoToSession(*ConnectionInfo, OutSearchResult.Session);
		}

		double PingValue = 0.0;
		if (Entry->TryGetNumberField(TEXT("ping"), PingValue))
		{
			OutSearchResult.PingInMs = static_cast<int32>(PingValue);
		}

		return true;
	}

	inline bool ExtractSessionResultFromResponse(const TSharedPtr<FJsonObject>& JsonObject, FOnlineSessionSearchResult& OutSearchResult)
	{
		if (!JsonObject.IsValid())
		{
			return false;
		}

		bool bApiSuccess = true;
		JsonObject->TryGetBoolField(TEXT("success"), bApiSuccess);
		if (!bApiSuccess)
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* ResultObject = nullptr;
		if (JsonObject->TryGetObjectField(TEXT("result"), ResultObject) && ResultObject && (*ResultObject).IsValid())
		{
			const TSharedPtr<FJsonObject>* SessionObject = nullptr;
			if ((*ResultObject)->TryGetObjectField(TEXT("session"), SessionObject) && SessionObject && (*SessionObject).IsValid())
			{
				return BuildSearchResultFromSessionJson(*SessionObject, nullptr, OutSearchResult);
			}
			return BuildSearchResultFromSessionJson(*ResultObject, nullptr, OutSearchResult);
		}

		const TArray<TSharedPtr<FJsonValue>>* ResultsArray = nullptr;
		if (JsonObject->TryGetArrayField(TEXT("results"), ResultsArray) && ResultsArray && ResultsArray->Num() > 0)
		{
			const TSharedPtr<FJsonObject> FirstResult = (*ResultsArray)[0].IsValid() ? (*ResultsArray)[0]->AsObject() : nullptr;
			return BuildSearchResultFromSessionJson(FirstResult, nullptr, OutSearchResult);
		}

		return BuildSearchResultFromSessionJson(JsonObject, nullptr, OutSearchResult);
	}
}

namespace
{
static bool IsSuccessfulJsonResponse(FHttpResponsePtr Response)
{
		if (!Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			return false;
		}

		const FString Content = Response->GetContentAsString();
		if (Content.IsEmpty())
		{
			return true;
		}

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			if (JsonObject->HasField(TEXT("success")))
			{
				return JsonObject->GetBoolField(TEXT("success"));
			}
		}

	return true;
}

static FString ExtractErrorMessage(FHttpResponsePtr Response)
{
	if (!Response.IsValid())
	{
		return TEXT("No response received");
	}

	const int32 ResponseCode = Response->GetResponseCode();
	const FString Content = Response->GetContentAsString();
	if (!Content.IsEmpty())
	{
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			FString ErrorMessage;
			if (JsonObject->TryGetStringField(TEXT("error"), ErrorMessage) && !ErrorMessage.IsEmpty())
			{
				return ErrorMessage;
			}
		}

		return FString::Printf(TEXT("HTTP %d: %s"), ResponseCode, *Content);
	}

	return FString::Printf(TEXT("HTTP %d"), ResponseCode);
}
}


FOnlineSessionSimplyStream::FOnlineSessionSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
	: SimplyStreamSubsystem(InSubsystem)
{
}

void FOnlineSessionSimplyStream::RegisterVoice(const FUniqueNetId& PlayerId)
{
	if (!SimplyStreamSubsystem || !PlayerId.IsValid())
	{
		return;
	}

	IOnlineVoicePtr VoiceInt = SimplyStreamSubsystem->GetVoiceInterface();
	if (!VoiceInt.IsValid())
	{
		return;
	}

	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	const int32 LocalUserNum = IdentityInt.IsValid()
		? SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, PlayerId)
		: INDEX_NONE;

	if (LocalUserNum != INDEX_NONE)
	{
		VoiceInt->ProcessMuteChangeNotification();
	}
	else
	{
		VoiceInt->RegisterRemoteTalker(PlayerId);
	}
}

void FOnlineSessionSimplyStream::UnregisterVoice(const FUniqueNetId& PlayerId)
{
	if (!SimplyStreamSubsystem || !PlayerId.IsValid())
	{
		return;
	}

	IOnlineVoicePtr VoiceInt = SimplyStreamSubsystem->GetVoiceInterface();
	if (!VoiceInt.IsValid())
	{
		return;
	}

	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	const int32 LocalUserNum = IdentityInt.IsValid()
		? SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, PlayerId)
		: INDEX_NONE;

	if (LocalUserNum == INDEX_NONE)
	{
		VoiceInt->UnregisterRemoteTalker(PlayerId);
	}
}

bool FOnlineSessionSimplyStream::Init()
{
	return true;
}

void FOnlineSessionSimplyStream::Tick(float DeltaTime)
{
}

void FOnlineSessionSimplyStream::Shutdown()
{
}

FOnlineSessionInfoSimplyStream::FOnlineSessionInfoSimplyStream() 
	: HostAddr(nullptr)
	, SessionId(FUniqueNetIdSimplyStream::Create(TEXT("")))
{
}

FOnlineSessionInfoSimplyStream::FOnlineSessionInfoSimplyStream(const FUniqueNetIdPtr& InSessionId)
	: HostAddr(nullptr)
	, SessionId(InSessionId)
{
	if (!SessionId.IsValid())
	{
		SessionId = FUniqueNetIdSimplyStream::Create(TEXT(""));
	}
}

void FOnlineSessionInfoSimplyStream::Init()
{
	HostAddr = nullptr;
	const FGuid NewGuid = FGuid::NewGuid();
	SessionId = FUniqueNetIdSimplyStream::Create(NewGuid.ToString());
}

FUniqueNetIdPtr FOnlineSessionSimplyStream::CreateSessionIdFromString(const FString& SessionIdStr)
{
	FUniqueNetIdPtr SessionId;
	if (!SessionIdStr.IsEmpty())
	{
		SessionId = FUniqueNetIdSimplyStream::Create(SessionIdStr);
	}
	return SessionId;
}

FUniqueNetIdPtr FOnlineSessionSimplyStream::GetUniquePlayerId(int32 LocalUserNum) const
{
	FOnlineIdentitySimplyStreamPtr IdentityInt = StaticCastSharedPtr<FOnlineIdentitySimplyStream>(SimplyStreamSubsystem->GetIdentityInterface());
	if (IdentityInt.IsValid())
	{
		return IdentityInt->GetUniquePlayerId(LocalUserNum);
	}
	return nullptr;
}

FNamedOnlineSession* FOnlineSessionSimplyStream::AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings)
{
	FScopeLock ScopeLock(&SessionLock);
	return new (Sessions) FNamedOnlineSessionSimplyStream(SessionName, SessionSettings);
}

FNamedOnlineSession* FOnlineSessionSimplyStream::AddNamedSession(FName SessionName, const FOnlineSession& Session)
{
	FScopeLock ScopeLock(&SessionLock);
	return new (Sessions)FNamedOnlineSessionSimplyStream(SessionName, Session);
}

FNamedOnlineSession* FOnlineSessionSimplyStream::GetNamedSession(FName SessionName)
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionName == SessionName)
		{
			return &Sessions[SearchIndex];
		}
	}
	return nullptr;
}

void FOnlineSessionSimplyStream::RemoveNamedSession(FName SessionName)
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionName == SessionName)
		{
			const TSharedPtr<FOnlineSessionInfoSimplyStream> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoSimplyStream>(Sessions[SearchIndex].SessionInfo);
			const FString SessionId = SessionInfo.IsValid() && SessionInfo->SessionId.IsValid()
				? SessionInfo->SessionId->ToString()
				: FString();
			if (!SessionId.IsEmpty())
			{
				const FString Prefix = SessionId + TEXT("|");
				TArray<FString> KeysToRemove;
				for (const FString& Key : BackendRegisteredPlayerKeys)
				{
					if (Key.StartsWith(Prefix))
					{
						KeysToRemove.Add(Key);
					}
				}
				for (const FString& Key : BackendRegisterInFlightPlayerKeys)
				{
					if (Key.StartsWith(Prefix))
					{
						KeysToRemove.Add(Key);
					}
				}
				for (const FString& Key : KeysToRemove)
				{
					BackendRegisteredPlayerKeys.Remove(Key);
					BackendRegisterInFlightPlayerKeys.Remove(Key);
				}
			}
			Sessions.RemoveAtSwap(SearchIndex);
			return;
		}
	}
}

EOnlineSessionState::Type FOnlineSessionSimplyStream::GetSessionState(FName SessionName) const
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionName == SessionName)
		{
			return Sessions[SearchIndex].SessionState;
		}
	}

	return EOnlineSessionState::NoSession;
}

bool FOnlineSessionSimplyStream::HasPresenceSession()
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionSettings.bUsesPresence)
		{
			return true;
		}
	}
	
	return false;
}

bool FOnlineSessionSimplyStream::IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId)
{ 
	return IsPlayerInSessionImpl(this, SessionName, UniqueId);
}

FOnlineSessionSettings* FOnlineSessionSimplyStream::GetSessionSettings(FName SessionName)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		return &Session->SessionSettings;
	}
	return nullptr;
}

int32 FOnlineSessionSimplyStream::GetNumSessions()
{
	FScopeLock ScopeLock(&SessionLock);
	return Sessions.Num();
}

void FOnlineSessionSimplyStream::DumpSessionState()
{
	FScopeLock ScopeLock(&SessionLock);

	for (int32 SessionIdx = 0; SessionIdx < Sessions.Num(); SessionIdx++)
	{
		DumpNamedSession(&Sessions[SessionIdx]);
	}
}

FNamedOnlineSession* FOnlineSessionSimplyStream::GetPresenceSession()
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		FNamedOnlineSession& Session = Sessions[SearchIndex];
		if (Session.SessionSettings.bUsesPresence)
		{
			return &Sessions[SearchIndex];
		}
	}
	return nullptr;
}

bool FOnlineSessionSimplyStream::CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	FUniqueNetIdPtr HostingPlayerId = GetUniquePlayerId(HostingPlayerNum);
	if (!HostingPlayerId.IsValid())
	{
		return false;
	}
	return CreateSession(*HostingPlayerId, SessionName, NewSessionSettings);
}

bool FOnlineSessionSimplyStream::CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("CreateSession: Identity interface unavailable"));
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	if (GetNamedSession(SessionName) != nullptr)
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("CreateSession: Session '%s' already exists"), *SessionName.ToString());
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, HostingPlayerId);
	if (AuthToken.IsEmpty())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("CreateSession: Missing auth token"));
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	const FString ProjectId = SimplyStreamSessions::ExtractProjectId(HostingPlayerId);
	if (ProjectId.IsEmpty())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("CreateSession: Hosting player lacks project ID"));
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	FNamedOnlineSession* Session = AddNamedSession(SessionName, NewSessionSettings);
	if (!Session)
	{
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	Session->SessionState = EOnlineSessionState::Creating;
	Session->NumOpenPrivateConnections = NewSessionSettings.NumPrivateConnections;
	Session->NumOpenPublicConnections = NewSessionSettings.NumPublicConnections;
	Session->SessionInfo = MakeShareable(new FOnlineSessionInfoSimplyStream());
	Session->OwningUserId = FUniqueNetIdSimplyStream::CreateFrom(HostingPlayerId);
	Session->RegisteredPlayers.Empty();
	if (Session->OwningUserId.IsValid())
	{
		Session->RegisteredPlayers.Add(Session->OwningUserId.ToSharedRef());
	}

	const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, HostingPlayerId);
	Session->OwningUserName = IdentityInt->GetPlayerNickname(LocalUserNum);

	const FString SessionType = SimplyStreamSessions::SessionTypeFromSettings(NewSessionSettings);
	const FString GameMode = SimplyStreamSessions::GetSessionSettingString(NewSessionSettings, SETTING_GAMEMODE);
	const FString MapName = SimplyStreamSessions::GetSessionSettingString(NewSessionSettings, SETTING_MAPNAME);
	const FString Region = SimplyStreamSessions::GetSessionSettingString(NewSessionSettings, SETTING_REGION);
	const FString ConfigId = SimplyStreamSessions::GetSessionSettingString(NewSessionSettings, SimplyStreamSessions::SettingConfigIdKey);
	const FString AllocationPolicy = SimplyStreamSessions::GetSessionSettingString(NewSessionSettings, SimplyStreamSessions::SettingAllocationPolicyKey);

	int32 MinPlayers = 1;
	int32 NeedsSetting = 0;
	if (SimplyStreamSessions::GetSessionSettingValue(NewSessionSettings, SETTING_NEEDS, NeedsSetting) && NeedsSetting > 0)
	{
		MinPlayers = NeedsSetting;
	}
	else if (SimplyStreamSessions::GetSessionSettingValue(NewSessionSettings, SimplyStreamSessions::SettingMinPlayersKey, MinPlayers))
	{
		MinPlayers = FMath::Max(1, MinPlayers);
	}
	else
	{
		MinPlayers = FMath::Clamp(NewSessionSettings.NumPrivateConnections, 1, FMath::Max(1, NewSessionSettings.NumPublicConnections));
	}

	const int32 MaxPlayers = FMath::Max(1, NewSessionSettings.NumPublicConnections + NewSessionSettings.NumPrivateConnections);

	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	RequestJson->SetStringField(TEXT("projectId"), ProjectId);
	RequestJson->SetStringField(TEXT("sessionName"), SessionName.ToString());
	RequestJson->SetStringField(TEXT("sessionType"), SessionType);
	RequestJson->SetNumberField(TEXT("maxPlayers"), MaxPlayers);
	RequestJson->SetNumberField(TEXT("minPlayers"), FMath::Clamp(MinPlayers, 1, MaxPlayers));
	RequestJson->SetBoolField(TEXT("isJoinable"), SessionType != TEXT("invite_only"));

	if (!GameMode.IsEmpty())
	{
		RequestJson->SetStringField(TEXT("gameMode"), GameMode);
	}
	if (!MapName.IsEmpty())
	{
		RequestJson->SetStringField(TEXT("mapName"), MapName);
	}
	if (!Region.IsEmpty())
	{
		RequestJson->SetStringField(TEXT("region"), Region);
	}
	if (!ConfigId.IsEmpty())
	{
		RequestJson->SetStringField(TEXT("configId"), ConfigId);
		FString EffectiveAllocationPolicy = AllocationPolicy.IsEmpty() ? TEXT("scale_if_needed") : AllocationPolicy;
#if UE_SERVER
		if (IsRunningDedicatedServer())
		{
			// A dedicated server creating its own Lyra GameSession must advertise this process, not ask the
			// backend to allocate another server recursively. The runtime allocation is finalized separately.
			EffectiveAllocationPolicy = TEXT("manual_connection_info");
		}
#endif
		RequestJson->SetStringField(TEXT("allocationPolicy"), EffectiveAllocationPolicy);
	}

	const FString KeywordString = SimplyStreamSessions::GetSessionSettingString(NewSessionSettings, SimplyStreamSessions::SettingKeywordsKey);
	TArray<FString> Keywords = SimplyStreamSessions::ParseKeywords(!KeywordString.IsEmpty() ? KeywordString : SimplyStreamSessions::GetSessionSettingString(NewSessionSettings, SEARCH_KEYWORDS));
	if (Keywords.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> KeywordValues;
		for (const FString& Keyword : Keywords)
		{
			KeywordValues.Add(MakeShared<FJsonValueString>(Keyword));
		}
		RequestJson->SetArrayField(TEXT("searchKeywords"), KeywordValues);
	}

	const TSharedPtr<FJsonObject> SessionSettingsJson = SimplyStreamSessions::BuildSessionSettingsPayload(NewSessionSettings);
	if (SessionSettingsJson.IsValid() && SessionSettingsJson->Values.Num() > 0)
	{
		RequestJson->SetObjectField(TEXT("sessionSettings"), SessionSettingsJson);
	}

	FString RequestContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/sessions"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestContent);
	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSessionSimplyStream::OnCreateSessionRequestComplete, SessionName, AuthToken);

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("CreateSession: Failed to send HTTP request"));
		RemoveNamedSession(SessionName);
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	UE_LOG(LogOnlineSession, Verbose, TEXT("CreateSession: Sent request for '%s'"), *SessionName.ToString());
	return true;
}

bool FOnlineSessionSimplyStream::StartSession(FName SessionName)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session && Session->SessionState == EOnlineSessionState::Pending)
	{
		Session->SessionState = EOnlineSessionState::InProgress;
		TriggerOnStartSessionCompleteDelegates(SessionName, true);
		return true;
	}
	
	TriggerOnStartSessionCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionSimplyStream::UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (!Session)
	{
		TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	TSharedPtr<FOnlineSessionInfoSimplyStream> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoSimplyStream>(Session->SessionInfo);
	if (!SessionInfo.IsValid() || !SessionInfo->SessionId.IsValid())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("UpdateSession: Missing session info for '%s'"), *SessionName.ToString());
		TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	FUniqueNetIdPtr OwnerId = Session->OwningUserId;
	if (!OwnerId.IsValid() && Session->RegisteredPlayers.Num() > 0)
	{
		OwnerId = Session->RegisteredPlayers[0];
	}

	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid() || !OwnerId.IsValid())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("UpdateSession: Unable to resolve owning user for '%s'"), *SessionName.ToString());
		TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, *OwnerId);
	if (AuthToken.IsEmpty())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("UpdateSession: Missing auth token for '%s'"), *SessionName.ToString());
		TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	const FString SessionType = SimplyStreamSessions::SessionTypeFromSettings(UpdatedSessionSettings);
	const FString GameMode = SimplyStreamSessions::GetSessionSettingString(UpdatedSessionSettings, SETTING_GAMEMODE);
	const FString MapName = SimplyStreamSessions::GetSessionSettingString(UpdatedSessionSettings, SETTING_MAPNAME);
	const FString Region = SimplyStreamSessions::GetSessionSettingString(UpdatedSessionSettings, SETTING_REGION);

	int32 MinPlayers = 1;
	int32 NeedsSetting = 0;
	if (SimplyStreamSessions::GetSessionSettingValue(UpdatedSessionSettings, SETTING_NEEDS, NeedsSetting) && NeedsSetting > 0)
	{
		MinPlayers = NeedsSetting;
	}
	else if (SimplyStreamSessions::GetSessionSettingValue(UpdatedSessionSettings, SimplyStreamSessions::SettingMinPlayersKey, MinPlayers))
	{
		MinPlayers = FMath::Max(1, MinPlayers);
	}
	else
	{
		MinPlayers = FMath::Clamp(UpdatedSessionSettings.NumPrivateConnections, 1, FMath::Max(1, UpdatedSessionSettings.NumPublicConnections));
	}

	const int32 MaxPlayers = FMath::Max(1, UpdatedSessionSettings.NumPublicConnections + UpdatedSessionSettings.NumPrivateConnections);

	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	// Only PATCH sessionType/isJoinable when the caller EXPLICITLY set the session-type custom setting.
	// SessionTypeFromSettings' flags fallback classifies `!bAllowJoinInProgress && !bAllowInvites` as
	// invite_only — and UE's AGameSession updates joinability flags when the match starts, so a dedicated
	// server's session got PATCHed isJoinable:false the moment its (bot) match began: unjoinable AND
	// filtered out of session search forever ("Session is not joinable" — the Quick Play dead end).
	// The backend PATCH treats absent fields as no-change (COALESCE), so omitting them preserves the
	// create-time classification.
	FString ExplicitSessionType;
	if (SimplyStreamSessions::GetSessionSettingValue(UpdatedSessionSettings, SimplyStreamSessions::SettingSessionTypeKey, ExplicitSessionType) && !ExplicitSessionType.IsEmpty())
	{
		RequestJson->SetStringField(TEXT("sessionType"), SessionType);
		RequestJson->SetBoolField(TEXT("isJoinable"), SessionType != TEXT("invite_only"));
	}
	RequestJson->SetNumberField(TEXT("maxPlayers"), MaxPlayers);
	RequestJson->SetNumberField(TEXT("minPlayers"), FMath::Clamp(MinPlayers, 1, MaxPlayers));

	if (!GameMode.IsEmpty())
	{
		RequestJson->SetStringField(TEXT("gameMode"), GameMode);
	}
	if (!MapName.IsEmpty())
	{
		RequestJson->SetStringField(TEXT("mapName"), MapName);
	}
	if (!Region.IsEmpty())
	{
		RequestJson->SetStringField(TEXT("region"), Region);
	}

	const FString KeywordString = SimplyStreamSessions::GetSessionSettingString(UpdatedSessionSettings, SimplyStreamSessions::SettingKeywordsKey);
	TArray<FString> Keywords = SimplyStreamSessions::ParseKeywords(!KeywordString.IsEmpty() ? KeywordString : SimplyStreamSessions::GetSessionSettingString(UpdatedSessionSettings, SEARCH_KEYWORDS));
	if (Keywords.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> KeywordValues;
		for (const FString& Keyword : Keywords)
		{
			KeywordValues.Add(MakeShared<FJsonValueString>(Keyword));
		}
		RequestJson->SetArrayField(TEXT("searchKeywords"), KeywordValues);
	}

	const TSharedPtr<FJsonObject> SessionSettingsJson = SimplyStreamSessions::BuildSessionSettingsPayload(UpdatedSessionSettings);
	if (SessionSettingsJson.IsValid() && SessionSettingsJson->Values.Num() > 0)
	{
		RequestJson->SetObjectField(TEXT("sessionSettings"), SessionSettingsJson);
	}

	FString RequestContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	const FString SessionId = SessionInfo->SessionId.IsValid() ? SessionInfo->SessionId->ToString() : FString();
	const FString RequestUrl = SimplyStreamSubsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/player/sessions/%s"), *SessionId);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(RequestUrl);
	HttpRequest->SetVerb(TEXT("PATCH"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestContent);

	TSharedPtr<FOnlineSessionSettings> PreviousSettings = MakeShared<FOnlineSessionSettings>(Session->SessionSettings);
	Session->SessionSettings = UpdatedSessionSettings;

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSessionSimplyStream::OnUpdateSessionRequestComplete, SessionName, PreviousSettings);

	if (!HttpRequest->ProcessRequest())
	{
		Session->SessionSettings = *PreviousSettings;
		UE_LOG(LogOnlineSession, Warning, TEXT("UpdateSession: Failed to send HTTP request"));
		TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	return true;
}

bool FOnlineSessionSimplyStream::EndSession(FName SessionName)
{
	// CONTRACT: never fire the completion delegate synchronously from inside this call. Lyra's
	// UCommonSessionSubsystem::OnEndSessionComplete re-enters EndSession from the delegate, so a synchronous
	// Trigger here recurses until the stack dies (observed as a wasm 'Maximum call stack size exceeded' after
	// a failed join travel). Defer every failure path to the next tick.
	auto FailNextTick = [this, SessionName]()
	{
		SimplyStreamSubsystem->ExecuteNextTick([this, SessionName]()
		{
			TriggerOnEndSessionCompleteDelegates(SessionName, false);
		});
		return false;
	};

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (!Session)
	{
		return FailNextTick();
	}

	TSharedPtr<FOnlineSessionInfoSimplyStream> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoSimplyStream>(Session->SessionInfo);
	if (!SessionInfo.IsValid() || !SessionInfo->SessionId.IsValid())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("EndSession: Missing session info for '%s'"), *SessionName.ToString());
		return FailNextTick();
	}

	FUniqueNetIdPtr OwnerId = Session->OwningUserId;
	if (!OwnerId.IsValid() && Session->RegisteredPlayers.Num() > 0)
	{
		OwnerId = Session->RegisteredPlayers[0];
	}

	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		return FailNextTick();
	}

	// A JOINER's named session carries the HOST's OwningUserId, whose token isn't held locally — holding the
	// owner's token is what distinguishes the host from a joiner here.
	FString AuthToken = OwnerId.IsValid() ? SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, *OwnerId) : FString();
	const bool bLocalUserIsOwner = !AuthToken.IsEmpty();
	if (!bLocalUserIsOwner)
	{
		AuthToken = IdentityInt->GetAuthToken(0);
	}
	if (AuthToken.IsEmpty())
	{
		return FailNextTick();
	}

	const FString SessionId = SessionInfo->SessionId->ToString();
	// 'end' is OWNER-ONLY on the backend — a joiner calling it 403s forever ("Only the session owner can end
	// the session"), and Lyra retries a failed end, which spammed the endpoint in a loop. A joiner departing
	// is a member 'leave'.
	const TCHAR* Action = bLocalUserIsOwner ? TEXT("end") : TEXT("leave");
	const FString RequestUrl = SimplyStreamSubsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/player/sessions/%s/%s"), *SessionId, Action);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(RequestUrl);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(TEXT("{}"));
	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSessionSimplyStream::OnEndSessionRequestComplete, SessionName);

	if (!HttpRequest->ProcessRequest())
	{
		return FailNextTick();
	}

	return true;
}

bool FOnlineSessionSimplyStream::DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (!Session)
	{
		CompletionDelegate.ExecuteIfBound(SessionName, false);
		TriggerOnDestroySessionCompleteDelegates(SessionName, false);
		return false;
	}

	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		CompletionDelegate.ExecuteIfBound(SessionName, false);
		TriggerOnDestroySessionCompleteDelegates(SessionName, false);
		return false;
	}

	FUniqueNetIdPtr LocalParticipantId;
	bool bLocalIsHost = false;

	for (int32 LocalUserNum = 0; LocalUserNum < MAX_LOCAL_PLAYERS; ++LocalUserNum)
	{
		FUniqueNetIdPtr LocalId = IdentityInt->GetUniquePlayerId(LocalUserNum);
		if (!LocalId.IsValid())
		{
			continue;
		}

		if (Session->OwningUserId.IsValid() && *Session->OwningUserId == *LocalId)
		{
			bLocalIsHost = true;
			LocalParticipantId = LocalId;
			break;
		}

		if (!LocalParticipantId.IsValid())
		{
			for (const FUniqueNetIdRef& RegisteredId : Session->RegisteredPlayers)
			{
				if (*RegisteredId == *LocalId)
				{
					LocalParticipantId = LocalId;
					break;
				}
			}
		}
	}

	if (!LocalParticipantId.IsValid())
	{
		LocalParticipantId = IdentityInt->GetUniquePlayerId(0);
	}

	if (!LocalParticipantId.IsValid())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("DestroySession: No local participant for '%s'"), *SessionName.ToString());
		CompletionDelegate.ExecuteIfBound(SessionName, false);
		TriggerOnDestroySessionCompleteDelegates(SessionName, false);
		return false;
	}

	const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, *LocalParticipantId);
	if (AuthToken.IsEmpty())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("DestroySession: Missing auth token for local participant"));
		CompletionDelegate.ExecuteIfBound(SessionName, false);
		TriggerOnDestroySessionCompleteDelegates(SessionName, false);
		return false;
	}

	const FString SessionIdStr = Session->GetSessionIdStr();
	if (SessionIdStr.IsEmpty() || SessionIdStr == TEXT("InvalidSession"))
	{
		RemoveNamedSession(SessionName);
		CompletionDelegate.ExecuteIfBound(SessionName, true);
		TriggerOnDestroySessionCompleteDelegates(SessionName, true);
		return true;
	}

	if (!bLocalIsHost)
	{
		TSharedRef<IHttpRequest> LeaveRequest = FHttpModule::Get().CreateRequest();
		LeaveRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/player/sessions/%s/leave"), *SessionIdStr));
		LeaveRequest->SetVerb(TEXT("POST"));
		LeaveRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
		LeaveRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		LeaveRequest->SetContentAsString(TEXT("{}"));
		LeaveRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSessionSimplyStream::OnLeaveSessionRequestComplete, SessionName, CompletionDelegate);

		if (!LeaveRequest->ProcessRequest())
		{
			UE_LOG(LogOnlineSession, Warning, TEXT("DestroySession: Failed to send leave request"));
			CompletionDelegate.ExecuteIfBound(SessionName, false);
			TriggerOnDestroySessionCompleteDelegates(SessionName, false);
			return false;
		}

		return true;
	}

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	// POST /end, not DELETE: a browser (wasm) DELETE triggers a CORS preflight the worker doesn't satisfy,
	// so the request died at fetch level ("request failed, fetch error: 1") and the host's session was
	// never ended — every abandoned browser-host attempt left an endpoint-less zombie in session search.
	// The owner-only /end action is semantically the same cleanup and is a plain CORS-safe POST.
	HttpRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/player/sessions/%s/end"), *SessionIdStr));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(TEXT("{}"));
	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSessionSimplyStream::OnDestroySessionRequestComplete, SessionName, CompletionDelegate);

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("DestroySession: Failed to send HTTP request"));
		CompletionDelegate.ExecuteIfBound(SessionName, false);
		TriggerOnDestroySessionCompleteDelegates(SessionName, false);
		return false;
	}

	return true;
}

bool FOnlineSessionSimplyStream::JoinSession(int32 PlayerNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	FUniqueNetIdPtr PlayerId = GetUniquePlayerId(PlayerNum);
	if (!PlayerId.IsValid())
	{
		return false;
	}
	return JoinSession(*PlayerId, SessionName, DesiredSession);
}

bool FOnlineSessionSimplyStream::JoinSession(const FUniqueNetId& PlayerId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	WndrMarkNetTravelStage(WNDR_NET_STAGE_OSS_JOIN_START);
	UE_LOG(LogOnlineSession, Log, TEXT("[wndr-session] JoinSession('%s') sessionId=%s"),
		*SessionName.ToString(), *DesiredSession.GetSessionIdStr());

	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}

	const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, PlayerId);
	if (AuthToken.IsEmpty())
	{
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}

	const FString SessionIdStr = DesiredSession.GetSessionIdStr();
	if (SessionIdStr.IsEmpty() || SessionIdStr == TEXT("InvalidSession"))
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("JoinSession: Desired session missing ID"));
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}

	// Reserve the named session before the async backend join. This mirrors the stock
	// OnlineSubsystems: once a join is accepted, later JoinSession calls see a session in
	// Pending/InProgress instead of an empty slot.
	if (FNamedOnlineSession* Existing = GetNamedSession(SessionName))
	{
		const TSharedPtr<FOnlineSessionInfoSimplyStream> ExistingInfo = StaticCastSharedPtr<FOnlineSessionInfoSimplyStream>(Existing->SessionInfo);
		const FString ExistingId = ExistingInfo.IsValid() && ExistingInfo->SessionId.IsValid() ? ExistingInfo->SessionId->ToString() : FString();
		if (!ExistingId.IsEmpty() && ExistingId == SessionIdStr)
		{
			WndrMarkNetTravelStage(WNDR_NET_STAGE_OSS_JOIN_DUPLICATE);
			UE_LOG(LogOnlineSession, Warning, TEXT("[wndr-session] JoinSession DUPLICATE for '%s' (%s) — already joining/joined; swallowing (no re-join, no re-travel)"),
				*SessionName.ToString(), *ExistingId);
			return true;
		}

		UE_LOG(LogOnlineSession, Warning, TEXT("[wndr-session] JoinSession('%s') rejected; existing session id=%s requested id=%s"),
			*SessionName.ToString(), ExistingId.IsEmpty() ? TEXT("<unknown>") : *ExistingId, *SessionIdStr);
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::AlreadyInSession);
		return false;
	}

	FNamedOnlineSession* PendingSession = AddNamedSession(SessionName, DesiredSession.Session);
	if (!PendingSession)
	{
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}

	PendingSession->HostingPlayerNum = GetLocalUserIdx(PlayerId);
	PendingSession->SessionState = EOnlineSessionState::Pending;
	PendingSession->SessionInfo = MakeShareable(new FOnlineSessionInfoSimplyStream(FUniqueNetIdSimplyStream::Create(SessionIdStr)));
	if (DesiredSession.Session.SessionInfo.IsValid())
	{
		const FOnlineSessionInfoSimplyStream* DesiredInfo = static_cast<const FOnlineSessionInfoSimplyStream*>(DesiredSession.Session.SessionInfo.Get());
		if (DesiredInfo && DesiredInfo->HostAddr.IsValid())
		{
			StaticCastSharedPtr<FOnlineSessionInfoSimplyStream>(PendingSession->SessionInfo)->HostAddr = DesiredInfo->HostAddr;
		}
	}

	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	RequestJson->SetObjectField(TEXT("playerData"), MakeShared<FJsonObject>());
	FString InviteId;
	if (SimplyStreamSessions::GetSessionSettingValue(DesiredSession.Session.SessionSettings, SimplyStreamSessions::SettingInviteIdKey, InviteId) && !InviteId.IsEmpty())
	{
		RequestJson->SetStringField(TEXT("inviteId"), InviteId);
	}
	FString InvitedBy;
	if (SimplyStreamSessions::GetSessionSettingValue(DesiredSession.Session.SessionSettings, SimplyStreamSessions::SettingInvitedByKey, InvitedBy) && !InvitedBy.IsEmpty())
	{
		RequestJson->SetStringField(TEXT("invitedBy"), InvitedBy);
	}
	FString SessionType;
	if (SimplyStreamSessions::GetSessionSettingValue(DesiredSession.Session.SessionSettings, SimplyStreamSessions::SettingSessionTypeKey, SessionType) && !SessionType.IsEmpty())
	{
		RequestJson->SetStringField(TEXT("sessionType"), SessionType);
	}

	FString RequestContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/player/sessions/%s/join"), *SessionIdStr));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestContent);

	FOnlineSessionSearchResult DesiredSessionCopy = DesiredSession;
	FUniqueNetIdPtr PlayerIdCopy = FUniqueNetIdSimplyStream::CreateFrom(PlayerId);

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSessionSimplyStream::OnJoinSessionRequestComplete, SessionName, DesiredSessionCopy, PlayerIdCopy);

	if (!HttpRequest->ProcessRequest())
	{
		RemoveNamedSession(SessionName);
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}

	return true;
}

bool FOnlineSessionSimplyStream::RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited)
{
	TArray<FUniqueNetIdRef> Players;
	Players.Add(PlayerId.AsShared());
	return RegisterPlayers(SessionName, Players, bWasInvited);
}

// Membership ops (join/leave) must be performed AS THE PLAYER. GetAuthTokenForUser falls back to the
// SERVER's API key for non-local ids — on a dedicated server that made Register/UnregisterPlayers act
// as the session OWNER: unregistering a disconnecting player POSTed /leave with the server token, the
// backend transferred ownership to that (departing) player, whose own client then left as "owner was
// last player" -> the server's session was ENDED and vanished from search after every join/leave cycle.
// Resolve strictly LOCAL tokens here; remote players manage their own membership from their clients.
static FString GetLocalOnlyAuthToken(const IOnlineIdentityPtr& IdentityPtr, const FUniqueNetId& UserId)
{
	const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityPtr, UserId);
	if (LocalUserNum != INDEX_NONE && IdentityPtr.IsValid())
	{
		return IdentityPtr->GetAuthToken(LocalUserNum);
	}
	return FString();
}

bool FOnlineSessionSimplyStream::RegisterPlayers(FName SessionName, const TArray<FUniqueNetIdRef>& Players, bool bWasInvited)
{
	WndrMarkNetTravelStage(WNDR_NET_STAGE_OSS_REGISTER_START);
	UE_LOG(LogOnlineSession, Log, TEXT("[wndr-session] RegisterPlayers('%s', %d player(s))"),
		*SessionName.ToString(), Players.Num());
	if (Players.Num() == 0)
	{
		BroadcastRegisterPlayersDetailed(SessionName, Players, false, TEXT("No players provided"));
		TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, false);
		return false;
	}

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("RegisterPlayers: Session '%s' not found"), *SessionName.ToString());
		BroadcastRegisterPlayersDetailed(SessionName, Players, false, TEXT("Session not found"));
		TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, false);
		return false;
	}

	TSharedPtr<FOnlineSessionInfoSimplyStream> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoSimplyStream>(Session->SessionInfo);
	if (!SessionInfo.IsValid() || !SessionInfo->SessionId.IsValid())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("RegisterPlayers: Session info missing for '%s'"), *SessionName.ToString());
		BroadcastRegisterPlayersDetailed(SessionName, Players, false, TEXT("Session info missing"));
		TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, false);
		return false;
	}

	const FString SessionIdStr = SessionInfo->SessionId->ToString();

	// IDEMPOTENT (2026-07-09): JoinSession already registers backend membership, while
	// Unreal still calls RegisterPlayers after travel to establish local session state.
	// Treat backend-known memberships as authoritative for HTTP purposes, but still apply
	// the Unreal-local registration side effects here.
	bool bAllBackendRegisteredOrPending = true;
	for (const FUniqueNetIdRef& PlayerId : Players)
	{
		const FString BackendKey = SimplyStreamSessions::MakeBackendMembershipKey(SessionIdStr, *PlayerId);
		const bool bSessionRegistered = SimplyStreamSessions::SessionHasRegisteredPlayer(*Session, *PlayerId);
		const bool bBackendRegistered = BackendRegisteredPlayerKeys.Contains(BackendKey);
		const bool bBackendRegisterPending = BackendRegisterInFlightPlayerKeys.Contains(BackendKey);
		if (bSessionRegistered || bBackendRegistered)
		{
			BackendRegisteredPlayerKeys.Add(BackendKey);
			ApplyLocalPlayerRegistration(*Session, PlayerId);
			continue;
		}
		if (bBackendRegisterPending)
		{
			continue;
		}
		bAllBackendRegisteredOrPending = false;
		break;
	}
	if (bAllBackendRegisteredOrPending)
	{
		WndrMarkNetTravelStage(WNDR_NET_STAGE_OSS_REGISTER_DUPLICATE);
		UE_LOG(LogOnlineSession, Warning, TEXT("[wndr-session] RegisterPlayers DUPLICATE for '%s' — backend membership already registered or pending; skipping HTTP"), *SessionName.ToString());
		BroadcastRegisterPlayersDetailed(SessionName, Players, true, FString());
		WndrMarkNetTravelStage(WNDR_NET_STAGE_OSS_REGISTER_COMPLETE);
		TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, true);
		return true;
	}

	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		BroadcastRegisterPlayersDetailed(SessionName, Players, false, TEXT("Identity interface unavailable"));
		TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, false);
		return false;
	}

	TSharedPtr<FPendingRegisterPlayersOperation, ESPMode::ThreadSafe> Operation = MakeShared<FPendingRegisterPlayersOperation, ESPMode::ThreadSafe>(SessionName, Players);

	for (int32 PlayerIdx = 0; PlayerIdx < Players.Num(); ++PlayerIdx)
	{
		const FUniqueNetIdRef& PlayerId = Players[PlayerIdx];
		const FString BackendKey = SimplyStreamSessions::MakeBackendMembershipKey(SessionIdStr, *PlayerId);
		if (SimplyStreamSessions::SessionHasRegisteredPlayer(*Session, *PlayerId) || BackendRegisteredPlayerKeys.Contains(BackendKey))
		{
			BackendRegisteredPlayerKeys.Add(BackendKey);
			ApplyLocalPlayerRegistration(*Session, PlayerId);
			Operation->Results[PlayerIdx] = true;
			continue;
		}
		if (BackendRegisterInFlightPlayerKeys.Contains(BackendKey))
		{
			Operation->Results[PlayerIdx] = true;
			continue;
		}

		const FString AuthToken = GetLocalOnlyAuthToken(IdentityInt, *PlayerId);
		if (AuthToken.IsEmpty())
		{
			// Remote player (dedicated server registering a joiner): the player's own client performs the
			// backend join — doing it here with the server key impersonated the OWNER (see helper comment).
			UE_LOG(LogOnlineSession, Verbose, TEXT("RegisterPlayers: %s is not a local user — backend membership handled by their client"), *PlayerId->ToDebugString());
			continue;
		}

		TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
		RequestJson->SetObjectField(TEXT("playerData"), MakeShared<FJsonObject>());

		FString RequestContent;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
		FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

		TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/player/sessions/%s/join"), *SessionIdStr));
		HttpRequest->SetVerb(TEXT("POST"));
		HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
		HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		HttpRequest->SetContentAsString(RequestContent);

		const FUniqueNetIdPtr PlayerIdCopy = PlayerId;
		Operation->BackendRegistrationKeys[PlayerIdx] = BackendKey;
		BackendRegisterInFlightPlayerKeys.Add(BackendKey);
		Operation->PendingRequests++;
		HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSessionSimplyStream::OnRegisterPlayersRequestComplete, Operation, PlayerIdx, PlayerIdCopy);

		if (!HttpRequest->ProcessRequest())
		{
			UE_LOG(LogOnlineSession, Warning, TEXT("RegisterPlayers: HTTP request failed to start for %s"), *PlayerId->ToDebugString());
			BackendRegisterInFlightPlayerKeys.Remove(BackendKey);
			if (Operation->Errors.IsValidIndex(PlayerIdx))
			{
				Operation->Errors[PlayerIdx] = TEXT("Failed to start request");
			}
			Operation->PendingRequests--;
		}
	}

	if (Operation->PendingRequests == 0)
	{
		CompleteRegisterPlayersOperation(Operation);
		return Algo::AllOf(Operation->Results, [](bool b) { return b; });
	}

	return true;
}

bool FOnlineSessionSimplyStream::UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId)
{
	TArray<FUniqueNetIdRef> Players;
	Players.Add(PlayerId.AsShared());
	return UnregisterPlayers(SessionName, Players);
}

bool FOnlineSessionSimplyStream::UnregisterPlayers(FName SessionName, const TArray<FUniqueNetIdRef>& Players)
{
	WndrMarkNetTravelStage(WNDR_NET_STAGE_OSS_UNREGISTER_START);
	// wndr breadcrumb (2026-07-05 freeze forensics): leaves observed firing MID-JOIN during the
	// WT->WS fallback, wrecking the session before RegisterPlayers re-joins (red join #2 in the
	// field waterfall). Name every unregister so the console timeline is self-explaining.
	UE_LOG(LogOnlineSession, Log, TEXT("[wndr-session] UnregisterPlayers('%s', %d player(s))"),
		*SessionName.ToString(), Players.Num());
#if defined(__EMSCRIPTEN__) // symbol lives in wasm_utils; host cooker builds have no fallback either
	{
		extern double GWndrNetFallbackGraceUntil; // SimplyStreamPlatform.h
		if (FPlatformTime::Seconds() < GWndrNetFallbackGraceUntil)
		{
			WndrMarkNetTravelStage(WNDR_NET_STAGE_OSS_UNREGISTER_SUPPRESS_FALLBACK);
			UE_LOG(LogOnlineSession, Warning, TEXT("[wndr-session] SUPPRESSING leave for '%s' — transport fallback grace window (backend membership must survive the WS retry)"),
				*SessionName.ToString());
			TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, true);
			return true;
		}
	}
#endif
	// 2026-07-05 (corrected attribution): the double-leave fires from Lyra's logout path when
	// the FRONTEND world tears down during travel INTO the server (BeginTearingDown -> logout ->
	// unregister) — on ANY transport (reproduced with WebTransport fully disabled). Leaving the
	// backend session we are actively traveling into is always wrong: suppress the HTTP leave
	// whenever a pending net game exists (we are mid-travel to a server).
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game && Context.PendingNetGame != nullptr)
			{
				WndrMarkNetTravelStage(WNDR_NET_STAGE_OSS_UNREGISTER_SUPPRESS_PENDING_TRAVEL);
				UE_LOG(LogOnlineSession, Warning, TEXT("[wndr-session] SUPPRESSING leave for '%s' — mid-travel to a server (frontend teardown logout must not end backend membership)"),
					*SessionName.ToString());
				TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, true);
				return true;
			}
		}
	}
	if (Players.Num() == 0)
	{
		BroadcastUnregisterPlayersDetailed(SessionName, Players, false, TEXT("No players provided"));
		TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, false);
		return false;
	}

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("UnregisterPlayers: Session '%s' not found"), *SessionName.ToString());
		BroadcastUnregisterPlayersDetailed(SessionName, Players, false, TEXT("Session not found"));
		TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, false);
		return false;
	}

	TSharedPtr<FOnlineSessionInfoSimplyStream> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoSimplyStream>(Session->SessionInfo);
	if (!SessionInfo.IsValid() || !SessionInfo->SessionId.IsValid())
	{
		BroadcastUnregisterPlayersDetailed(SessionName, Players, false, TEXT("Session info missing"));
		TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, false);
		return false;
	}

	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		BroadcastUnregisterPlayersDetailed(SessionName, Players, false, TEXT("Identity interface unavailable"));
		TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, false);
		return false;
	}

	const FString SessionIdStr = SessionInfo->SessionId->ToString();
	TSharedPtr<FPendingUnregisterPlayersOperation, ESPMode::ThreadSafe> Operation = MakeShared<FPendingUnregisterPlayersOperation, ESPMode::ThreadSafe>(SessionName, Players);

	for (int32 PlayerIdx = 0; PlayerIdx < Players.Num(); ++PlayerIdx)
	{
		const FUniqueNetIdRef& PlayerId = Players[PlayerIdx];
		const FString AuthToken = GetLocalOnlyAuthToken(IdentityInt, *PlayerId);
		if (AuthToken.IsEmpty())
		{
			// Remote player (dedicated server unregistering a leaver): POSTing /leave with the server key
			// made the backend treat it as the OWNER leaving — ownership transferred to the departing
			// player and the session got ENDED when their client's own leave landed (see helper comment
			// above RegisterPlayers). Their client performs the real leave; nothing to do here.
			UE_LOG(LogOnlineSession, Verbose, TEXT("UnregisterPlayers: %s is not a local user — backend membership handled by their client"), *PlayerId->ToDebugString());
			continue;
		}

		TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
		WndrMarkNetTravelStage(WNDR_NET_STAGE_OSS_UNREGISTER_HTTP);
		HttpRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/player/sessions/%s/leave"), *SessionIdStr));
		HttpRequest->SetVerb(TEXT("POST"));
		HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
		HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		HttpRequest->SetContentAsString(TEXT("{}"));

		const FUniqueNetIdPtr PlayerIdCopy = PlayerId;
		Operation->PendingRequests++;
		HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSessionSimplyStream::OnUnregisterPlayersRequestComplete, Operation, PlayerIdx, PlayerIdCopy);

		if (!HttpRequest->ProcessRequest())
		{
			UE_LOG(LogOnlineSession, Warning, TEXT("UnregisterPlayers: HTTP request failed to start for %s"), *PlayerId->ToDebugString());
			if (Operation->Errors.IsValidIndex(PlayerIdx))
			{
				Operation->Errors[PlayerIdx] = TEXT("Failed to start request");
			}
			Operation->PendingRequests--;
		}
	}

	if (Operation->PendingRequests == 0)
	{
		CompleteUnregisterPlayersOperation(Operation);
		return false;
	}

	return true;
}

void FOnlineSessionSimplyStream::RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate)
{
	UE_LOG(LogOnlineSession, Verbose, TEXT("RegisterLocalPlayer not implemented for SimplyStream (%s | %s)"), *SessionName.ToString(), *PlayerId.ToDebugString());
	Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::UnknownError);
}

void FOnlineSessionSimplyStream::UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate)
{
	UE_LOG(LogOnlineSession, Verbose, TEXT("UnregisterLocalPlayer not implemented for SimplyStream (%s | %s)"), *SessionName.ToString(), *PlayerId.ToDebugString());
	Delegate.ExecuteIfBound(PlayerId, false);
}

void FOnlineSessionSimplyStream::OnRegisterPlayersRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, TSharedPtr<FPendingRegisterPlayersOperation> Operation, int32 PlayerIndex, FUniqueNetIdPtr PlayerId)
{
	bool bSuccess = bWasSuccessful && IsSuccessfulJsonResponse(Response);
	const FString ErrorMessage = bSuccess ? FString() : ExtractErrorMessage(Response);
	const FString BackendKey = Operation.IsValid() && Operation->BackendRegistrationKeys.IsValidIndex(PlayerIndex)
		? Operation->BackendRegistrationKeys[PlayerIndex]
		: FString();
	if (!BackendKey.IsEmpty())
	{
		BackendRegisterInFlightPlayerKeys.Remove(BackendKey);
	}
	if (!bSuccess)
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("RegisterPlayers HTTP error: %s"), *ErrorMessage);
	}

	if (bSuccess && Operation.IsValid())
	{
		if (FNamedOnlineSession* Session = GetNamedSession(Operation->SessionName))
		{
			if (!BackendKey.IsEmpty())
			{
				BackendRegisteredPlayerKeys.Add(BackendKey);
			}

			if (PlayerId.IsValid())
			{
				ApplyLocalPlayerRegistration(*Session, PlayerId.ToSharedRef());
			}
		}
	}

	if (Operation.IsValid())
	{
		if (Operation->Results.IsValidIndex(PlayerIndex))
		{
			Operation->Results[PlayerIndex] = bSuccess;
		}
		if (Operation->Errors.IsValidIndex(PlayerIndex))
		{
			Operation->Errors[PlayerIndex] = ErrorMessage;
		}

		Operation->PendingRequests = FMath::Max(Operation->PendingRequests - 1, 0);
		if (Operation->PendingRequests == 0)
		{
			CompleteRegisterPlayersOperation(Operation);
		}
	}
}

void FOnlineSessionSimplyStream::OnUnregisterPlayersRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, TSharedPtr<FPendingUnregisterPlayersOperation> Operation, int32 PlayerIndex, FUniqueNetIdPtr PlayerId)
{
	bool bSuccess = bWasSuccessful && IsSuccessfulJsonResponse(Response);
	const FString ErrorMessage = bSuccess ? FString() : ExtractErrorMessage(Response);
	if (!bSuccess)
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("UnregisterPlayers HTTP error: %s"), *ErrorMessage);
	}

	if (bSuccess && Operation.IsValid())
	{
		if (FNamedOnlineSession* Session = GetNamedSession(Operation->SessionName))
		{
			const TSharedPtr<FOnlineSessionInfoSimplyStream> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoSimplyStream>(Session->SessionInfo);
			if (SessionInfo.IsValid() && SessionInfo->SessionId.IsValid())
			{
				const FString BackendKey = SimplyStreamSessions::MakeBackendMembershipKey(SessionInfo->SessionId->ToString(), *PlayerId);
				BackendRegisteredPlayerKeys.Remove(BackendKey);
				BackendRegisterInFlightPlayerKeys.Remove(BackendKey);
			}

			const int32 RegistrantIndex = Session->RegisteredPlayers.IndexOfByPredicate(
				[&PlayerId](const FUniqueNetIdRef& Existing)
				{
					return Existing->ToString() == PlayerId->ToString();
				});

			if (RegistrantIndex != INDEX_NONE)
			{
				Session->RegisteredPlayers.RemoveAtSwap(RegistrantIndex);
				UnregisterVoice(*PlayerId);
				UntrackPresenceForPlayer(*PlayerId);

				if (Session->NumOpenPublicConnections < Session->SessionSettings.NumPublicConnections)
				{
					Session->NumOpenPublicConnections++;
				}
				else if (Session->NumOpenPrivateConnections < Session->SessionSettings.NumPrivateConnections)
				{
					Session->NumOpenPrivateConnections++;
				}
			}
		}
	}

	if (Operation.IsValid())
	{
		if (Operation->Results.IsValidIndex(PlayerIndex))
		{
			Operation->Results[PlayerIndex] = bSuccess;
		}
		if (Operation->Errors.IsValidIndex(PlayerIndex))
		{
			Operation->Errors[PlayerIndex] = ErrorMessage;
		}

		Operation->PendingRequests = FMath::Max(Operation->PendingRequests - 1, 0);
		if (Operation->PendingRequests == 0)
		{
			CompleteUnregisterPlayersOperation(Operation);
		}
	}
}

void FOnlineSessionSimplyStream::RegisterServerConnectionInfo(const FString& SessionId)
{
	// Endpoint source: -SimplyStreamServerUrl=<host[:port]> (the mp runtime passes the VM's public address),
	// falling back to the SIMPLYSTREAM_SERVER_URL env. Browser clients have neither -> no-op (they cannot
	// listen, so advertising an endpoint would be wrong).
	FString ServerUrl;
	if (!FParse::Value(FCommandLine::Get(), TEXT("SimplyStreamServerUrl="), ServerUrl) || ServerUrl.IsEmpty())
	{
		ServerUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("SIMPLYSTREAM_SERVER_URL"));
	}
	ServerUrl.TrimStartAndEndInline();
	if (ServerUrl.IsEmpty())
	{
		UE_LOG(LogOnlineSession, Log, TEXT("RegisterServerConnectionInfo: no SimplyStreamServerUrl configured; session %s stays without connectionInfo"), *SessionId);
		SimplyStreamSessions::EmitServerReadinessMarker("Failed to create session readiness: no SimplyStreamServerUrl configured");
		return;
	}

	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	const FString AuthToken = IdentityInt.IsValid() ? IdentityInt->GetAuthToken(0) : FString();
	if (AuthToken.IsEmpty())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("RegisterServerConnectionInfo: missing auth token; cannot advertise endpoint for session %s"), *SessionId);
		SimplyStreamSessions::EmitServerReadinessMarker("Failed to create session readiness: missing endpoint advertisement auth token");
		return;
	}

	TSharedPtr<FJsonObject> ConnectionInfo = MakeShared<FJsonObject>();
	ConnectionInfo->SetStringField(TEXT("serverUrl"), ServerUrl);
	// Split host[:port] into the ip/port fields too, so both OSS resolve paths (HostAddr and serverUrl) work.
	FString Host = ServerUrl;
	FString PortStr;
	if (ServerUrl.Split(TEXT(":"), &Host, &PortStr, ESearchCase::CaseSensitive, ESearchDir::FromEnd) && PortStr.IsNumeric())
	{
		ConnectionInfo->SetStringField(TEXT("ip"), Host);
		ConnectionInfo->SetNumberField(TEXT("port"), FCString::Atoi(*PortStr));
	}

	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	RequestJson->SetObjectField(TEXT("connectionInfo"), ConnectionInfo);

	FString RequestContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/player/sessions/%s"), *SessionId));
	HttpRequest->SetVerb(TEXT("PATCH"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestContent);
	HttpRequest->OnProcessRequestComplete().BindLambda([SessionId, ServerUrl](FHttpRequestPtr, FHttpResponsePtr InResponse, bool bInWasSuccessful)
	{
		const int32 Code = (bInWasSuccessful && InResponse.IsValid()) ? InResponse->GetResponseCode() : -1;
		if (Code >= 200 && Code < 300)
		{
			UE_LOG(LogOnlineSession, Log, TEXT("RegisterServerConnectionInfo: advertised %s on session %s"), *ServerUrl, *SessionId);
			SimplyStreamSessions::EmitServerReadinessMarker("RegisterServerConnectionInfo: advertised endpoint on session ready");
		}
		else
		{
			UE_LOG(LogOnlineSession, Warning, TEXT("RegisterServerConnectionInfo: PATCH failed (HTTP %d) for session %s — joiners cannot travel"), Code, *SessionId);
			SimplyStreamSessions::EmitServerReadinessMarker("Failed to create session readiness: endpoint advertisement PATCH failed");
		}
	});
	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("RegisterServerConnectionInfo: failed to send PATCH for session %s"), *SessionId);
		SimplyStreamSessions::EmitServerReadinessMarker("Failed to create session readiness: endpoint advertisement request was not sent");
	}
}

void FOnlineSessionSimplyStream::OnCreateSessionRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, FString AuthToken)
{
	bool bSuccess = false;
	FNamedOnlineSession* Session = GetNamedSession(SessionName);

	if (bWasSuccessful && Response.IsValid() && Session)
	{
		if (Response->GetResponseCode() == 201 || Response->GetResponseCode() == 202)
		{
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
			{
				const bool bApiSuccess = JsonObject->GetBoolField(TEXT("success"));
				const TSharedPtr<FJsonObject>* ResultObject = nullptr;
				if (bApiSuccess && JsonObject->TryGetObjectField(TEXT("result"), ResultObject) && ResultObject && (*ResultObject).IsValid())
				{
					const FString SessionId = (*ResultObject)->GetStringField(TEXT("sessionId"));
					Session->SessionState = EOnlineSessionState::Pending;
					Session->NumOpenPublicConnections = Session->SessionSettings.NumPublicConnections;
					Session->SessionInfo = MakeShareable(new FOnlineSessionInfoSimplyStream());
					StaticCastSharedPtr<FOnlineSessionInfoSimplyStream>(Session->SessionInfo)->SessionId = FUniqueNetIdSimplyStream::Create(SessionId);
					if (Response->GetResponseCode() == 202)
					{
						const TSharedPtr<FJsonObject>* AllocationObject = nullptr;
						FString AllocationId;
						if ((*ResultObject)->TryGetObjectField(TEXT("allocation"), AllocationObject) && AllocationObject && (*AllocationObject).IsValid())
						{
							(*AllocationObject)->TryGetStringField(TEXT("allocationId"), AllocationId);
						}
						if (!AllocationId.IsEmpty())
						{
							PollCreateSessionAllocation(SessionName, AuthToken, AllocationId, 90);
							return;
						}
					}
					else
					{
						bool bHaveAllocationEndpoint = false;
						const TSharedPtr<FJsonObject>* ConnectionInfo = nullptr;
						if ((*ResultObject)->TryGetObjectField(TEXT("connectionInfo"), ConnectionInfo) && ConnectionInfo && (*ConnectionInfo).IsValid())
						{
							bHaveAllocationEndpoint = SimplyStreamSessions::ApplyConnectionInfoToSession(*ConnectionInfo, *Session);
						}
						bSuccess = true;

						// Sessions are created with connection_info NULL; joiners then can't resolve a travel
						// address ("Travel to Session failed"). If this host has a reachable endpoint (dedicated
						// server), advertise it now.
						if (!bHaveAllocationEndpoint)
						{
							RegisterServerConnectionInfo(SessionId);
						}
						else
						{
							SimplyStreamSessions::EmitServerReadinessMarker("RegisterServerConnectionInfo: advertised backend endpoint on session ready");
						}
					}
				}
			}
		}
		else
		{
			UE_LOG(LogOnlineSession, Warning, TEXT("CreateSession HTTP %d: %s"), Response->GetResponseCode(), *Response->GetContentAsString());
		}
	}

	if (!bSuccess)
	{
		SimplyStreamSessions::EmitServerReadinessMarker("Failed to create session readiness: backend CreateSession failed");
		if (Session)
		{
			RemoveNamedSession(SessionName);
		}
	}

	TriggerOnCreateSessionCompleteDelegates(SessionName, bSuccess);
}

void FOnlineSessionSimplyStream::PollCreateSessionAllocation(FName SessionName, const FString& AuthToken, const FString& AllocationId, int32 RemainingAttempts)
{
	if (RemainingAttempts <= 0)
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("CreateSession allocation %s timed out"), *AllocationId);
		RemoveNamedSession(SessionName);
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return;
	}

	TWeakPtr<FOnlineSessionSimplyStream, ESPMode::ThreadSafe> WeakThis = AsShared();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, SessionName, AuthToken, AllocationId, RemainingAttempts](float)
	{
		TSharedPtr<FOnlineSessionSimplyStream, ESPMode::ThreadSafe> Pinned = WeakThis.Pin();
		if (!Pinned.IsValid())
		{
			return false;
		}
		TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetURL(Pinned->SimplyStreamSubsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/player/allocations/%s"), *AllocationId));
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
		HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
		HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(Pinned.ToSharedRef(), &FOnlineSessionSimplyStream::OnCreateSessionAllocationPollComplete, SessionName, AuthToken, AllocationId, RemainingAttempts);
		if (!HttpRequest->ProcessRequest())
		{
			Pinned->RemoveNamedSession(SessionName);
			Pinned->TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		}
		return false;
	}), 1.0f);
}

void FOnlineSessionSimplyStream::OnCreateSessionAllocationPollComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, FString AuthToken, FString AllocationId, int32 RemainingAttempts)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200 || !Session)
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("CreateSession allocation poll failed for %s"), *AllocationId);
		RemoveNamedSession(SessionName);
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid() || !JsonObject->GetBoolField(TEXT("success")))
	{
		RemoveNamedSession(SessionName);
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return;
	}

	const TSharedPtr<FJsonObject>* ResultObject = nullptr;
	if (!JsonObject->TryGetObjectField(TEXT("result"), ResultObject) || !ResultObject || !(*ResultObject).IsValid())
	{
		RemoveNamedSession(SessionName);
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return;
	}

	FString Status;
	(*ResultObject)->TryGetStringField(TEXT("status"), Status);
	if (Status == TEXT("ready"))
	{
		const TSharedPtr<FJsonObject>* ConnectionInfo = nullptr;
		if ((*ResultObject)->TryGetObjectField(TEXT("connectionInfo"), ConnectionInfo) && ConnectionInfo && (*ConnectionInfo).IsValid())
		{
			SimplyStreamSessions::ApplyConnectionInfoToSession(*ConnectionInfo, *Session);
		}
		Session->SessionState = EOnlineSessionState::Pending;
		TriggerOnCreateSessionCompleteDelegates(SessionName, true);
		return;
	}

	if (Status == TEXT("failed") || Status == TEXT("capacity_full") || Status == TEXT("expired"))
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("CreateSession allocation %s ended with status %s"), *AllocationId, *Status);
		RemoveNamedSession(SessionName);
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return;
	}

	PollCreateSessionAllocation(SessionName, AuthToken, AllocationId, RemainingAttempts - 1);
}

void FOnlineSessionSimplyStream::OnDestroySessionRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, FOnDestroySessionCompleteDelegate CompletionDelegate)
{
	bool bSuccess = bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200;
	if (bSuccess)
	{
		RemoveNamedSession(SessionName);
	}
	else if (Response.IsValid())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("DestroySession HTTP %d: %s"), Response->GetResponseCode(), *Response->GetContentAsString());
	}

	CompletionDelegate.ExecuteIfBound(SessionName, bSuccess);
	TriggerOnDestroySessionCompleteDelegates(SessionName, bSuccess);
}
void FOnlineSessionSimplyStream::OnLeaveSessionRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, FOnDestroySessionCompleteDelegate CompletionDelegate)
{
	bool bSuccess = bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200;
	if (bSuccess)
	{
		RemoveNamedSession(SessionName);
	}
	else if (Response.IsValid())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("LeaveSession HTTP %d: %s"), Response->GetResponseCode(), *Response->GetContentAsString());
	}

	CompletionDelegate.ExecuteIfBound(SessionName, bSuccess);
	TriggerOnDestroySessionCompleteDelegates(SessionName, bSuccess);
}

void FOnlineSessionSimplyStream::LeaveSessionById(const FString& SessionId, const FUniqueNetIdPtr& PlayerId)
{
	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid() || !PlayerId.IsValid() || SessionId.IsEmpty())
	{
		return;
	}
	const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, *PlayerId);
	if (AuthToken.IsEmpty())
	{
		return;
	}
	TSharedRef<IHttpRequest> LeaveRequest = FHttpModule::Get().CreateRequest();
	LeaveRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/player/sessions/%s/leave"), *SessionId));
	LeaveRequest->SetVerb(TEXT("POST"));
	LeaveRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	LeaveRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	LeaveRequest->SetContentAsString(TEXT("{}"));
	const FString SessionIdCopy = SessionId;
	LeaveRequest->OnProcessRequestComplete().BindLambda([SessionIdCopy](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		const int32 Code = (bOk && Resp.IsValid()) ? Resp->GetResponseCode() : -1;
		UE_LOG(LogOnlineSession, Verbose, TEXT("LeaveSessionById(%s): HTTP %d"), *SessionIdCopy, Code);
	});
	LeaveRequest->ProcessRequest();
}

bool FOnlineSessionSimplyStream::ApplyLocalPlayerRegistration(FNamedOnlineSession& Session, const FUniqueNetIdRef& PlayerId)
{
	const bool bAlreadyRegistered = SimplyStreamSessions::SessionHasRegisteredPlayer(Session, *PlayerId);
	if (!bAlreadyRegistered)
	{
		Session.RegisteredPlayers.Add(PlayerId);
	}

	RegisterVoice(*PlayerId);
	TrackPresenceForPlayer(*PlayerId);

	if (!bAlreadyRegistered)
	{
		if (Session.NumOpenPublicConnections > 0)
		{
			Session.NumOpenPublicConnections--;
		}
		else if (Session.NumOpenPrivateConnections > 0)
		{
			Session.NumOpenPrivateConnections--;
		}
	}

	return !bAlreadyRegistered;
}

void FOnlineSessionSimplyStream::OnJoinSessionRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, FOnlineSessionSearchResult DesiredSession, FUniqueNetIdPtr PlayerId)
{
	EOnJoinSessionCompleteResult::Type Result = EOnJoinSessionCompleteResult::UnknownError;

	if (bWasSuccessful && Response.IsValid())
	{
		if (Response->GetResponseCode() == 200)
		{
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid() && JsonObject->GetBoolField(TEXT("success")))
			{
				const TSharedPtr<FJsonObject>* ResultObject = nullptr;
				if (JsonObject->TryGetObjectField(TEXT("result"), ResultObject) && ResultObject && (*ResultObject).IsValid())
				{
					const FString SessionId = (*ResultObject)->GetStringField(TEXT("sessionId"));
					FNamedOnlineSession* Session = GetNamedSession(SessionName);
					if (!Session)
					{
						Session = AddNamedSession(SessionName, DesiredSession.Session);
					}
					if (Session)
					{
						Session->SessionState = EOnlineSessionState::InProgress;
						Session->SessionInfo = MakeShareable(new FOnlineSessionInfoSimplyStream());
						StaticCastSharedPtr<FOnlineSessionInfoSimplyStream>(Session->SessionInfo)->SessionId = FUniqueNetIdSimplyStream::Create(SessionId);
						Session->RegisteredPlayers.Empty();
						if (PlayerId.IsValid())
						{
							BackendRegisteredPlayerKeys.Add(SimplyStreamSessions::MakeBackendMembershipKey(SessionId, *PlayerId));
						}

						bool bHaveEndpoint = false;
						const TSharedPtr<FJsonObject>* ConnectionInfo = nullptr;
						if ((*ResultObject)->TryGetObjectField(TEXT("connectionInfo"), ConnectionInfo) && ConnectionInfo && (*ConnectionInfo).IsValid())
						{
							bHaveEndpoint = SimplyStreamSessions::ApplyConnectionInfoToSession(*ConnectionInfo, *Session);
						}

						if (!bHaveEndpoint)
						{
							WndrMarkNetTravelStage(WNDR_NET_STAGE_OSS_JOIN_NO_ENDPOINT);
							// A join with NOTHING to travel to must FAIL, not "succeed": reporting Success here
							// let CommonSession travel on an empty connect string and left the player on an
							// infinite loading screen (Quick Play joining an endpoint-less zombie session).
							// Leave the membership we just acquired (fire-and-forget) and report the precise
							// failure so the UI returns to the menu.
							UE_LOG(LogOnlineSession, Warning, TEXT("JoinSession: session %s has no usable endpoint (no serverUrl / ip:port) — failing join instead of traveling nowhere"), *SessionId);
							LeaveSessionById(SessionId, PlayerId);
							RemoveNamedSession(SessionName);
							Result = EOnJoinSessionCompleteResult::CouldNotRetrieveAddress;
						}
						else
						{
							WndrMarkNetTravelStage(WNDR_NET_STAGE_OSS_JOIN_ENDPOINT_READY);
							Result = EOnJoinSessionCompleteResult::Success;
						}
					}
				}
			}
		}
		else
		{
			UE_LOG(LogOnlineSession, Warning, TEXT("JoinSession HTTP %d: %s"), Response->GetResponseCode(), *Response->GetContentAsString());
		}
	}

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		RemoveNamedSession(SessionName);
	}

	WndrMarkNetTravelStage(Result == EOnJoinSessionCompleteResult::Success
		? WNDR_NET_STAGE_OSS_JOIN_SUCCESS_DELEGATE
		: WNDR_NET_STAGE_OSS_JOIN_FAILURE_DELEGATE);
	TriggerOnJoinSessionCompleteDelegates(SessionName, Result);
}

void FOnlineSessionSimplyStream::OnUpdateSessionRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, TSharedPtr<FOnlineSessionSettings> PreviousSettings)
{
	bool bSuccess = false;
	if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		if (Response->GetContentLength() == 0)
		{
			bSuccess = true;
		}
		else
		{
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
			{
				bSuccess = JsonObject->GetBoolField(TEXT("success"));
			}
		}
	}

	if (!bSuccess && PreviousSettings.IsValid())
	{
		if (FNamedOnlineSession* Session = GetNamedSession(SessionName))
		{
			Session->SessionSettings = *PreviousSettings;
		}
	}

	if (!bSuccess)
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("UpdateSession: Request failed for '%s'"), *SessionName.ToString());
	}

	TriggerOnUpdateSessionCompleteDelegates(SessionName, bSuccess);
}

void FOnlineSessionSimplyStream::OnEndSessionRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName)
{
	const bool bSuccess = bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode());

	if (!bSuccess)
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("EndSession: Request failed for '%s' (HTTP %d) — ending locally anyway"),
			*SessionName.ToString(), Response.IsValid() ? Response->GetResponseCode() : -1);
	}

	// ALWAYS transition the local session to Ended and report success: Lyra retries a failed EndSession from
	// its completion handler, so reporting failure here loops the request forever (observed as 'end' spam
	// against a session the local user can't end). The backend row is reconciled by the owner/expiry instead.
	if (FNamedOnlineSession* Session = GetNamedSession(SessionName))
	{
		Session->SessionState = EOnlineSessionState::Ended;
	}

	TriggerOnEndSessionCompleteDelegates(SessionName, true);
}

void FOnlineSessionSimplyStream::TriggerFindSessionsCompleteNextTick(bool bWasSuccessful)
{
	SimplyStreamSubsystem->ExecuteNextTick([this, bWasSuccessful]()
	{
		TriggerOnFindSessionsCompleteDelegates(bWasSuccessful);
	});
}

void FOnlineSessionSimplyStream::CompleteFindSessionsOperation(const FString& QueryKey, TArray<FOnlineSessionSearchResult>&& Results, bool bWasSuccessful)
{
	TArray<TWeakPtr<FOnlineSessionSearch>> SearchesToComplete;
	{
		FScopeLock Lock(&FindSessionsLock);
		if (FPendingFindSessionsBatch* PendingBatch = PendingFindSessions.Find(QueryKey))
		{
			SearchesToComplete = MoveTemp(PendingBatch->Searches);
			PendingFindSessions.Remove(QueryKey);
		}

		if (bWasSuccessful)
		{
			if (!CachedFindSessions.Contains(QueryKey) && CachedFindSessions.Num() >= 16)
			{
				CachedFindSessions.Empty();
			}
			FCachedFindSessionsBatch& CacheEntry = CachedFindSessions.FindOrAdd(QueryKey);
			CacheEntry.Results = Results;
			CacheEntry.CachedAtSeconds = FPlatformTime::Seconds();
		}
	}

	int32 CompletedSearchCount = 0;
	for (const TWeakPtr<FOnlineSessionSearch>& SearchWeak : SearchesToComplete)
	{
		TSharedPtr<FOnlineSessionSearch> SearchSettings = SearchWeak.Pin();
		if (!SearchSettings.IsValid())
		{
			continue;
		}

		SearchSettings->SearchResults = Results;
		SearchSettings->SearchState = bWasSuccessful ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
		++CompletedSearchCount;
	}

	UE_LOG(LogOnlineSession, Verbose, TEXT("FindSessions: completed query batch (%s, %d result(s), %d waiter(s))"),
		bWasSuccessful ? TEXT("success") : TEXT("failure"), Results.Num(), CompletedSearchCount);

	if (CompletedSearchCount > 0)
	{
		TriggerFindSessionsCompleteNextTick(bWasSuccessful);
	}
}

void FOnlineSessionSimplyStream::OnFindSessionsRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString QueryKey)
{
	bool bSuccess = false;
	TArray<FOnlineSessionSearchResult> Results;

	if (bWasSuccessful && Response.IsValid())
	{
		if (Response->GetResponseCode() == 200)
		{
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid() && JsonObject->GetBoolField(TEXT("success")))
			{
				const TArray<TSharedPtr<FJsonValue>>* ResultsArray = nullptr;
				if (JsonObject->TryGetArrayField(TEXT("results"), ResultsArray) && ResultsArray)
				{
					for (const TSharedPtr<FJsonValue>& EntryValue : *ResultsArray)
					{
						const TSharedPtr<FJsonObject> Entry = EntryValue->AsObject();
						FOnlineSessionSearchResult SearchResult;
						if (SimplyStreamSessions::BuildSearchResultFromSessionJson(Entry, nullptr, SearchResult))
						{
							Results.Add(MoveTemp(SearchResult));
						}
					}

					bSuccess = true;
				}
			}
		}
		else
		{
			UE_LOG(LogOnlineSession, Warning, TEXT("FindSessions HTTP %d: %s"), Response->GetResponseCode(), *Response->GetContentAsString());
		}
	}

	CompleteFindSessionsOperation(QueryKey, MoveTemp(Results), bSuccess);
}

void FOnlineSessionSimplyStream::OnFindSessionByIdRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FOnSingleSessionResultCompleteDelegate CompletionDelegate)
{
	bool bSuccess = false;
	FOnlineSessionSearchResult SearchResult;

	if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			bSuccess = SimplyStreamSessions::ExtractSessionResultFromResponse(JsonObject, SearchResult);
		}
	}

	if (!bSuccess)
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("FindSessionById failed (HTTP %d): %s"),
			Response.IsValid() ? Response->GetResponseCode() : -1,
			Response.IsValid() ? *Response->GetContentAsString() : TEXT("no response"));
	}

	CompletionDelegate.ExecuteIfBound(LocalUserNum, bSuccess, SearchResult);
}

void FOnlineSessionSimplyStream::OnSendSessionInviteRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, FUniqueNetIdRef FriendId)
{
	const bool bSuccess = bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()) && IsSuccessfulJsonResponse(Response);
	if (bSuccess)
	{
		UE_LOG(LogOnlineSession, Log, TEXT("SendSessionInviteToFriend: invited %s to '%s'"), *FriendId->ToDebugString(), *SessionName.ToString());
	}
	else
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("SendSessionInviteToFriend failed for %s -> '%s' (HTTP %d): %s"),
			*FriendId->ToDebugString(),
			*SessionName.ToString(),
			Response.IsValid() ? Response->GetResponseCode() : -1,
			Response.IsValid() ? *Response->GetContentAsString() : TEXT("no response"));
	}
}

bool FOnlineSessionSimplyStream::StartMatchmaking(const TArray< FUniqueNetIdRef >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings) { return false; }
bool FOnlineSessionSimplyStream::CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName) { return false; }
bool FOnlineSessionSimplyStream::CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName) { return false; }
bool FOnlineSessionSimplyStream::FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	FUniqueNetIdPtr SearchingPlayerId = GetUniquePlayerId(SearchingPlayerNum);
	if (!SearchingPlayerId.IsValid())
	{
		return false;
	}
	return FindSessions(*SearchingPlayerId, SearchSettings);
}

bool FOnlineSessionSimplyStream::FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	auto FailSearchNextTick = [this, SearchSettings]()
	{
		SearchSettings->SearchResults.Empty();
		SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
		TriggerFindSessionsCompleteNextTick(false);
		return false;
	};

	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		return FailSearchNextTick();
	}

	const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, SearchingPlayerId);
	if (AuthToken.IsEmpty())
	{
		return FailSearchNextTick();
	}

	const FString ProjectId = SimplyStreamSessions::ExtractProjectId(SearchingPlayerId);
	if (ProjectId.IsEmpty())
	{
		return FailSearchNextTick();
	}

	SearchSettings->SearchResults.Empty();
	SearchSettings->SearchState = EOnlineAsyncTaskState::InProgress;

	FString Query;
	SimplyStreamSessions::AppendQueryParam(Query, SimplyStreamSessions::BuildQueryParam(TEXT("projectId"), ProjectId));

	const FString SessionType = SimplyStreamSessions::GetSessionSettingString(SearchSettings->QuerySettings, SimplyStreamSessions::SettingSessionTypeKey);
	if (!SessionType.IsEmpty())
	{
		SimplyStreamSessions::AppendQueryParam(Query, SimplyStreamSessions::BuildQueryParam(TEXT("sessionType"), SessionType));
	}

	const FString GameMode = SimplyStreamSessions::GetSessionSettingString(SearchSettings->QuerySettings, SETTING_GAMEMODE);
	if (!GameMode.IsEmpty())
	{
		SimplyStreamSessions::AppendQueryParam(Query, SimplyStreamSessions::BuildQueryParam(TEXT("gameMode"), GameMode));
	}

	const FString MapName = SimplyStreamSessions::GetSessionSettingString(SearchSettings->QuerySettings, SETTING_MAPNAME);
	if (!MapName.IsEmpty())
	{
		SimplyStreamSessions::AppendQueryParam(Query, SimplyStreamSessions::BuildQueryParam(TEXT("mapName"), MapName));
	}

	const FString Region = SimplyStreamSessions::GetSessionSettingString(SearchSettings->QuerySettings, SETTING_REGION);
	if (!Region.IsEmpty())
	{
		SimplyStreamSessions::AppendQueryParam(Query, SimplyStreamSessions::BuildQueryParam(TEXT("region"), Region));
	}

	const FString Keywords = SimplyStreamSessions::GetSessionSettingString(SearchSettings->QuerySettings, SEARCH_KEYWORDS);
	if (!Keywords.IsEmpty())
	{
		SimplyStreamSessions::AppendQueryParam(Query, SimplyStreamSessions::BuildQueryParam(TEXT("keywords"), Keywords));
	}

	const FString FriendUserId = SimplyStreamSessions::GetSessionSettingString(SearchSettings->QuerySettings, SimplyStreamSessions::SettingFriendUserIdKey);
	if (!FriendUserId.IsEmpty())
	{
		SimplyStreamSessions::AppendQueryParam(Query, SimplyStreamSessions::BuildQueryParam(TEXT("friendUserId"), FriendUserId));
	}

	const FString MatchId = SimplyStreamSessions::GetSessionSettingString(SearchSettings->QuerySettings, SimplyStreamSessions::SettingMatchIdKey);
	if (!MatchId.IsEmpty())
	{
		SimplyStreamSessions::AppendQueryParam(Query, SimplyStreamSessions::BuildQueryParam(TEXT("matchId"), MatchId));
	}

	const FString InviteId = SimplyStreamSessions::GetSessionSettingString(SearchSettings->QuerySettings, SimplyStreamSessions::SettingInviteIdKey);
	if (!InviteId.IsEmpty())
	{
		SimplyStreamSessions::AppendQueryParam(Query, SimplyStreamSessions::BuildQueryParam(TEXT("inviteId"), InviteId));
	}

	int32 MinSlots = 0;
	if (SimplyStreamSessions::GetSessionSettingValue(SearchSettings->QuerySettings, SEARCH_MINSLOTSAVAILABLE, MinSlots) && MinSlots > 0)
	{
		SimplyStreamSessions::AppendQueryParam(Query, SimplyStreamSessions::BuildQueryParam(TEXT("minSlots"), FString::FromInt(MinSlots)));
	}

	bool bIncludeFullSessions = false;
	SimplyStreamSessions::GetSessionSettingValue(SearchSettings->QuerySettings, SimplyStreamSessions::SettingIncludeFullKey, bIncludeFullSessions);
	if (bIncludeFullSessions)
	{
		SimplyStreamSessions::AppendQueryParam(Query, TEXT("includeFullSessions=true"));
	}

	bool bIncludeInvites = false;
	SimplyStreamSessions::GetSessionSettingValue(SearchSettings->QuerySettings, SimplyStreamSessions::SettingIncludeInvitesKey, bIncludeInvites);
	if (bIncludeInvites)
	{
		SimplyStreamSessions::AppendQueryParam(Query, TEXT("includeInvites=true"));
	}

	const int32 Limit = FMath::Clamp(SearchSettings->MaxSearchResults > 0 ? SearchSettings->MaxSearchResults : 20, 1, 100);
	SimplyStreamSessions::AppendQueryParam(Query, FString::Printf(TEXT("limit=%d"), Limit));
	const FString QueryKey = Query;

	TArray<FOnlineSessionSearchResult> CachedResults;
	bool bUseCachedResults = false;
	{
		FScopeLock Lock(&FindSessionsLock);
		if (const FCachedFindSessionsBatch* CacheEntry = CachedFindSessions.Find(QueryKey))
		{
			if (FPlatformTime::Seconds() - CacheEntry->CachedAtSeconds <= SimplyStreamSessions::FindSessionsCacheTtlSeconds)
			{
				CachedResults = CacheEntry->Results;
				bUseCachedResults = true;
			}
		}
	}

	if (bUseCachedResults)
	{
		SearchSettings->SearchResults = MoveTemp(CachedResults);
		SearchSettings->SearchState = EOnlineAsyncTaskState::Done;
		UE_LOG(LogOnlineSession, Verbose, TEXT("FindSessions: served cached query (%d result(s))"), SearchSettings->SearchResults.Num());
		TriggerFindSessionsCompleteNextTick(true);
		return true;
	}

	bool bStartBackendRequest = false;
	{
		FScopeLock Lock(&FindSessionsLock);
		FPendingFindSessionsBatch& PendingBatch = PendingFindSessions.FindOrAdd(QueryKey);
		bStartBackendRequest = PendingBatch.Searches.Num() == 0;
		PendingBatch.Searches.Add(SearchSettings);
	}

	if (!bStartBackendRequest)
	{
		UE_LOG(LogOnlineSession, Verbose, TEXT("FindSessions: coalesced duplicate query while backend request is in flight"));
		return true;
	}

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/sessions") + Query);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSessionSimplyStream::OnFindSessionsRequestComplete, QueryKey);

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("FindSessions: Failed to send HTTP request"));
		CompleteFindSessionsOperation(QueryKey, TArray<FOnlineSessionSearchResult>(), false);
		return false;
	}

	return true;
}
bool FOnlineSessionSimplyStream::FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate)
{
	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, SearchingUserId);

	auto FailNextTick = [this, LocalUserNum, CompletionDelegate]()
	{
		SimplyStreamSubsystem->ExecuteNextTick([LocalUserNum, CompletionDelegate]()
		{
			FOnlineSessionSearchResult EmptyResult;
			CompletionDelegate.ExecuteIfBound(LocalUserNum != INDEX_NONE ? LocalUserNum : 0, false, EmptyResult);
		});
		return false;
	};

	if (!IdentityInt.IsValid())
	{
		return FailNextTick();
	}

	const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, SearchingUserId);
	if (AuthToken.IsEmpty())
	{
		return FailNextTick();
	}

	const FString SessionIdStr = SessionId.ToString();
	if (SessionIdStr.IsEmpty())
	{
		return FailNextTick();
	}

	FString Query;
	const FString ProjectId = SimplyStreamSessions::ExtractProjectId(SearchingUserId);
	if (!ProjectId.IsEmpty())
	{
		SimplyStreamSessions::AppendQueryParam(Query, SimplyStreamSessions::BuildQueryParam(TEXT("projectId"), ProjectId));
	}
	if (FriendId.IsValid() && !FriendId.ToString().IsEmpty())
	{
		SimplyStreamSessions::AppendQueryParam(Query, SimplyStreamSessions::BuildQueryParam(TEXT("friendUserId"), FriendId.ToString()));
	}

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/player/sessions/%s"), *FGenericPlatformHttp::UrlEncode(SessionIdStr)) + Query);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSessionSimplyStream::OnFindSessionByIdRequestComplete, LocalUserNum != INDEX_NONE ? LocalUserNum : 0, CompletionDelegate);

	if (!HttpRequest->ProcessRequest())
	{
		return FailNextTick();
	}

	return true;
}
bool FOnlineSessionSimplyStream::CancelFindSessions()
{
	FScopeLock Lock(&FindSessionsLock);
	const bool bHadPendingSearches = PendingFindSessions.Num() > 0;
	PendingFindSessions.Empty();
	return bHadPendingSearches;
}
bool FOnlineSessionSimplyStream::PingSearchResults(const FOnlineSessionSearchResult& SearchResult) { return false; }

bool FOnlineSessionSimplyStream::FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend)
{
	FUniqueNetIdPtr LocalUserId = GetUniquePlayerId(LocalUserNum);
	if (!LocalUserId.IsValid())
	{
		TriggerFindFriendSessionFailure(LocalUserNum);
		return false;
	}

	FOnlinePresenceSimplyStreamPtr Presence = StaticCastSharedPtr<FOnlinePresenceSimplyStream>(SimplyStreamSubsystem->GetPresenceInterface());
	if (!Presence.IsValid())
	{
		TriggerFindFriendSessionFailure(LocalUserNum);
		return false;
	}

	FUniqueNetIdPtr SearchingUserIdCopy = FUniqueNetIdSimplyStream::CreateFrom(*LocalUserId);
	FUniqueNetIdPtr FriendIdCopy = FUniqueNetIdSimplyStream::CreateFrom(Friend);
	Presence->QueryPresence(*FriendIdCopy, IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateLambda(
		[this, LocalUserNum, SearchingUserIdCopy, FriendIdCopy](const FUniqueNetId&, bool bWasSuccessful)
		{
			FOnlinePresenceSimplyStreamPtr PresenceInt = StaticCastSharedPtr<FOnlinePresenceSimplyStream>(SimplyStreamSubsystem->GetPresenceInterface());
			TSharedPtr<FOnlineUserPresence> FriendPresence;
			if (!bWasSuccessful || !PresenceInt.IsValid() || PresenceInt->GetCachedPresence(*FriendIdCopy, FriendPresence) != EOnlineCachedResult::Success || !FriendPresence.IsValid() || !FriendPresence->SessionId.IsValid())
			{
				TriggerFindFriendSessionFailure(LocalUserNum);
				return;
			}

			FindSessionById(*SearchingUserIdCopy, *FriendPresence->SessionId, *FriendIdCopy, FOnSingleSessionResultCompleteDelegate::CreateLambda(
				[this, LocalUserNum](int32, bool bFoundSession, const FOnlineSessionSearchResult& SearchResult)
				{
					TArray<FOnlineSessionSearchResult> FriendResults;
					if (bFoundSession)
					{
						FriendResults.Add(SearchResult);
					}
					TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, bFoundSession, FriendResults);
				}));
		}));

	return true;
}

bool FOnlineSessionSimplyStream::FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend)
{
	const int32 LocalUserNum = GetLocalUserIdx(LocalUserId);
	if (LocalUserNum == INDEX_NONE)
	{
		TriggerFindFriendSessionFailure(0);
		return false;
	}
	return FindFriendSession(LocalUserNum, Friend);
}

bool FOnlineSessionSimplyStream::FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<FUniqueNetIdRef>& FriendList)
{
	if (FriendList.Num() == 0)
	{
		const int32 LocalUserNum = GetLocalUserIdx(LocalUserId);
		TriggerFindFriendSessionFailure(LocalUserNum != INDEX_NONE ? LocalUserNum : 0);
		return false;
	}

	return FindFriendSession(LocalUserId, *FriendList[0]);
}

bool FOnlineSessionSimplyStream::SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend)
{
	TArray<FUniqueNetIdRef> Friends;
	Friends.Add(Friend.AsShared());
	return SendSessionInviteToFriends(LocalUserNum, SessionName, Friends);
}

bool FOnlineSessionSimplyStream::SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend)
{
	TArray<FUniqueNetIdRef> Friends;
	Friends.Add(Friend.AsShared());
	return SendSessionInviteToFriends(LocalUserId, SessionName, Friends);
}

bool FOnlineSessionSimplyStream::SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< FUniqueNetIdRef >& Friends)
{
	FUniqueNetIdPtr LocalUserId = GetUniquePlayerId(LocalUserNum);
	return LocalUserId.IsValid() ? SendSessionInviteToFriends(*LocalUserId, SessionName, Friends) : false;
}

bool FOnlineSessionSimplyStream::SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< FUniqueNetIdRef >& Friends)
{
	if (Friends.Num() == 0)
	{
		return false;
	}

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("SendSessionInviteToFriends: Session '%s' not found"), *SessionName.ToString());
		return false;
	}

	TSharedPtr<FOnlineSessionInfoSimplyStream> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoSimplyStream>(Session->SessionInfo);
	if (!SessionInfo.IsValid() || !SessionInfo->SessionId.IsValid())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("SendSessionInviteToFriends: Session '%s' has no backend session id"), *SessionName.ToString());
		return false;
	}

	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, LocalUserId);
	if (AuthToken.IsEmpty())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("SendSessionInviteToFriends: Missing auth token"));
		return false;
	}

	const FString SessionId = SessionInfo->SessionId->ToString();
	FString SessionType;
	SimplyStreamSessions::GetSessionSettingValue(Session->SessionSettings, SimplyStreamSessions::SettingSessionTypeKey, SessionType);

	bool bStartedAny = false;
	for (const FUniqueNetIdRef& FriendId : Friends)
	{
		TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
		RequestJson->SetStringField(TEXT("friendUserId"), FriendId->ToString());
		RequestJson->SetStringField(TEXT("recipientUserId"), FriendId->ToString());
		RequestJson->SetStringField(TEXT("sessionId"), SessionId);
		RequestJson->SetStringField(TEXT("sessionName"), SessionName.ToString());
		if (!SessionType.IsEmpty())
		{
			RequestJson->SetStringField(TEXT("sessionType"), SessionType);
		}

		FString RequestContent;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
		FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

		TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/player/sessions/%s/invites"), *FGenericPlatformHttp::UrlEncode(SessionId)));
		HttpRequest->SetVerb(TEXT("POST"));
		HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
		HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		HttpRequest->SetContentAsString(RequestContent);
		HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSessionSimplyStream::OnSendSessionInviteRequestComplete, SessionName, FriendId);

		if (HttpRequest->ProcessRequest())
		{
			bStartedAny = true;
		}
		else
		{
			UE_LOG(LogOnlineSession, Warning, TEXT("SendSessionInviteToFriends: Failed to start invite request for %s"), *FriendId->ToDebugString());
		}
	}

	return bStartedAny;
}
namespace
{
	inline bool ResolveConnectStringFromSession(const FOnlineSession& Session, FString& OutConnectInfo)
	{
		FString JoinInfo;
		if (Session.SessionSettings.Get(SETTING_CUSTOM_JOIN_INFO, JoinInfo) && !JoinInfo.IsEmpty())
		{
			OutConnectInfo = JoinInfo;
			return true;
		}

		if (Session.SessionInfo.IsValid())
		{
			const FOnlineSessionInfoSimplyStream* SessionInfo = static_cast<const FOnlineSessionInfoSimplyStream*>(Session.SessionInfo.Get());
			if (SessionInfo && SessionInfo->HostAddr.IsValid() && SessionInfo->HostAddr->IsValid())
			{
				// Include the port: ToString(false) dropped it, so travel fell back to the URL default (7777)
				// instead of the advertised endpoint port.
				OutConnectInfo = SessionInfo->HostAddr->ToString(true);
				return true;
			}
		}

		return false;
	}
}

bool FOnlineSessionSimplyStream::GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType)
{
	if (FNamedOnlineSession* Session = GetNamedSession(SessionName))
	{
		const bool bResolved = ResolveConnectStringFromSession(*Session, ConnectInfo);
		WndrMarkNetTravelStage(bResolved
			? WNDR_NET_STAGE_OSS_RESOLVE_CONNECT_STRING
			: WNDR_NET_STAGE_OSS_RESOLVE_CONNECT_STRING_FAILED);
		return bResolved;
	}
	WndrMarkNetTravelStage(WNDR_NET_STAGE_OSS_RESOLVE_CONNECT_STRING_FAILED);
	return false;
}

bool FOnlineSessionSimplyStream::GetResolvedConnectString(const FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo)
{
	const bool bResolved = ResolveConnectStringFromSession(SearchResult.Session, ConnectInfo);
	WndrMarkNetTravelStage(bResolved
		? WNDR_NET_STAGE_OSS_RESOLVE_CONNECT_STRING
		: WNDR_NET_STAGE_OSS_RESOLVE_CONNECT_STRING_FAILED);
	return bResolved;
}

int32 FOnlineSessionSimplyStream::GetLocalUserIdx(const FUniqueNetId& UserId) const
{
	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	return SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
}

bool FOnlineSessionSimplyStream::JoinMatchmakingQueue(const FUniqueNetId& UserId, const FString& ConfigId, const FString& QueueType, const FString& GameMode)
{
	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		return false;
	}

	int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
	FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
	if (AuthToken.IsEmpty())
	{
		return false;
	}

	// Build matchmaking request
	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	RequestJson->SetStringField(TEXT("configId"), ConfigId);
	RequestJson->SetStringField(TEXT("queueType"), QueueType);
	RequestJson->SetStringField(TEXT("gameMode"), GameMode);

	FString RequestContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/matchmaking/queue"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestContent);

	FName SessionName(TEXT("Matchmaking"));
	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSessionSimplyStream::OnStartMatchmakingComplete, UserId.AsShared(), SessionName);
	
	return HttpRequest->ProcessRequest();
}

bool FOnlineSessionSimplyStream::LeaveMatchmakingQueue(const FUniqueNetId& UserId)
{
	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		return false;
	}

	int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
	FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
	if (AuthToken.IsEmpty())
	{
		return false;
	}

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/matchmaking/queue"));
	HttpRequest->SetVerb(TEXT("DELETE"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSessionSimplyStream::OnCancelMatchmakingComplete, UserId.AsShared());
	
	return HttpRequest->ProcessRequest();
}

bool FOnlineSessionSimplyStream::GetMatchmakingStatus(const FUniqueNetId& UserId)
{
	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		return false;
	}

	int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
	FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
	if (AuthToken.IsEmpty())
	{
		return false;
	}

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/matchmaking/status"));
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// For simplicity, we'll log the response. In production, you'd trigger specific delegates
	HttpRequest->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		if (bWasSuccessful && Response.IsValid())
		{
			UE_LOG(LogOnlineSession, Log, TEXT("Matchmaking status: %s"), *Response->GetContentAsString());
		}
	});
	
	return HttpRequest->ProcessRequest();
}

bool FOnlineSessionSimplyStream::RequestBackfill(const FUniqueNetId& UserId, const FString& MatchId, int32 SlotsNeeded)
{
	IOnlineIdentityPtr IdentityInt = SimplyStreamSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		return false;
	}

	int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
	FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
	if (AuthToken.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
	RequestJson->SetStringField(TEXT("matchId"), MatchId);
	RequestJson->SetNumberField(TEXT("slotsNeeded"), SlotsNeeded);

	FString RequestContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(SimplyStreamSubsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/matchmaking/backfill"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestContent);

	HttpRequest->OnProcessRequestComplete().BindLambda([MatchId](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		if (bWasSuccessful && Response.IsValid())
		{
			UE_LOG(LogOnlineSession, Log, TEXT("Backfill requested for match %s: %s"), *MatchId, *Response->GetContentAsString());
		}
	});
	
	return HttpRequest->ProcessRequest();
}

void FOnlineSessionSimplyStream::OnStartMatchmakingComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FName SessionName)
{
	bool bSuccess = false;
	
	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 201)
	{
		FString ResponseContent = Response->GetContentAsString();
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			bSuccess = JsonObject->GetBoolField(TEXT("success"));
			if (bSuccess)
			{
				// Store queue ID for potential cancellation
				TSharedPtr<FJsonObject> ResultObject = JsonObject->GetObjectField(TEXT("result"));
				if (ResultObject.IsValid())
				{
					FString QueueId = ResultObject->GetStringField(TEXT("queueId"));
					ActiveMatchmakingRequests.Add(UserId, QueueId);
					UE_LOG(LogOnlineSession, Log, TEXT("Matchmaking started - Queue ID: %s"), *QueueId);
				}
			}
		}
	}

	TriggerOnMatchmakingCompleteDelegates(SessionName, bSuccess);
}

void FOnlineSessionSimplyStream::OnCancelMatchmakingComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId)
{
	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
	{
		ActiveMatchmakingRequests.Remove(UserId);
		UE_LOG(LogOnlineSession, Log, TEXT("Matchmaking cancelled for user: %s"), *UserId->ToDebugString());
	}

	FName SessionName(TEXT("Matchmaking"));
	TriggerOnCancelMatchmakingCompleteDelegates(SessionName, bWasSuccessful);
}

void FOnlineSessionSimplyStream::CompleteRegisterPlayersOperation(const TSharedPtr<FPendingRegisterPlayersOperation>& Operation)
{
	WndrMarkNetTravelStage(WNDR_NET_STAGE_OSS_REGISTER_COMPLETE);
	if (!Operation.IsValid())
	{
		return;
	}

	const bool bAllSuccess = (Operation->Players.Num() > 0) &&
		(Operation->Results.Num() == Operation->Players.Num()) &&
		Algo::AllOf(Operation->Results, [](bool b) { return b; });

	for (int32 PlayerIdx = 0; PlayerIdx < Operation->Players.Num(); ++PlayerIdx)
	{
		const bool bPlayerSuccess = Operation->Results.IsValidIndex(PlayerIdx) ? Operation->Results[PlayerIdx] : false;
		const FString& Error = Operation->Errors.IsValidIndex(PlayerIdx) ? Operation->Errors[PlayerIdx] : FString();
		RegisterPlayersDetailedEvent.Broadcast(Operation->SessionName, *Operation->Players[PlayerIdx], bPlayerSuccess, Error);
	}

	TriggerOnRegisterPlayersCompleteDelegates(Operation->SessionName, Operation->Players, bAllSuccess);
}

void FOnlineSessionSimplyStream::CompleteUnregisterPlayersOperation(const TSharedPtr<FPendingUnregisterPlayersOperation>& Operation)
{
	if (!Operation.IsValid())
	{
		return;
	}

	const bool bAllSuccess = (Operation->Players.Num() > 0) &&
		(Operation->Results.Num() == Operation->Players.Num()) &&
		Algo::AllOf(Operation->Results, [](bool b) { return b; });

	for (int32 PlayerIdx = 0; PlayerIdx < Operation->Players.Num(); ++PlayerIdx)
	{
		const bool bPlayerSuccess = Operation->Results.IsValidIndex(PlayerIdx) ? Operation->Results[PlayerIdx] : false;
		const FString& Error = Operation->Errors.IsValidIndex(PlayerIdx) ? Operation->Errors[PlayerIdx] : FString();
		UnregisterPlayersDetailedEvent.Broadcast(Operation->SessionName, *Operation->Players[PlayerIdx], bPlayerSuccess, Error);
	}

	TriggerOnUnregisterPlayersCompleteDelegates(Operation->SessionName, Operation->Players, bAllSuccess);
}

void FOnlineSessionSimplyStream::BroadcastRegisterPlayersDetailed(FName SessionName, const TArray<FUniqueNetIdRef>& Players, bool bWasSuccessful, const FString& Error)
{
	for (const FUniqueNetIdRef& PlayerId : Players)
	{
		RegisterPlayersDetailedEvent.Broadcast(SessionName, *PlayerId, bWasSuccessful, Error);
	}
}

void FOnlineSessionSimplyStream::BroadcastUnregisterPlayersDetailed(FName SessionName, const TArray<FUniqueNetIdRef>& Players, bool bWasSuccessful, const FString& Error)
{
	for (const FUniqueNetIdRef& PlayerId : Players)
	{
		UnregisterPlayersDetailedEvent.Broadcast(SessionName, *PlayerId, bWasSuccessful, Error);
	}
}

void FOnlineSessionSimplyStream::TriggerFindFriendSessionFailure(int32 LocalUserNum)
{
	SimplyStreamSubsystem->ExecuteNextTick([this, LocalUserNum]()
	{
		TArray<FOnlineSessionSearchResult> EmptyResults;
		TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, EmptyResults);
	});
}

void FOnlineSessionSimplyStream::TrackPresenceForPlayer(const FUniqueNetId& PlayerId)
{
	if (!SimplyStreamSubsystem || !PlayerId.IsValid())
	{
		return;
	}

	FOnlinePresenceSimplyStreamPtr Presence = StaticCastSharedPtr<FOnlinePresenceSimplyStream>(SimplyStreamSubsystem->GetPresenceInterface());
	if (Presence.IsValid())
	{
		Presence->TrackRemoteUser(PlayerId);
	}
}

void FOnlineSessionSimplyStream::UntrackPresenceForPlayer(const FUniqueNetId& PlayerId)
{
	if (!SimplyStreamSubsystem || !PlayerId.IsValid())
	{
		return;
	}

	FOnlinePresenceSimplyStreamPtr Presence = StaticCastSharedPtr<FOnlinePresenceSimplyStream>(SimplyStreamSubsystem->GetPresenceInterface());
	if (Presence.IsValid())
	{
		Presence->UntrackRemoteUser(PlayerId);
	}
}
