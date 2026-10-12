// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineSharingSimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineIdentitySimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FOnlineSharingSimplyStream::FOnlineSharingSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
    : Subsystem(InSubsystem)
{
}

bool FOnlineSharingSimplyStream::Init()
{
    return true;
}

void FOnlineSharingSimplyStream::Tick(float DeltaTime)
{
}

void FOnlineSharingSimplyStream::Shutdown()
{
    CachedReadPermissions.Empty();
    CachedPublishPermissions.Empty();
    CachedNewsFeeds.Empty();
    PendingStatusUpdateDelegates.Empty();
    PendingNewsFeedDelegates.Empty();
}

void FOnlineSharingSimplyStream::RequestCurrentPermissions(int32 LocalUserNum, FOnRequestCurrentPermissionsComplete& CompletionDelegate)
{
    TArray<FSharingPermission> CombinedPermissions;
    if (const TArray<FSharingPermission>* ReadPerms = CachedReadPermissions.Find(LocalUserNum))
    {
        CombinedPermissions.Append(*ReadPerms);
    }
    if (const TArray<FSharingPermission>* PublishPerms = CachedPublishPermissions.Find(LocalUserNum))
    {
        CombinedPermissions.Append(*PublishPerms);
    }

    CompletionDelegate.ExecuteIfBound(LocalUserNum, true, CombinedPermissions);
}

void FOnlineSharingSimplyStream::GetCurrentPermissions(int32 LocalUserNum, TArray<FSharingPermission>& OutPermissions)
{
    OutPermissions.Reset();
    if (const TArray<FSharingPermission>* ReadPerms = CachedReadPermissions.Find(LocalUserNum))
    {
        OutPermissions.Append(*ReadPerms);
    }
    if (const TArray<FSharingPermission>* PublishPerms = CachedPublishPermissions.Find(LocalUserNum))
    {
        OutPermissions.Append(*PublishPerms);
    }
}

bool FOnlineSharingSimplyStream::RequestNewReadPermissions(int32 LocalUserNum, EOnlineSharingCategory Category)
{
    // For SimplyStream, read permissions are automatically granted
    TArray<FSharingPermission> Permissions;
    FSharingPermission GrantedPermission(GetContentTypeFromCategory(Category), Category);
    GrantedPermission.Status = EOnlineSharingPermissionState::Granted;
    Permissions.Add(GrantedPermission);
    
    CachedReadPermissions.Add(LocalUserNum, Permissions);
    
    TriggerOnRequestNewReadPermissionsCompleteDelegates(LocalUserNum, true);
    return true;
}

bool FOnlineSharingSimplyStream::RequestNewPublishPermissions(int32 LocalUserNum, EOnlineSharingCategory Category, EOnlineStatusUpdatePrivacy Privacy)
{
    FUniqueNetIdPtr UserId = GetUniquePlayerId(LocalUserNum);
    if (!UserId.IsValid())
    {
        TriggerOnRequestNewPublishPermissionsCompleteDelegates(LocalUserNum, false);
        return false;
    }

    // Check if user has configured platform sharing
    FString AuthToken = GetJWTToken(*UserId);
    if (AuthToken.IsEmpty())
    {
        TriggerOnRequestNewPublishPermissionsCompleteDelegates(LocalUserNum, false);
        return false;
    }

    // For now, grant publish permissions for all configured platforms
    TArray<FSharingPermission> Permissions;
    FSharingPermission GrantedPermission(GetContentTypeFromCategory(Category), Category);
    GrantedPermission.Status = EOnlineSharingPermissionState::Granted;
    Permissions.Add(GrantedPermission);
    
    CachedPublishPermissions.Add(LocalUserNum, Permissions);
    TriggerOnRequestNewPublishPermissionsCompleteDelegates(LocalUserNum, true);
    
    return true;
}

EOnlineCachedResult::Type FOnlineSharingSimplyStream::GetCachedNewReadPermissions(int32 LocalUserNum, EOnlineSharingCategory Category, TArray<FSharingPermission>& OutPermissions)
{
    const TArray<FSharingPermission>* FoundPermissions = CachedReadPermissions.Find(LocalUserNum);
    if (FoundPermissions)
    {
        OutPermissions = *FoundPermissions;
        return EOnlineCachedResult::Success;
    }
    return EOnlineCachedResult::NotFound;
}

EOnlineCachedResult::Type FOnlineSharingSimplyStream::GetCachedNewPublishPermissions(int32 LocalUserNum, EOnlineSharingCategory Category, TArray<FSharingPermission>& OutPermissions)
{
    const TArray<FSharingPermission>* FoundPermissions = CachedPublishPermissions.Find(LocalUserNum);
    if (FoundPermissions)
    {
        OutPermissions = *FoundPermissions;
        return EOnlineCachedResult::Success;
    }
    return EOnlineCachedResult::NotFound;
}

bool FOnlineSharingSimplyStream::ShareStatusUpdate(int32 LocalUserNum, const FOnlineStatusUpdate& StatusUpdate)
{
    return ShareStatusUpdate(LocalUserNum, StatusUpdate, FOnSharePostCompleteDelegate());
}

bool FOnlineSharingSimplyStream::ReadNewsFeed(int32 LocalUserNum, int32 NumPostsToRead)
{
    return ReadNewsFeed(LocalUserNum, NumPostsToRead, FOnReadNewsFeedCompleteDelegate());
}

bool FOnlineSharingSimplyStream::ShareStatusUpdate(int32 LocalUserNum, const FOnlineStatusUpdate& StatusUpdate, const FOnSharePostCompleteDelegate& CompletionDelegate)
{
    FUniqueNetIdPtr UserId = GetUniquePlayerId(LocalUserNum);
    if (!UserId.IsValid())
    {
        CompletionDelegate.ExecuteIfBound(LocalUserNum, false);
        return false;
    }

    FString AuthToken = GetJWTToken(*UserId);
    if (AuthToken.IsEmpty())
    {
        CompletionDelegate.ExecuteIfBound(LocalUserNum, false);
        return false;
    }

    // Store delegate for callback
    PendingStatusUpdateDelegates.Add(LocalUserNum, CompletionDelegate);

    // Build sharing request based on status update
    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    
    // Determine content payload from status update
    FString ContentType = TEXT("score"); // Default to text-style post
    FString ContentUrl = TEXT("");

    if (StatusUpdate.Image != nullptr)
    {
        UE_LOG_ONLINE_SHARING(Verbose, TEXT("ShareStatusUpdate: Image uploads not yet supported, ignoring provided FImage data"));
    }

    if (!StatusUpdate.Message.IsEmpty())
    {
        ContentUrl = FString::Printf(TEXT("text://%s"), *StatusUpdate.Message);
    }

    RequestJson->SetStringField(TEXT("contentUrl"), ContentUrl);

    RequestJson->SetStringField(TEXT("contentType"), ContentType);
    RequestJson->SetStringField(TEXT("contentTitle"), StatusUpdate.Message);
    RequestJson->SetStringField(TEXT("contentDescription"), StatusUpdate.Message);

    // Set visibility based on privacy setting
    FString Visibility = TEXT("public");
    switch (StatusUpdate.PostPrivacy)
    {
        case EOnlineStatusUpdatePrivacy::OnlyMe:
            Visibility = TEXT("private");
            break;
        case EOnlineStatusUpdatePrivacy::OnlyFriends:
            Visibility = TEXT("friends_only");
            break;
        case EOnlineStatusUpdatePrivacy::Everyone:
        default:
            Visibility = TEXT("public");
            break;
    }
    RequestJson->SetStringField(TEXT("visibility"), Visibility);

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/sharing/create"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSharingSimplyStream::OnShareContentComplete, 
        UserId->AsShared(), ContentType, LocalUserNum, CompletionDelegate);

    return HttpRequest->ProcessRequest();
}

bool FOnlineSharingSimplyStream::ReadNewsFeed(int32 LocalUserNum, int32 NumPostsToRead, const FOnReadNewsFeedCompleteDelegate& CompletionDelegate)
{
    FUniqueNetIdPtr UserId = GetUniquePlayerId(LocalUserNum);
    if (!UserId.IsValid())
    {
        CompletionDelegate.ExecuteIfBound(LocalUserNum, false);
        return false;
    }

    FString AuthToken = GetJWTToken(*UserId);
    if (AuthToken.IsEmpty())
    {
        CompletionDelegate.ExecuteIfBound(LocalUserNum, false);
        return false;
    }

    // Store delegate for callback
    PendingNewsFeedDelegates.Add(LocalUserNum, CompletionDelegate);

    // Build news feed request
    FString RequestUrl = FString::Printf(TEXT("%s/api/v1/multiplayer/player/sharing/list?limit=%d&visibility=friends_only"), 
        *GetAPIBaseURL(), FMath::Clamp(NumPostsToRead, 1, 100));

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(RequestUrl);
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSharingSimplyStream::OnGetNewsFeedComplete, 
        UserId->AsShared(), LocalUserNum, CompletionDelegate);

    return HttpRequest->ProcessRequest();
}

EOnlineCachedResult::Type FOnlineSharingSimplyStream::GetCachedNewsFeed(int32 LocalUserNum, int32 NewsFeedIdx, FOnlineStatusUpdate& OutNewsFeed)
{
    const TArray<FOnlineStatusUpdate>* FoundFeeds = CachedNewsFeeds.Find(LocalUserNum);
    if (FoundFeeds && FoundFeeds->IsValidIndex(NewsFeedIdx))
    {
        OutNewsFeed = (*FoundFeeds)[NewsFeedIdx];
        return EOnlineCachedResult::Success;
    }
    return EOnlineCachedResult::NotFound;
}

EOnlineCachedResult::Type FOnlineSharingSimplyStream::GetCachedNewsFeeds(int32 LocalUserNum, TArray<FOnlineStatusUpdate>& OutNewsFeeds)
{
    const TArray<FOnlineStatusUpdate>* FoundFeeds = CachedNewsFeeds.Find(LocalUserNum);
    if (FoundFeeds)
    {
        OutNewsFeeds = *FoundFeeds;
        return EOnlineCachedResult::Success;
    }
    return EOnlineCachedResult::NotFound;
}

bool FOnlineSharingSimplyStream::ShareScreenshot(const FUniqueNetId& UserId, const FString& ScreenshotUrl, const FString& Title, const TArray<FString>& Platforms)
{
    FString AuthToken = GetJWTToken(UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("contentType"), TEXT("screenshot"));
    RequestJson->SetStringField(TEXT("contentUrl"), ScreenshotUrl);
    if (!Title.IsEmpty())
    {
        RequestJson->SetStringField(TEXT("contentTitle"), Title);
    }

    if (Platforms.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> PlatformArray;
        for (const FString& Platform : Platforms)
        {
            PlatformArray.Add(MakeShared<FJsonValueString>(Platform));
        }
        RequestJson->SetArrayField(TEXT("sharedToPlatforms"), PlatformArray);
    }

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/sharing/create"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    const FUniqueNetIdPtr UserIdCopy = FUniqueNetIdSimplyStream::CreateFrom(UserId);

    HttpRequest->OnProcessRequestComplete().BindLambda([UserIdCopy](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        if (bWasSuccessful && Response.IsValid() && UserIdCopy.IsValid())
        {
            UE_LOG(LogOnlineSharing, Log, TEXT("Screenshot shared for user: %s"), *UserIdCopy->ToDebugString());
        }
    });

    return HttpRequest->ProcessRequest();
}

bool FOnlineSharingSimplyStream::ShareVideo(const FUniqueNetId& UserId, const FString& VideoUrl, const FString& Title, const FString& ThumbnailUrl, const TArray<FString>& Platforms)
{
    FString AuthToken = GetJWTToken(UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("contentType"), TEXT("video"));
    RequestJson->SetStringField(TEXT("contentUrl"), VideoUrl);
    if (!Title.IsEmpty())
    {
        RequestJson->SetStringField(TEXT("contentTitle"), Title);
    }
    if (!ThumbnailUrl.IsEmpty())
    {
        RequestJson->SetStringField(TEXT("thumbnailUrl"), ThumbnailUrl);
    }

    if (Platforms.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> PlatformArray;
        for (const FString& Platform : Platforms)
        {
            PlatformArray.Add(MakeShared<FJsonValueString>(Platform));
        }
        RequestJson->SetArrayField(TEXT("sharedToPlatforms"), PlatformArray);
    }

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/sharing/create"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    const FUniqueNetIdPtr UserIdCopy = FUniqueNetIdSimplyStream::CreateFrom(UserId);

    HttpRequest->OnProcessRequestComplete().BindLambda([UserIdCopy](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        if (bWasSuccessful && Response.IsValid() && UserIdCopy.IsValid())
        {
            UE_LOG(LogOnlineSharing, Log, TEXT("Video shared for user: %s"), *UserIdCopy->ToDebugString());
        }
    });

    return HttpRequest->ProcessRequest();
}

bool FOnlineSharingSimplyStream::ShareAchievement(const FUniqueNetId& UserId, const FString& AchievementId, const TArray<FString>& Platforms)
{
    FString AuthToken = GetJWTToken(UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("contentType"), TEXT("achievement"));
    RequestJson->SetStringField(TEXT("contentUrl"), FString::Printf(TEXT("achievement://%s"), *AchievementId));
    RequestJson->SetStringField(TEXT("contentTitle"), FString::Printf(TEXT("Achievement Unlocked: %s"), *AchievementId));

    // Add achievement metadata
    TSharedPtr<FJsonObject> MetadataJson = MakeShared<FJsonObject>();
    MetadataJson->SetStringField(TEXT("achievementId"), AchievementId);
    RequestJson->SetObjectField(TEXT("metadata"), MetadataJson);

    if (Platforms.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> PlatformArray;
        for (const FString& Platform : Platforms)
        {
            PlatformArray.Add(MakeShared<FJsonValueString>(Platform));
        }
        RequestJson->SetArrayField(TEXT("sharedToPlatforms"), PlatformArray);
    }

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/sharing/create"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    const FUniqueNetIdPtr UserIdCopy = FUniqueNetIdSimplyStream::CreateFrom(UserId);

    HttpRequest->OnProcessRequestComplete().BindLambda([UserIdCopy, AchievementId](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        if (bWasSuccessful && Response.IsValid() && UserIdCopy.IsValid())
        {
            UE_LOG(LogOnlineSharing, Log, TEXT("Achievement %s shared for user: %s"), *AchievementId, *UserIdCopy->ToDebugString());
        }
    });

    return HttpRequest->ProcessRequest();
}

bool FOnlineSharingSimplyStream::ShareScore(const FUniqueNetId& UserId, const FString& LeaderboardId, int32 Score, const TArray<FString>& Platforms)
{
    FString AuthToken = GetJWTToken(UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("contentType"), TEXT("score"));
    RequestJson->SetStringField(TEXT("contentUrl"), FString::Printf(TEXT("score://%s/%d"), *LeaderboardId, Score));
    RequestJson->SetStringField(TEXT("contentTitle"), FString::Printf(TEXT("New High Score: %d"), Score));

    // Add score metadata
    TSharedPtr<FJsonObject> MetadataJson = MakeShared<FJsonObject>();
    MetadataJson->SetStringField(TEXT("leaderboardId"), LeaderboardId);
    MetadataJson->SetNumberField(TEXT("score"), Score);
    RequestJson->SetObjectField(TEXT("metadata"), MetadataJson);

    if (Platforms.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> PlatformArray;
        for (const FString& Platform : Platforms)
        {
            PlatformArray.Add(MakeShared<FJsonValueString>(Platform));
        }
        RequestJson->SetArrayField(TEXT("sharedToPlatforms"), PlatformArray);
    }

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/sharing/create"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    const FUniqueNetIdPtr UserIdCopy = FUniqueNetIdSimplyStream::CreateFrom(UserId);

    HttpRequest->OnProcessRequestComplete().BindLambda([UserIdCopy, Score](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        if (bWasSuccessful && Response.IsValid() && UserIdCopy.IsValid())
        {
            UE_LOG(LogOnlineSharing, Log, TEXT("Score %d shared for user: %s"), Score, *UserIdCopy->ToDebugString());
        }
    });

    return HttpRequest->ProcessRequest();
}

bool FOnlineSharingSimplyStream::ShareMatchResult(const FUniqueNetId& UserId, const FString& MatchId, const TArray<FString>& Platforms)
{
    FString AuthToken = GetJWTToken(UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("contentType"), TEXT("match_result"));
    RequestJson->SetStringField(TEXT("contentUrl"), FString::Printf(TEXT("match://%s"), *MatchId));
    RequestJson->SetStringField(TEXT("contentTitle"), TEXT("Epic Match Result!"));

    // Add match metadata
    TSharedPtr<FJsonObject> MetadataJson = MakeShared<FJsonObject>();
    MetadataJson->SetStringField(TEXT("matchId"), MatchId);
    RequestJson->SetObjectField(TEXT("metadata"), MetadataJson);

    if (Platforms.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> PlatformArray;
        for (const FString& Platform : Platforms)
        {
            PlatformArray.Add(MakeShared<FJsonValueString>(Platform));
        }
        RequestJson->SetArrayField(TEXT("sharedToPlatforms"), PlatformArray);
    }

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/sharing/create"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    const FUniqueNetIdPtr UserIdCopy = FUniqueNetIdSimplyStream::CreateFrom(UserId);

    HttpRequest->OnProcessRequestComplete().BindLambda([UserIdCopy, MatchId](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        if (bWasSuccessful && Response.IsValid() && UserIdCopy.IsValid())
        {
            UE_LOG(LogOnlineSharing, Log, TEXT("Match result %s shared for user: %s"), *MatchId, *UserIdCopy->ToDebugString());
        }
    });

    return HttpRequest->ProcessRequest();
}

bool FOnlineSharingSimplyStream::ConfigurePlatformSharing(const FUniqueNetId& UserId, const FString& Platform, const TMap<FString, FString>& Settings)
{
    FString AuthToken = GetJWTToken(UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    // Build platform configuration request
    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("platform"), Platform.ToLower());

    // Extract settings from map
    for (const auto& Setting : Settings)
    {
        if (Setting.Key == TEXT("accessToken"))
        {
            RequestJson->SetStringField(TEXT("accessToken"), Setting.Value);
        }
        else if (Setting.Key == TEXT("refreshToken"))
        {
            RequestJson->SetStringField(TEXT("refreshToken"), Setting.Value);
        }
        else if (Setting.Key == TEXT("autoShareAchievements"))
        {
            RequestJson->SetBoolField(TEXT("autoShareAchievements"), Setting.Value.ToBool());
        }
        else if (Setting.Key == TEXT("autoShareHighScores"))
        {
            RequestJson->SetBoolField(TEXT("autoShareHighScores"), Setting.Value.ToBool());
        }
        else if (Setting.Key == TEXT("autoShareMatchHighlights"))
        {
            RequestJson->SetBoolField(TEXT("autoShareMatchHighlights"), Setting.Value.ToBool());
        }
        else if (Setting.Key == TEXT("defaultVisibility"))
        {
            RequestJson->SetStringField(TEXT("defaultVisibility"), Setting.Value);
        }
    }

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/sharing/platform-config"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineSharingSimplyStream::OnConfigurePlatformComplete, 
        UserId.AsShared(), Platform);

    return HttpRequest->ProcessRequest();
}

bool FOnlineSharingSimplyStream::GetSharedContentFeed(const FUniqueNetId& UserId, const FString& ContentType, int32 Limit, int32 Offset)
{
    FString AuthToken = GetJWTToken(UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    // Build feed request URL
    FString RequestUrl = FString::Printf(TEXT("%s/api/v1/multiplayer/player/sharing/list?limit=%d&offset=%d"), 
        *GetAPIBaseURL(), FMath::Clamp(Limit, 1, 100), FMath::Max(Offset, 0));

    if (!ContentType.IsEmpty())
    {
        RequestUrl += FString::Printf(TEXT("&contentType=%s"), *ContentType);
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(RequestUrl);
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    const FUniqueNetIdPtr UserIdCopy = FUniqueNetIdSimplyStream::CreateFrom(UserId);

    HttpRequest->OnProcessRequestComplete().BindLambda([UserIdCopy](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        if (bWasSuccessful && Response.IsValid())
        {
            if (UserIdCopy.IsValid())
            {
                UE_LOG(LogOnlineSharing, Log, TEXT("Content feed retrieved for user: %s"), *UserIdCopy->ToDebugString());
            }
        }
    });

    return HttpRequest->ProcessRequest();
}

void FOnlineSharingSimplyStream::OnShareContentComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString ContentType, int32 LocalUserNum, FOnSharePostCompleteDelegate Delegate)
{
    bool bSuccess = bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 201;
    
    if (bSuccess)
    {
        FString ResponseContent = Response->GetContentAsString();
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            bSuccess = JsonObject->GetBoolField(TEXT("success"));
            if (bSuccess)
            {
                UE_LOG(LogOnlineSharing, Log, TEXT("Content (%s) shared successfully for user: %s"), *ContentType, *UserId->ToDebugString());
            }
        }
    }

    Delegate.ExecuteIfBound(LocalUserNum, bSuccess);
    TriggerOnSharePostCompleteDelegates(LocalUserNum, bSuccess);
    PendingStatusUpdateDelegates.Remove(LocalUserNum);
}

void FOnlineSharingSimplyStream::OnGetNewsFeedComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, int32 LocalUserNum, FOnReadNewsFeedCompleteDelegate Delegate)
{
    bool bSuccess = false;
    TArray<FOnlineStatusUpdate> NewsFeeds;

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
                const TArray<TSharedPtr<FJsonValue>>* ResultsArray = nullptr;
                if (JsonObject->TryGetArrayField(TEXT("results"), ResultsArray))
                {
                    for (const auto& ResultValue : *ResultsArray)
                    {
                        const TSharedPtr<FJsonObject>* ResultObject = nullptr;
                        if (ResultValue->TryGetObject(ResultObject))
                        {
                            FOnlineStatusUpdate StatusUpdate = CreateStatusUpdateFromJson(*ResultObject);
                            NewsFeeds.Add(StatusUpdate);
                        }
                    }
                }
            }
        }
    }

    // Cache the news feeds
    if (bSuccess)
    {
        CachedNewsFeeds.Add(LocalUserNum, NewsFeeds);
    }

    Delegate.ExecuteIfBound(LocalUserNum, bSuccess);
    PendingNewsFeedDelegates.Remove(LocalUserNum);
}

void FOnlineSharingSimplyStream::OnConfigurePlatformComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString Platform)
{
    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 201)
    {
        UE_LOG(LogOnlineSharing, Log, TEXT("Platform %s configuration updated for user: %s"), *Platform, *UserId->ToDebugString());
    }
}

FString FOnlineSharingSimplyStream::GetAPIBaseURL() const
{
    return Subsystem->GetAPIBaseURL();
}

FString FOnlineSharingSimplyStream::GetJWTToken(const FUniqueNetId& UserId) const
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (IdentityInt.IsValid())
    {
        return SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, UserId);
    }
    return FString();
}

int32 FOnlineSharingSimplyStream::GetLocalUserNum(const FUniqueNetId& UserId) const
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (IdentityInt.IsValid())
    {
        return SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
    }
    return INDEX_NONE;
}

FUniqueNetIdPtr FOnlineSharingSimplyStream::GetUniquePlayerId(int32 LocalUserNum) const
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (IdentityInt.IsValid())
    {
        return IdentityInt->GetUniquePlayerId(LocalUserNum);
    }
    return nullptr;
}

EOnlineSharingCategory FOnlineSharingSimplyStream::GetCategoryFromContentType(const FString& ContentType) const
{
    if (ContentType.Equals(TEXT("screenshot"), ESearchCase::IgnoreCase) ||
        ContentType.Equals(TEXT("video"), ESearchCase::IgnoreCase) ||
        ContentType.Equals(TEXT("achievement"), ESearchCase::IgnoreCase) ||
        ContentType.Equals(TEXT("score"), ESearchCase::IgnoreCase))
    {
        return EOnlineSharingCategory::SubmitPosts;
    }

    return EOnlineSharingCategory::None;
}

FString FOnlineSharingSimplyStream::GetContentTypeFromCategory(EOnlineSharingCategory Category) const
{
    switch (Category)
    {
        case EOnlineSharingCategory::SubmitPosts:
            return TEXT("screenshot");
        case EOnlineSharingCategory::ReadPosts:
            return TEXT("news");
        default:
            return TEXT("");
    }
}

FOnlineStatusUpdate FOnlineSharingSimplyStream::CreateStatusUpdateFromJson(const TSharedPtr<FJsonObject>& JsonObject) const
{
    FOnlineStatusUpdate StatusUpdate;
    
    if (JsonObject.IsValid())
    {
        if (JsonObject->HasField(TEXT("title")))
        {
            StatusUpdate.Message = JsonObject->GetStringField(TEXT("title"));
        }

        if (JsonObject->HasField(TEXT("contentType")))
        {
            StatusUpdate.Type = JsonObject->GetStringField(TEXT("contentType"));
        }

        if (JsonObject->HasField(TEXT("contentUrl")))
        {
            const FString ContentUrl = JsonObject->GetStringField(TEXT("contentUrl"));
            if (!ContentUrl.IsEmpty())
            {
                if (!StatusUpdate.Message.IsEmpty())
                {
                    StatusUpdate.Message = FString::Printf(TEXT("%s (%s)"), *StatusUpdate.Message, *ContentUrl);
                }
                else
                {
                    StatusUpdate.Message = ContentUrl;
                }
            }
        }

        // Set default privacy (would need to be determined from actual data)
        StatusUpdate.PostPrivacy = EOnlineStatusUpdatePrivacy::Everyone;
    }

    return StatusUpdate;
}
