// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "OnlineSubsystemSimplyStreamTypes.h"
#include "Http.h"

#ifndef SIMPLYSTREAM_ENABLE_EXTERNAL_UI
#define SIMPLYSTREAM_ENABLE_EXTERNAL_UI 0
#endif

class FOnlineSubsystemSimplyStream;

#if SIMPLYSTREAM_ENABLE_EXTERNAL_UI

/**
 * External UI interface for SimplyStream platform overlays and social features
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlineExternalUISimplyStream : public IOnlineExternalUI, public TSharedFromThis<FOnlineExternalUISimplyStream, ESPMode::ThreadSafe>
{
public:
    FOnlineExternalUISimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
    virtual ~FOnlineExternalUISimplyStream() = default;

    // IOnlineExternalUI interface
    virtual bool ShowLoginUI(int32 LocalUserNum, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate) override;
    virtual bool ShowAccountCreationUI(int32 LocalUserNum, const FOnAccountCreationUIClosedDelegate& Delegate) override;
    virtual bool ShowFriendsUI(int32 LocalUserNum) override;
    virtual bool ShowInviteUI(int32 LocalUserNum, FName SessionName) override;
    virtual bool ShowAchievementsUI(int32 LocalUserNum) override;
    virtual bool ShowLeaderboardUI(int32 LocalUserNum, const FString& LeaderboardName) override;
    virtual bool ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate) override;
    virtual bool CloseWebURL() override;
    virtual bool ShowProfileUI(int32 LocalUserNum, const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate) override;
    virtual bool ShowAccountUpgradeUI(int32 LocalUserNum) override;
    virtual bool ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate) override;
    virtual bool ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams) override;

    // SimplyStream-specific external UI methods
    bool ShowCustomOverlay(const FUniqueNetId& UserId, const FString& UIType, const FString& Action = TEXT("show"));
    bool ConfigureOverlay(const FUniqueNetId& UserId, const FString& UIType, const TMap<FString, FString>& Settings);
    bool UpdateAchievementShowcase(const FUniqueNetId& UserId, const TArray<FString>& AchievementIds);
    bool ShareContent(const FUniqueNetId& UserId, const FString& ContentType, const FString& ContentUrl, const FString& Title = TEXT(""), const TArray<FString>& Platforms = {});

    // Initialize/shutdown
    virtual bool Init();
    virtual void Tick(float DeltaTime);
    virtual void Shutdown();

private:
    /** Reference to the main subsystem */
    FOnlineSubsystemSimplyStream* Subsystem;

    /** Active overlay states */
    TMap<FString, bool> ActiveOverlays; // UIType -> IsVisible

    /** Pending UI delegates */
    TMap<int32, FOnLoginUIClosedDelegate> PendingLoginDelegates;
    TMap<int32, FOnAccountCreationUIClosedDelegate> PendingAccountCreationDelegates;
    TMap<int32, FOnShowWebUrlClosedDelegate> PendingWebUrlDelegates;
    TMap<int32, FOnProfileUIClosedDelegate> PendingProfileDelegates;
    TMap<int32, FOnShowStoreUIClosedDelegate> PendingStoreDelegates;

    /** HTTP completion handlers */
    void OnShowOverlayComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString UIType, FString Action);
    void OnConfigureOverlayComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString UIType);
    void OnShowProfileComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef RequestorId, FUniqueNetIdRef RequesteeId, int32 LocalUserNum, FOnProfileUIClosedDelegate Delegate);
    void OnShareContentComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString ContentType);

    /** Helper methods */
    FString GetAPIBaseURL() const;
    FString GetJWTToken(const FUniqueNetId& UserId) const;
    bool IsOverlayActive(const FString& UIType) const;
    void SetOverlayState(const FString& UIType, bool bIsVisible);
    int32 GetLocalUserNum(const FUniqueNetId& UserId) const;
};

typedef TSharedPtr<FOnlineExternalUISimplyStream, ESPMode::ThreadSafe> FOnlineExternalUISimplyStreamPtr;

#else

/**
 * Stub external UI implementation used while the SimplyStream overlay is under development.
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlineExternalUISimplyStream : public IOnlineExternalUI, public TSharedFromThis<FOnlineExternalUISimplyStream, ESPMode::ThreadSafe>
{
public:
	explicit FOnlineExternalUISimplyStream(FOnlineSubsystemSimplyStream*) {}
	virtual ~FOnlineExternalUISimplyStream() = default;

	virtual bool ShowLoginUI(int32, bool, bool, const FOnLoginUIClosedDelegate&) override { return false; }
	virtual bool ShowAccountCreationUI(int32, const FOnAccountCreationUIClosedDelegate&) override { return false; }
	virtual bool ShowFriendsUI(int32) override { return false; }
	virtual bool ShowInviteUI(int32, FName) override { return false; }
	virtual bool ShowAchievementsUI(int32) override { return false; }
	virtual bool ShowLeaderboardUI(const FString&) override { return false; }
	virtual bool ShowWebURL(const FString&, const FShowWebUrlParams&, const FOnShowWebUrlClosedDelegate&) override { return false; }
	virtual bool CloseWebURL() override { return false; }
	virtual bool ShowProfileUI(const FUniqueNetId&, const FUniqueNetId&, const FOnProfileUIClosedDelegate&) override { return false; }
	virtual bool ShowAccountUpgradeUI(const FUniqueNetId&) override { return false; }
	virtual bool ShowStoreUI(int32, const FShowStoreParams&, const FOnShowStoreUIClosedDelegate&) override { return false; }
	virtual bool ShowSendMessageUI(int32, const FShowSendMessageParams&, const FOnShowSendMessageUIClosedDelegate&) override { return false; }
};

typedef TSharedPtr<FOnlineExternalUISimplyStream, ESPMode::ThreadSafe> FOnlineExternalUISimplyStreamPtr;

#endif // SIMPLYSTREAM_ENABLE_EXTERNAL_UI
