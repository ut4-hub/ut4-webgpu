// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineVoiceSimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineIdentitySimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FOnlineVoiceImplSimplyStream::FOnlineVoiceImplSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
    : Subsystem(InSubsystem)
    , bIsLocalPlayerTalking(false)
    , bIsVoiceNetworked(false)
{
}

bool FOnlineVoiceImplSimplyStream::Init()
{
    UE_LOG(LogOnlineVoice, Log, TEXT("SimplyStream Voice: Initialized"));
    return true;
}

void FOnlineVoiceImplSimplyStream::StartNetworkedVoice(uint8 LocalUserNum)
{
    bIsVoiceNetworked = true;
    UE_LOG(LogOnlineVoice, Log, TEXT("SimplyStream Voice: Starting networked voice for user %d"), LocalUserNum);
}

void FOnlineVoiceImplSimplyStream::StopNetworkedVoice(uint8 LocalUserNum)
{
    bIsVoiceNetworked = false;
    UE_LOG(LogOnlineVoice, Log, TEXT("SimplyStream Voice: Stopping networked voice for user %d"), LocalUserNum);
    
    // Leave all active voice channels
    for (const auto& TokenPair : ActiveVoiceTokens)
    {
        IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
        if (IdentityInt.IsValid())
        {
            FUniqueNetIdPtr UserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
            if (UserId.IsValid())
            {
                LeaveVoiceChannel(*UserId, TokenPair.Key);
            }
        }
    }
}

bool FOnlineVoiceImplSimplyStream::RegisterLocalTalker(uint32 LocalUserNum)
{
    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
    if (Identity.IsValid())
    {
        FUniqueNetIdPtr UserId = Identity->GetUniquePlayerId(LocalUserNum);
        if (UserId.IsValid())
        {
            bIsLocalPlayerTalking = true;
            TriggerOnPlayerTalkingStateChangedDelegates(UserId.ToSharedRef(), true);
            UE_LOG(LogOnlineVoice, Verbose, TEXT("Local talker registered: %s"), *UserId->ToDebugString());
            return true;
        }
    }
    return false;
}

void FOnlineVoiceImplSimplyStream::RegisterLocalTalkers()
{
    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
    if (!Identity.IsValid())
    {
        return;
    }

    for (int32 LocalUserNum = 0; LocalUserNum < MAX_LOCAL_PLAYERS; ++LocalUserNum)
    {
        if (Identity->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
        {
            RegisterLocalTalker(LocalUserNum);
        }
    }
}

bool FOnlineVoiceImplSimplyStream::UnregisterLocalTalker(uint32 LocalUserNum)
{
    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
    if (Identity.IsValid())
    {
        FUniqueNetIdPtr UserId = Identity->GetUniquePlayerId(LocalUserNum);
        if (UserId.IsValid())
        {
            bIsLocalPlayerTalking = false;
            TriggerOnPlayerTalkingStateChangedDelegates(UserId.ToSharedRef(), false);
            UE_LOG(LogOnlineVoice, Verbose, TEXT("Local talker unregistered: %s"), *UserId->ToDebugString());
            return true;
        }
    }
    return false;
}

void FOnlineVoiceImplSimplyStream::UnregisterLocalTalkers()
{
    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
    if (!Identity.IsValid())
    {
        return;
    }

    for (int32 LocalUserNum = 0; LocalUserNum < MAX_LOCAL_PLAYERS; ++LocalUserNum)
    {
        if (Identity->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
        {
            UnregisterLocalTalker(LocalUserNum);
        }
    }
}

bool FOnlineVoiceImplSimplyStream::RegisterRemoteTalker(const FUniqueNetId& UniqueId)
{
    if (!RemoteTalkers.Contains(UniqueId.AsShared()))
    {
        FRemoteTalker Talker;
        Talker.TalkerId = UniqueId.AsShared();
        RemoteTalkers.Add(UniqueId.AsShared(), Talker);
        UE_LOG(LogOnlineVoice, Verbose, TEXT("Remote talker registered: %s"), *UniqueId.ToDebugString());
    }
    return true;
}

bool FOnlineVoiceImplSimplyStream::UnregisterRemoteTalker(const FUniqueNetId& UniqueId)
{
    if (RemoteTalkers.Remove(UniqueId.AsShared()) > 0)
    {
        UE_LOG(LogOnlineVoice, Verbose, TEXT("Remote talker unregistered: %s"), *UniqueId.ToDebugString());
    }
    return true;
}

void FOnlineVoiceImplSimplyStream::RemoveAllRemoteTalkers()
{
    RemoteTalkers.Empty();
    MutedTalkerIds.Empty();
    UE_LOG(LogOnlineVoice, Log, TEXT("All remote talkers removed"));
}

bool FOnlineVoiceImplSimplyStream::IsHeadsetPresent(uint32 LocalUserNum)
{
    // TODO: Integrate with platform-specific audio device detection
    return true;
}

bool FOnlineVoiceImplSimplyStream::IsLocalPlayerTalking(uint32 LocalUserNum)
{
    return bIsLocalPlayerTalking;
}

bool FOnlineVoiceImplSimplyStream::IsRemotePlayerTalking(const FUniqueNetId& UniqueId)
{
    if (const FRemoteTalker* Talker = RemoteTalkers.Find(UniqueId.AsShared()))
    {
        return Talker->bIsTalking;
    }
    return false;
}

bool FOnlineVoiceImplSimplyStream::IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const
{
    return MutedTalkerIds.Contains(UniqueId.ToString());
}

bool FOnlineVoiceImplSimplyStream::MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
    MutedTalkerIds.Add(PlayerId.ToString());
    UE_LOG(LogOnlineVoice, Log, TEXT("Muted player %s"), *PlayerId.ToDebugString());
    return true;
}

bool FOnlineVoiceImplSimplyStream::UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
    MutedTalkerIds.Remove(PlayerId.ToString());
    UE_LOG(LogOnlineVoice, Log, TEXT("Unmuted player %s"), *PlayerId.ToDebugString());
    return true;
}

TSharedPtr<FVoicePacket> FOnlineVoiceImplSimplyStream::GetLocalPacket(uint32 LocalUserNum)
{
    // Voice data would be handled by external voice provider (Agora, Vivox)
    // This interface primarily manages channel access and permissions
    return nullptr;
}

TSharedPtr<FVoicePacket> FOnlineVoiceImplSimplyStream::SerializeRemotePacket(FArchive& Ar)
{
    // Voice data handled by external provider
    return nullptr;
}

int32 FOnlineVoiceImplSimplyStream::GetNumLocalTalkers()
{
    return bIsLocalPlayerTalking ? 1 : 0;
}

void FOnlineVoiceImplSimplyStream::ClearVoicePackets()
{
    UE_LOG(LogOnlineVoice, VeryVerbose, TEXT("SimplyStream Voice: ClearVoicePackets noop"));
}

void FOnlineVoiceImplSimplyStream::Tick(float DeltaTime)
{
    (void)DeltaTime;
}

FString FOnlineVoiceImplSimplyStream::GetVoiceDebugState() const
{
    return FString::Printf(TEXT("SimplyStream Voice | networked=%s | remote=%d | channels=%d | muted=%d"),
        bIsVoiceNetworked ? TEXT("true") : TEXT("false"),
        RemoteTalkers.Num(),
        VoiceChannels.Num(),
        MutedTalkerIds.Num());
}

void FOnlineVoiceImplSimplyStream::ClearAllMutes()
{
    MutedTalkerIds.Empty();
    UE_LOG(LogOnlineVoice, Log, TEXT("All mutes cleared"));
}

FString FOnlineVoiceImplSimplyStream::GetVoiceDataObjectId(const FUniqueNetId& InUniqueNetId) const
{
    return InUniqueNetId.ToString();
}

IVoiceEnginePtr FOnlineVoiceImplSimplyStream::CreateVoiceEngine()
{
    return IVoiceEnginePtr();
}

void FOnlineVoiceImplSimplyStream::ProcessMuteChangeNotification()
{
    UE_LOG(LogOnlineVoice, VeryVerbose, TEXT("SimplyStream Voice: ProcessMuteChangeNotification noop"));
}

bool FOnlineVoiceImplSimplyStream::CreateVoiceChannel(const FUniqueNetId& UserId, const FString& ChannelType, const FString& SessionId, const FString& MatchId)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("channelType"), ChannelType);
    if (!SessionId.IsEmpty())
    {
        RequestJson->SetStringField(TEXT("sessionId"), SessionId);
    }
    if (!MatchId.IsEmpty())
    {
        RequestJson->SetStringField(TEXT("matchId"), MatchId);
    }
    RequestJson->SetStringField(TEXT("provider"), TEXT("agora")); // Default to Agora
    RequestJson->SetNumberField(TEXT("maxParticipants"), 20);

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/voice/channels"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineVoiceImplSimplyStream::OnCreateChannelComplete, UserId.AsShared(), ChannelType);
    
    return HttpRequest->ProcessRequest();
}

bool FOnlineVoiceImplSimplyStream::JoinVoiceChannel(const FUniqueNetId& UserId, const FString& ChannelId)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/voice/channels/%s/join"), *ChannelId));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineVoiceImplSimplyStream::OnJoinChannelComplete, UserId.AsShared(), ChannelId);
    
    return HttpRequest->ProcessRequest();
}

bool FOnlineVoiceImplSimplyStream::LeaveVoiceChannel(const FUniqueNetId& UserId, const FString& ChannelId)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/voice/channels/%s/leave"), *ChannelId));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineVoiceImplSimplyStream::OnLeaveChannelComplete, UserId.AsShared(), ChannelId);
    
    return HttpRequest->ProcessRequest();
}

bool FOnlineVoiceImplSimplyStream::GetVoiceToken(const FUniqueNetId& UserId, const FString& ChannelId)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("tokenType"), TEXT("participant"));
    RequestJson->SetNumberField(TEXT("expirationMinutes"), 60);

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/voice/channels/%s/token"), *ChannelId));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineVoiceImplSimplyStream::OnVoiceTokenComplete, UserId.AsShared(), ChannelId);
    
    return HttpRequest->ProcessRequest();
}

void FOnlineVoiceImplSimplyStream::OnCreateChannelComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString ChannelType)
{
    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 201)
    {
        FString ResponseContent = Response->GetContentAsString();
        ProcessVoiceChannelResponse(ResponseContent, ChannelType);
        UE_LOG(LogOnlineVoice, Log, TEXT("Voice channel created successfully for type: %s"), *ChannelType);
    }
    else
    {
        UE_LOG(LogOnlineVoice, Warning, TEXT("Failed to create voice channel: %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }
}

void FOnlineVoiceImplSimplyStream::OnJoinChannelComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString ChannelId)
{
    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        // User successfully joined voice channel
        // Now get a voice token for this channel
        GetVoiceToken(*UserId, ChannelId);
        UE_LOG(LogOnlineVoice, Log, TEXT("Joined voice channel: %s"), *ChannelId);
    }
    else
    {
        UE_LOG(LogOnlineVoice, Warning, TEXT("Failed to join voice channel %s: %d"), *ChannelId, Response.IsValid() ? Response->GetResponseCode() : 0);
    }
}

void FOnlineVoiceImplSimplyStream::OnLeaveChannelComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString ChannelId)
{
    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        // Remove cached token
        ActiveVoiceTokens.Remove(ChannelId);
        UE_LOG(LogOnlineVoice, Log, TEXT("Left voice channel: %s"), *ChannelId);
    }
    else
    {
        UE_LOG(LogOnlineVoice, Warning, TEXT("Failed to leave voice channel %s: %d"), *ChannelId, Response.IsValid() ? Response->GetResponseCode() : 0);
    }
}

void FOnlineVoiceImplSimplyStream::OnVoiceTokenComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString ChannelId)
{
    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 201)
    {
        FString ResponseContent = Response->GetContentAsString();
        ProcessVoiceTokenResponse(ResponseContent, ChannelId);
        UE_LOG(LogOnlineVoice, Log, TEXT("Voice token created for channel: %s"), *ChannelId);
    }
    else
    {
        UE_LOG(LogOnlineVoice, Warning, TEXT("Failed to create voice token for channel %s: %d"), *ChannelId, Response.IsValid() ? Response->GetResponseCode() : 0);
    }
}

void FOnlineVoiceImplSimplyStream::ProcessVoiceChannelResponse(const FString& ResponseContent, const FString& ChannelType)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        bool bApiSuccess = JsonObject->GetBoolField(TEXT("success"));
        if (bApiSuccess)
        {
            TSharedPtr<FJsonObject> ResultObject = JsonObject->GetObjectField(TEXT("result"));
            if (ResultObject.IsValid())
            {
                TSharedRef<FVoiceChannelInfoSimplyStream> ChannelInfo = MakeShared<FVoiceChannelInfoSimplyStream>();
                ChannelInfo->ChannelId = ResultObject->GetStringField(TEXT("channelId"));
                ChannelInfo->ProviderChannelId = ResultObject->GetStringField(TEXT("providerChannelId"));
                ChannelInfo->Provider = ResultObject->GetStringField(TEXT("provider"));
                ChannelInfo->ChannelType = ChannelType;
                ChannelInfo->MaxParticipants = ResultObject->GetIntegerField(TEXT("maxParticipants"));
                ChannelInfo->bIsActive = true;

                VoiceChannels.Add(ChannelInfo->ChannelId, ChannelInfo);
            }
        }
    }
}

void FOnlineVoiceImplSimplyStream::ProcessVoiceTokenResponse(const FString& ResponseContent, const FString& ChannelId)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        bool bApiSuccess = JsonObject->GetBoolField(TEXT("success"));
        if (bApiSuccess)
        {
            TSharedPtr<FJsonObject> ResultObject = JsonObject->GetObjectField(TEXT("result"));
            if (ResultObject.IsValid())
            {
                FString Token = ResultObject->GetStringField(TEXT("token"));
                ActiveVoiceTokens.Add(ChannelId, Token);
                
                // TODO: Initialize voice provider SDK with this token
                // For Agora: Initialize RtcEngine with token and channel
                // For Vivox: Connect to voice server with token
            }
        }
    }
}
