// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSharingInterface.h"
#include "OnlineSubsystemSimplyStreamTypes.h"
#include "Http.h"

class FOnlineSubsystemSimplyStream;

/**
 * Sharing interface for SimplyStream platform social media integration
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlineSharingSimplyStream : public IOnlineSharing, public TSharedFromThis<FOnlineSharingSimplyStream, ESPMode::ThreadSafe>
{
public:
    FOnlineSharingSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
    virtual ~FOnlineSharingSimplyStream() = default;

    // IOnlineSharing interface
    virtual void RequestCurrentPermissions(int32 LocalUserNum, FOnRequestCurrentPermissionsComplete& CompletionDelegate) override;
    virtual void GetCurrentPermissions(int32 LocalUserNum, TArray<FSharingPermission>& OutPermissions) override;
    virtual bool RequestNewReadPermissions(int32 LocalUserNum, EOnlineSharingCategory Category) override;
    virtual bool RequestNewPublishPermissions(int32 LocalUserNum, EOnlineSharingCategory Category, EOnlineStatusUpdatePrivacy Privacy) override;
    EOnlineCachedResult::Type GetCachedNewReadPermissions(int32 LocalUserNum, EOnlineSharingCategory Category, TArray<FSharingPermission>& OutPermissions);
    EOnlineCachedResult::Type GetCachedNewPublishPermissions(int32 LocalUserNum, EOnlineSharingCategory Category, TArray<FSharingPermission>& OutPermissions);
    virtual bool ShareStatusUpdate(int32 LocalUserNum, const FOnlineStatusUpdate& StatusUpdate) override;
    bool ShareStatusUpdate(int32 LocalUserNum, const FOnlineStatusUpdate& StatusUpdate, const FOnSharePostCompleteDelegate& CompletionDelegate);
    virtual bool ReadNewsFeed(int32 LocalUserNum, int32 NumPostsToRead) override;
    bool ReadNewsFeed(int32 LocalUserNum, int32 NumPostsToRead, const FOnReadNewsFeedCompleteDelegate& CompletionDelegate);
    virtual EOnlineCachedResult::Type GetCachedNewsFeed(int32 LocalUserNum, int32 NewsFeedIdx, FOnlineStatusUpdate& OutNewsFeed) override;
    virtual EOnlineCachedResult::Type GetCachedNewsFeeds(int32 LocalUserNum, TArray<FOnlineStatusUpdate>& OutNewsFeeds) override;

    // SimplyStream-specific sharing methods
    bool ShareScreenshot(const FUniqueNetId& UserId, const FString& ScreenshotUrl, const FString& Title = TEXT(""), const TArray<FString>& Platforms = {});
    bool ShareVideo(const FUniqueNetId& UserId, const FString& VideoUrl, const FString& Title = TEXT(""), const FString& ThumbnailUrl = TEXT(""), const TArray<FString>& Platforms = {});
    bool ShareAchievement(const FUniqueNetId& UserId, const FString& AchievementId, const TArray<FString>& Platforms = {});
    bool ShareScore(const FUniqueNetId& UserId, const FString& LeaderboardId, int32 Score, const TArray<FString>& Platforms = {});
    bool ShareMatchResult(const FUniqueNetId& UserId, const FString& MatchId, const TArray<FString>& Platforms = {});
    bool ConfigurePlatformSharing(const FUniqueNetId& UserId, const FString& Platform, const TMap<FString, FString>& Settings);
    bool GetSharedContentFeed(const FUniqueNetId& UserId, const FString& ContentType = TEXT(""), int32 Limit = 20, int32 Offset = 0);

    // Initialize/shutdown
    virtual bool Init();
    virtual void Tick(float DeltaTime);
    virtual void Shutdown();

private:
    /** Reference to the main subsystem */
    FOnlineSubsystemSimplyStream* Subsystem;

    /** Cached permissions */
    TMap<int32, TArray<FSharingPermission>> CachedReadPermissions;
    TMap<int32, TArray<FSharingPermission>> CachedPublishPermissions;

    /** Cached news feeds */
    TMap<int32, TArray<FOnlineStatusUpdate>> CachedNewsFeeds;

    /** Pending delegates */
    TMap<int32, FOnSharePostCompleteDelegate> PendingStatusUpdateDelegates;
    TMap<int32, FOnReadNewsFeedCompleteDelegate> PendingNewsFeedDelegates;

    /** HTTP completion handlers */
    void OnShareContentComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString ContentType, int32 LocalUserNum, FOnSharePostCompleteDelegate Delegate);
    void OnGetNewsFeedComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, int32 LocalUserNum, FOnReadNewsFeedCompleteDelegate Delegate);
    void OnConfigurePlatformComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString Platform);

    /** Helper methods */
    FString GetAPIBaseURL() const;
    FString GetJWTToken(const FUniqueNetId& UserId) const;
    int32 GetLocalUserNum(const FUniqueNetId& UserId) const;
    FUniqueNetIdPtr GetUniquePlayerId(int32 LocalUserNum) const;
    EOnlineSharingCategory GetCategoryFromContentType(const FString& ContentType) const;
    FString GetContentTypeFromCategory(EOnlineSharingCategory Category) const;
    FOnlineStatusUpdate CreateStatusUpdateFromJson(const TSharedPtr<FJsonObject>& JsonObject) const;
};

typedef TSharedPtr<FOnlineSharingSimplyStream, ESPMode::ThreadSafe> FOnlineSharingSimplyStreamPtr;
