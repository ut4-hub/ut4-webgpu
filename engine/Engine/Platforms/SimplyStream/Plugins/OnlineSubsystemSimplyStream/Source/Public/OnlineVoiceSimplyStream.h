// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/VoiceInterface.h"
#include "OnlineSubsystemSimplyStreamTypes.h"

class FOnlineSubsystemSimplyStream;

/**
 * Voice channel info for SimplyStream platform
 */
class FVoiceChannelInfoSimplyStream
{
public:
    FString ChannelId;
    FString ProviderChannelId;
    FString Provider;
    FString ChannelType;
    int32 MaxParticipants;
    int32 CurrentParticipants;
    bool bIsActive;
    
    FVoiceChannelInfoSimplyStream()
        : MaxParticipants(20)
        , CurrentParticipants(0)
        , bIsActive(false)
    {}
};

/**
 * Implementation of IOnlineVoice for SimplyStream platform with HTTP API integration
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlineVoiceImplSimplyStream : public IOnlineVoice, public TSharedFromThis<FOnlineVoiceImplSimplyStream, ESPMode::ThreadSafe>
{
public:
    FOnlineVoiceImplSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
    virtual ~FOnlineVoiceImplSimplyStream() = default;

    // IOnlineVoice interface
    virtual bool Init() override;
    virtual void StartNetworkedVoice(uint8 LocalUserNum) override;
    virtual void StopNetworkedVoice(uint8 LocalUserNum) override;
    virtual bool RegisterLocalTalker(uint32 LocalUserNum) override;
    virtual void RegisterLocalTalkers() override;
    virtual bool UnregisterLocalTalker(uint32 LocalUserNum) override;
    virtual void UnregisterLocalTalkers() override;
    virtual bool RegisterRemoteTalker(const FUniqueNetId& UniqueId) override;
    virtual bool UnregisterRemoteTalker(const FUniqueNetId& UniqueId) override;
    virtual void RemoveAllRemoteTalkers() override;
    virtual bool IsHeadsetPresent(uint32 LocalUserNum) override;
    virtual bool IsLocalPlayerTalking(uint32 LocalUserNum) override;
    virtual bool IsRemotePlayerTalking(const FUniqueNetId& UniqueId) override;
    virtual bool IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const override;
    virtual bool MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
    virtual bool UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
    virtual TSharedPtr<FVoicePacket> GetLocalPacket(uint32 LocalUserNum) override;
    virtual TSharedPtr<FVoicePacket> SerializeRemotePacket(FArchive& Ar) override;
    virtual int32 GetNumLocalTalkers() override;
    virtual void ClearVoicePackets() override;
    virtual void Tick(float DeltaTime) override;
    virtual FString GetVoiceDebugState() const override;
    void ClearAllMutes();
    FString GetVoiceDataObjectId(const FUniqueNetId& InUniqueNetId) const;
    virtual IVoiceEnginePtr CreateVoiceEngine() override;
    virtual void ProcessMuteChangeNotification() override;

    // SimplyStream-specific voice methods
    bool CreateVoiceChannel(const FUniqueNetId& UserId, const FString& ChannelType, const FString& SessionId = TEXT(""), const FString& MatchId = TEXT(""));
    bool JoinVoiceChannel(const FUniqueNetId& UserId, const FString& ChannelId);
    bool LeaveVoiceChannel(const FUniqueNetId& UserId, const FString& ChannelId);
    bool GetVoiceToken(const FUniqueNetId& UserId, const FString& ChannelId);

private:
    /** Reference to the main subsystem */
    FOnlineSubsystemSimplyStream* Subsystem;

    /** Map of registered remote players */
    TMap<FUniqueNetIdRef, FRemoteTalker> RemoteTalkers;

    /** Voice channel cache */
    TMap<FString, TSharedRef<FVoiceChannelInfoSimplyStream>> VoiceChannels;

    /** Voice engine state */
    bool bIsLocalPlayerTalking;
    bool bIsVoiceNetworked;

    /** Track muted talkers by ID string */
    TSet<FString> MutedTalkerIds;

    /** Active voice tokens */
    TMap<FString, FString> ActiveVoiceTokens; // ChannelId -> Token

    /** HTTP completion handlers */
    void OnCreateChannelComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString ChannelType);
    void OnJoinChannelComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString ChannelId);
    void OnLeaveChannelComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString ChannelId);
    void OnVoiceTokenComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString ChannelId);

    /** Helper methods */
    void ProcessVoiceChannelResponse(const FString& ResponseContent, const FString& ChannelType);
    void ProcessVoiceTokenResponse(const FString& ResponseContent, const FString& ChannelId);
};

typedef TSharedPtr<FOnlineVoiceImplSimplyStream, ESPMode::ThreadSafe> FOnlineVoiceImplSimplyStreamPtr;
