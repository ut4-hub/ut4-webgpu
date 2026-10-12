// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineExternalUISimplyStream.h"

#if SIMPLYSTREAM_ENABLE_EXTERNAL_UI

#include "OnlineSubsystemSimplyStream.h"
#include "OnlineIdentitySimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FOnlineExternalUISimplyStream::FOnlineExternalUISimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
    : Subsystem(InSubsystem)
{
}

bool FOnlineExternalUISimplyStream::Init()
{
    return true;
}

void FOnlineExternalUISimplyStream::Tick(float DeltaTime)
{
}

void FOnlineExternalUISimplyStream::Shutdown()
{
    ActiveOverlays.Empty();
    PendingLoginDelegates.Empty();
    PendingAccountCreationDelegates.Empty();
    PendingWebUrlDelegates.Empty();
    PendingProfileDelegates.Empty();
    PendingStoreDelegates.Empty();
}

bool FOnlineExternalUISimplyStream::ShowLoginUI(int32 LocalUserNum, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    FUniqueNetIdPtr UserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
    if (!UserId.IsValid())
    {
        return false;
    }

    // Store delegate for callback
    PendingLoginDelegates.Add(LocalUserNum, Delegate);

    // Show login overlay
    return ShowCustomOverlay(*UserId, TEXT("login"), TEXT("show"));
}

bool FOnlineExternalUISimplyStream::ShowAccountCreationUI(int32 LocalUserNum, const FOnAccountCreationUIClosedDelegate& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    FUniqueNetIdPtr UserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
    if (!UserId.IsValid())
    {
        return false;
    }

    // Store delegate for callback
    PendingAccountCreationDelegates.Add(LocalUserNum, Delegate);

    // Show account creation overlay
    return ShowCustomOverlay(*UserId, TEXT("account_creation"), TEXT("show"));
}

bool FOnlineExternalUISimplyStream::ShowFriendsUI(int32 LocalUserNum)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    FUniqueNetIdPtr UserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
    if (!UserId.IsValid())
    {
        return false;
    }

    return ShowCustomOverlay(*UserId, TEXT("friends"), TEXT("show"));
}

bool FOnlineExternalUISimplyStream::ShowInviteUI(int32 LocalUserNum, FName SessionName)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    FUniqueNetIdPtr UserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
    if (!UserId.IsValid())
    {
        return false;
    }

    return ShowCustomOverlay(*UserId, TEXT("invite"), TEXT("show"));
}

bool FOnlineExternalUISimplyStream::ShowAchievementsUI(int32 LocalUserNum)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    FUniqueNetIdPtr UserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
    if (!UserId.IsValid())
    {
        return false;
    }

    return ShowCustomOverlay(*UserId, TEXT("achievements"), TEXT("show"));
}

bool FOnlineExternalUISimplyStream::ShowLeaderboardUI(int32 LocalUserNum, const FString& LeaderboardName)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    FUniqueNetIdPtr UserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
    if (!UserId.IsValid())
    {
        return false;
    }

    return ShowCustomOverlay(*UserId, TEXT("leaderboards"), TEXT("show"));
}

bool FOnlineExternalUISimplyStream::ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate)
{
    // Store delegate for callback
    int32 RequestId = FMath::Rand();
    PendingWebUrlDelegates.Add(RequestId, Delegate);

    // In a real implementation, this would open a web browser overlay
    // For now, simulate immediate completion
    FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
    
    // Trigger completion delegate after a short delay
    FTimerHandle TimerHandle;
    FTimerDelegate TimerDelegate = FTimerDelegate::CreateLambda([this, RequestId]()
    {
        if (FOnShowWebUrlClosedDelegate* FoundDelegate = PendingWebUrlDelegates.Find(RequestId))
        {
            FoundDelegate->ExecuteIfBound(FString(TEXT("")));
            PendingWebUrlDelegates.Remove(RequestId);
        }
    });
    
    // Simulate 1 second delay
    FTimerManager& TimerManager = GEngine->GetEngineSubsystem<UWorld>()->GetTimerManager();
    TimerManager.SetTimer(TimerHandle, TimerDelegate, 1.0f, false);

    return true;
}

bool FOnlineExternalUISimplyStream::CloseWebURL()
{
    // In a real implementation, this would close the web overlay
    return true;
}

bool FOnlineExternalUISimplyStream::ShowProfileUI(int32 LocalUserNum, const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate)
{
    FString AuthToken = GetJWTToken(Requestor);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    // Store delegate for callback
    PendingProfileDelegates.Add(LocalUserNum, Delegate);

    // Build profile view request
    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("targetUserId"), Requestee.ToString());
    RequestJson->SetStringField(TEXT("viewType"), TEXT("full_profile"));
    RequestJson->SetStringField(TEXT("platform"), TEXT("game"));

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/external-ui/profile"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineExternalUISimplyStream::OnShowProfileComplete, 
        Requestor.AsShared(), Requestee.AsShared(), LocalUserNum, Delegate);

    return HttpRequest->ProcessRequest();
}

bool FOnlineExternalUISimplyStream::ShowAccountUpgradeUI(int32 LocalUserNum)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    FUniqueNetIdPtr UserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
    if (!UserId.IsValid())
    {
        return false;
    }

    return ShowCustomOverlay(*UserId, TEXT("account_upgrade"), TEXT("show"));
}

bool FOnlineExternalUISimplyStream::ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    FUniqueNetIdPtr UserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
    if (!UserId.IsValid())
    {
        return false;
    }

    // Store delegate for callback
    PendingStoreDelegates.Add(LocalUserNum, Delegate);

    return ShowCustomOverlay(*UserId, TEXT("store"), TEXT("show"));
}

bool FOnlineExternalUISimplyStream::ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }

    FUniqueNetIdPtr UserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
    if (!UserId.IsValid())
    {
        return false;
    }

    return ShowCustomOverlay(*UserId, TEXT("send_message"), TEXT("show"));
}

bool FOnlineExternalUISimplyStream::ShowCustomOverlay(const FUniqueNetId& UserId, const FString& UIType, const FString& Action)
{
    FString AuthToken = GetJWTToken(UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    // Build overlay request
    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("uiType"), UIType);
    RequestJson->SetStringField(TEXT("action"), Action);

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/external-ui/overlay"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineExternalUISimplyStream::OnShowOverlayComplete, 
        UserId.AsShared(), UIType, Action);

    return HttpRequest->ProcessRequest();
}

bool FOnlineExternalUISimplyStream::ConfigureOverlay(const FUniqueNetId& UserId, const FString& UIType, const TMap<FString, FString>& Settings)
{
    FString AuthToken = GetJWTToken(UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    // Build configuration request
    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("uiType"), UIType);

    // Add settings from map
    for (const auto& Setting : Settings)
    {
        if (Setting.Key == TEXT("overlayEnabled"))
        {
            RequestJson->SetBoolField(TEXT("overlayEnabled"), Setting.Value.ToBool());
        }
        else if (Setting.Key == TEXT("position"))
        {
            RequestJson->SetStringField(TEXT("position"), Setting.Value);
        }
        else if (Setting.Key == TEXT("theme"))
        {
            RequestJson->SetStringField(TEXT("theme"), Setting.Value);
        }
        else if (Setting.Key == TEXT("opacity"))
        {
            RequestJson->SetNumberField(TEXT("opacity"), FCString::Atof(*Setting.Value));
        }
        else if (Setting.Key == TEXT("hotkeyBinding"))
        {
            RequestJson->SetStringField(TEXT("hotkeyBinding"), Setting.Value);
        }
        else if (Setting.Key == TEXT("autoHideDelay"))
        {
            RequestJson->SetNumberField(TEXT("autoHideDelay"), FCString::Atoi(*Setting.Value));
        }
        else if (Setting.Key == TEXT("languageCode"))
        {
            RequestJson->SetStringField(TEXT("languageCode"), Setting.Value);
        }
    }

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/external-ui/config"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineExternalUISimplyStream::OnConfigureOverlayComplete, 
        UserId.AsShared(), UIType);

    return HttpRequest->ProcessRequest();
}

bool FOnlineExternalUISimplyStream::UpdateAchievementShowcase(const FUniqueNetId& UserId, const TArray<FString>& AchievementIds)
{
    FString AuthToken = GetJWTToken(UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    // Build showcase request
    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> AchievementArray;

    for (int32 i = 0; i < AchievementIds.Num(); i++)
    {
        TSharedPtr<FJsonObject> AchievementObj = MakeShared<FJsonObject>();
        AchievementObj->SetStringField(TEXT("achievementId"), AchievementIds[i]);
        AchievementObj->SetNumberField(TEXT("displayOrder"), i);
        AchievementObj->SetBoolField(TEXT("isFeatured"), i < 3); // First 3 are featured

        AchievementArray.Add(MakeShared<FJsonValueObject>(AchievementObj));
    }

    RequestJson->SetArrayField(TEXT("achievements"), AchievementArray);

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/external-ui/showcase"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindLambda([UserId](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        if (bWasSuccessful && Response.IsValid())
        {
            UE_LOG(LogOnlineExternalUI, Log, TEXT("Achievement showcase updated for user: %s"), *UserId->ToDebugString());
        }
    });

    return HttpRequest->ProcessRequest();
}

bool FOnlineExternalUISimplyStream::ShareContent(const FUniqueNetId& UserId, const FString& ContentType, const FString& ContentUrl, const FString& Title, const TArray<FString>& Platforms)
{
    FString AuthToken = GetJWTToken(UserId);
    if (AuthToken.IsEmpty())
    {
        return false;
    }

    // Build sharing request
    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("contentType"), ContentType);
    RequestJson->SetStringField(TEXT("contentUrl"), ContentUrl);
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
    HttpRequest->SetURL(GetAPIBaseURL() + TEXT("/api/v1/multiplayer/sharing/create"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineExternalUISimplyStream::OnShareContentComplete, 
        UserId.AsShared(), ContentType);

    return HttpRequest->ProcessRequest();
}

void FOnlineExternalUISimplyStream::OnShowOverlayComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString UIType, FString Action)
{
    bool bSuccess = false;
    
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
                TSharedPtr<FJsonObject> ResultObject = JsonObject->GetObjectField(TEXT("result"));
                if (ResultObject.IsValid())
                {
                    FString NewState = ResultObject->GetStringField(TEXT("newState"));
                    SetOverlayState(UIType, NewState == TEXT("visible"));
                    UE_LOG(LogOnlineExternalUI, Log, TEXT("Overlay %s %s for user: %s"), *UIType, *Action, *UserId->ToDebugString());
                }
            }
        }
    }

    // Handle UI-specific completion logic
    if (UIType == TEXT("login"))
    {
        int32 LocalUserNum = GetLocalUserNum(*UserId);
        if (FOnLoginUIClosedDelegate* FoundDelegate = PendingLoginDelegates.Find(LocalUserNum))
        {
            FoundDelegate->ExecuteIfBound(UserId, LocalUserNum, bSuccess ? EOnlineExternalUIResult::Accepted : EOnlineExternalUIResult::Canceled);
            PendingLoginDelegates.Remove(LocalUserNum);
        }
    }
    else if (UIType == TEXT("account_creation"))
    {
        int32 LocalUserNum = GetLocalUserNum(*UserId);
        if (FOnAccountCreationUIClosedDelegate* FoundDelegate = PendingAccountCreationDelegates.Find(LocalUserNum))
        {
            FoundDelegate->ExecuteIfBound(bSuccess ? EOnlineExternalUIResult::Accepted : EOnlineExternalUIResult::Canceled);
            PendingAccountCreationDelegates.Remove(LocalUserNum);
        }
    }
    else if (UIType == TEXT("store"))
    {
        int32 LocalUserNum = GetLocalUserNum(*UserId);
        if (FOnShowStoreUIClosedDelegate* FoundDelegate = PendingStoreDelegates.Find(LocalUserNum))
        {
            FoundDelegate->ExecuteIfBound(bSuccess ? EOnlineExternalUIResult::Accepted : EOnlineExternalUIResult::Canceled);
            PendingStoreDelegates.Remove(LocalUserNum);
        }
    }
}

void FOnlineExternalUISimplyStream::OnConfigureOverlayComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString UIType)
{
    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 201)
    {
        UE_LOG(LogOnlineExternalUI, Log, TEXT("Overlay configuration updated for %s: %s"), *UIType, *UserId->ToDebugString());
    }
}

void FOnlineExternalUISimplyStream::OnShowProfileComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef RequestorId, FUniqueNetIdRef RequesteeId, int32 LocalUserNum, FOnProfileUIClosedDelegate Delegate)
{
    bool bSuccess = bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200;
    
    if (bSuccess)
    {
        UE_LOG(LogOnlineExternalUI, Log, TEXT("Profile UI shown for %s viewing %s"), *RequestorId->ToDebugString(), *RequesteeId->ToDebugString());
    }

    Delegate.ExecuteIfBound(bSuccess ? EOnlineExternalUIResult::Accepted : EOnlineExternalUIResult::Canceled);
    PendingProfileDelegates.Remove(LocalUserNum);
}

void FOnlineExternalUISimplyStream::OnShareContentComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString ContentType)
{
    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 201)
    {
        FString ResponseContent = Response->GetContentAsString();
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            bool bSuccess = JsonObject->GetBoolField(TEXT("success"));
            if (bSuccess)
            {
                TSharedPtr<FJsonObject> ResultObject = JsonObject->GetObjectField(TEXT("result"));
                if (ResultObject.IsValid())
                {
                    FString ShareId = ResultObject->GetStringField(TEXT("shareId"));
                    FString ShareUrl = ResultObject->GetStringField(TEXT("shareUrl"));
                    UE_LOG(LogOnlineExternalUI, Log, TEXT("Content shared successfully: %s (%s)"), *ShareUrl, *ShareId);
                }
            }
        }
    }
}

FString FOnlineExternalUISimplyStream::GetAPIBaseURL() const
{
    return Subsystem->GetAPIBaseURL();
}

FString FOnlineExternalUISimplyStream::GetJWTToken(const FUniqueNetId& UserId) const
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (IdentityInt.IsValid())
    {
        int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
        return IdentityInt->GetAuthToken(LocalUserNum);
    }
    return FString();
}

bool FOnlineExternalUISimplyStream::IsOverlayActive(const FString& UIType) const
{
    const bool* FoundState = ActiveOverlays.Find(UIType);
    return FoundState ? *FoundState : false;
}

void FOnlineExternalUISimplyStream::SetOverlayState(const FString& UIType, bool bIsVisible)
{
    ActiveOverlays.Add(UIType, bIsVisible);
}

int32 FOnlineExternalUISimplyStream::GetLocalUserNum(const FUniqueNetId& UserId) const
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (IdentityInt.IsValid())
    {
        return SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
    }
    return INDEX_NONE;
}

#endif // SIMPLYSTREAM_ENABLE_EXTERNAL_UI
