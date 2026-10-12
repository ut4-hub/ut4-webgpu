// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineAchievementsSimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineIdentitySimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FOnlineAchievementsSimplyStream::FOnlineAchievementsSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
    : Subsystem(InSubsystem)
{
}

void FOnlineAchievementsSimplyStream::WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, PlayerId);

    if (!IdentityInt.IsValid() || LocalUserNum == INDEX_NONE || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("WriteAchievements: User not logged in"));
        Subsystem->ExecuteNextTick([Delegate, PlayerId = PlayerId.AsShared()]()
        {
            Delegate.ExecuteIfBound(*PlayerId, false);
        });
        return;
    }

    const FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("WriteAchievements: No auth token"));
        Subsystem->ExecuteNextTick([Delegate, PlayerId = PlayerId.AsShared()]()
        {
            Delegate.ExecuteIfBound(*PlayerId, false);
        });
        return;
    }

    // Process each achievement update
    bool bAnyAchievementUpdated = false;
    for (FStatPropertyArray::TConstIterator It(WriteObject->Properties); It; ++It)
    {
        const FVariantData& Stat = It.Value();
        const FString AchievementId = It.Key();
        
        if (Stat.GetType() == EOnlineKeyValuePairDataType::Double || 
            Stat.GetType() == EOnlineKeyValuePairDataType::Int32 || 
            Stat.GetType() == EOnlineKeyValuePairDataType::Int64)
        {
            double Progress = 0.0;
            if (Stat.GetType() == EOnlineKeyValuePairDataType::Double)
            {
                Stat.GetValue(Progress);
            }
            else if (Stat.GetType() == EOnlineKeyValuePairDataType::Int32)
            {
                int32 IntValue = 0;
                Stat.GetValue(IntValue);
                Progress = static_cast<double>(IntValue);
            }
            else if (Stat.GetType() == EOnlineKeyValuePairDataType::Int64)
            {
                int64 Int64Value = 0;
                Stat.GetValue(Int64Value);
                Progress = static_cast<double>(Int64Value);
            }

            // Send unlock request for this achievement
            TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
            RequestJson->SetNumberField(TEXT("progress"), Progress);

            FString RequestContent;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
            FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

            TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
            HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/achievements/%s/unlock"), *AchievementId));
            HttpRequest->SetVerb(TEXT("POST"));
            HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
            HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
            HttpRequest->SetContentAsString(RequestContent);

            HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineAchievementsSimplyStream::OnWriteAchievementsComplete, PlayerId.AsShared(), Delegate);
            
            if (HttpRequest->ProcessRequest())
            {
                bAnyAchievementUpdated = true;
                UE_LOG_ONLINE_ACHIEVEMENTS(Verbose, TEXT("WriteAchievements: Sent unlock request for %s"), *AchievementId);
            }
        }
    }

    if (!bAnyAchievementUpdated)
    {
        UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("WriteAchievements: No valid achievements to update"));
        Subsystem->ExecuteNextTick([Delegate, PlayerId = PlayerId.AsShared()]()
        {
            Delegate.ExecuteIfBound(*PlayerId, false);
        });
    }
}

void FOnlineAchievementsSimplyStream::OnWriteAchievementsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef PlayerId, FOnAchievementsWrittenDelegate Delegate)
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
                // Refresh achievements cache
                QueryAchievements(*PlayerId, FOnQueryAchievementsCompleteDelegate());
                UE_LOG_ONLINE_ACHIEVEMENTS(Verbose, TEXT("WriteAchievements: Achievement updated successfully"));
            }
            else
            {
                FString ErrorMsg = JsonObject->GetStringField(TEXT("error"));
                UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("WriteAchievements failed: %s"), *ErrorMsg);
            }
        }
    }
    else
    {
        UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("WriteAchievements: HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }

    Delegate.ExecuteIfBound(*PlayerId, bSuccess);
}

void FOnlineAchievementsSimplyStream::QueryAchievements(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, PlayerId);

    if (!IdentityInt.IsValid() || LocalUserNum == INDEX_NONE || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("QueryAchievements: User not logged in"));
        Subsystem->ExecuteNextTick([Delegate, PlayerId = PlayerId.AsShared()]()
        {
            Delegate.ExecuteIfBound(*PlayerId, false);
        });
        return;
    }

    const FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("QueryAchievements: No auth token"));
        Subsystem->ExecuteNextTick([Delegate, PlayerId = PlayerId.AsShared()]()
        {
            Delegate.ExecuteIfBound(*PlayerId, false);
        });
        return;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/achievements/%s"), *PlayerId.ToString()));
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineAchievementsSimplyStream::OnQueryAchievementsComplete, PlayerId.AsShared(), Delegate);
    
    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("QueryAchievements: Failed to send HTTP request"));
        Subsystem->ExecuteNextTick([Delegate, PlayerId = PlayerId.AsShared()]()
        {
            Delegate.ExecuteIfBound(*PlayerId, false);
        });
    }
}

void FOnlineAchievementsSimplyStream::OnQueryAchievementsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef PlayerId, FOnQueryAchievementsCompleteDelegate Delegate)
{
    bool bSuccess = false;

    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        FString ResponseContent = Response->GetContentAsString();
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
                    TArray<FOnlineAchievement>& UserAchievements = PlayerAchievements.FindOrAdd(PlayerId);
                    UserAchievements.Empty();

                    const TArray<TSharedPtr<FJsonValue>>* AchievementsArray;
                    if (ResultObject->TryGetArrayField(TEXT("achievements"), AchievementsArray))
                    {
                        for (const TSharedPtr<FJsonValue>& AchievementValue : *AchievementsArray)
                        {
                            TSharedPtr<FJsonObject> AchievementObject = AchievementValue->AsObject();
                            if (AchievementObject.IsValid())
                            {
                                FString AchievementId = AchievementObject->GetStringField(TEXT("achievementId"));
                                FString Name = AchievementObject->GetStringField(TEXT("name"));
                                FString Description = AchievementObject->GetStringField(TEXT("description"));
                                bool bIsUnlocked = AchievementObject->GetBoolField(TEXT("isUnlocked"));
                                double Progress = AchievementObject->GetNumberField(TEXT("progress"));
                                double MaxProgress = AchievementObject->GetNumberField(TEXT("maxProgress"));

                                FDateTime UnlockTime = FDateTime::MinValue();
                                FString UnlockedAtStr;
                                if (AchievementObject->TryGetStringField(TEXT("unlockedAt"), UnlockedAtStr) && !UnlockedAtStr.IsEmpty())
                                {
                                    FDateTime::ParseIso8601(*UnlockedAtStr, UnlockTime);
                                }

                                FOnlineAchievement Achievement;
                                Achievement.Id = AchievementId;
                                const double ClampedMax = FMath::Max(MaxProgress, 1.0);
                                Achievement.Progress = FMath::Clamp((Progress / ClampedMax) * 100.0, 0.0, 100.0);
                                
                                UserAchievements.Add(Achievement);
                            }
                        }
                    }

                    bSuccess = true;
                    UE_LOG_ONLINE_ACHIEVEMENTS(Verbose, TEXT("QueryAchievements: Retrieved %d achievements"), UserAchievements.Num());
                }
            }
            else
            {
                FString ErrorMsg = JsonObject->GetStringField(TEXT("error"));
                UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("QueryAchievements failed: %s"), *ErrorMsg);
            }
        }
    }
    else
    {
        UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("QueryAchievements: HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }

    Delegate.ExecuteIfBound(*PlayerId, bSuccess);
}

void FOnlineAchievementsSimplyStream::QueryAchievementDescriptions(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, PlayerId);

    if (!IdentityInt.IsValid() || LocalUserNum == INDEX_NONE || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("QueryAchievementDescriptions: User not logged in"));
        Subsystem->ExecuteNextTick([Delegate, PlayerId = PlayerId.AsShared()]()
        {
            Delegate.ExecuteIfBound(*PlayerId, false);
        });
        return;
    }

    const FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("QueryAchievementDescriptions: No auth token"));
        Subsystem->ExecuteNextTick([Delegate, PlayerId = PlayerId.AsShared()]()
        {
            Delegate.ExecuteIfBound(*PlayerId, false);
        });
        return;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/achievements"));
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineAchievementsSimplyStream::OnQueryAchievementDescriptionsComplete, PlayerId.AsShared(), Delegate);
    
    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("QueryAchievementDescriptions: Failed to send HTTP request"));
        Subsystem->ExecuteNextTick([Delegate, PlayerId = PlayerId.AsShared()]()
        {
            Delegate.ExecuteIfBound(*PlayerId, false);
        });
    }
}

void FOnlineAchievementsSimplyStream::OnQueryAchievementDescriptionsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef PlayerId, FOnQueryAchievementsCompleteDelegate Delegate)
{
    bool bSuccess = false;

    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        FString ResponseContent = Response->GetContentAsString();
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            bool bApiSuccess = JsonObject->GetBoolField(TEXT("success"));
            if (bApiSuccess)
            {
                AchievementDescriptions.Empty();

                const TArray<TSharedPtr<FJsonValue>>* AchievementsArray;
                if (JsonObject->TryGetArrayField(TEXT("results"), AchievementsArray))
                {
                    for (const TSharedPtr<FJsonValue>& AchievementValue : *AchievementsArray)
                    {
                        TSharedPtr<FJsonObject> AchievementObject = AchievementValue->AsObject();
                        if (AchievementObject.IsValid())
                        {
                            FString AchievementId = AchievementObject->GetStringField(TEXT("achievementId"));
                            FString Name = AchievementObject->GetStringField(TEXT("name"));
                            FString Description = AchievementObject->GetStringField(TEXT("description"));
                            bool bIsHidden = AchievementObject->GetBoolField(TEXT("isHidden"));

                            FOnlineAchievementDesc AchievementDesc;
                            AchievementDesc.Title = FText::FromString(Name);
                            AchievementDesc.LockedDesc = FText::FromString(Description);
                            AchievementDesc.UnlockedDesc = FText::FromString(Description);
                            AchievementDesc.bIsHidden = bIsHidden;

                            AchievementDescriptions.Add(AchievementId, AchievementDesc);
                        }
                    }
                }

                bSuccess = true;
                UE_LOG_ONLINE_ACHIEVEMENTS(Verbose, TEXT("QueryAchievementDescriptions: Retrieved %d achievement definitions"), AchievementDescriptions.Num());
            }
            else
            {
                FString ErrorMsg = JsonObject->GetStringField(TEXT("error"));
                UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("QueryAchievementDescriptions failed: %s"), *ErrorMsg);
            }
        }
    }
    else
    {
        UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("QueryAchievementDescriptions: HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }

    Delegate.ExecuteIfBound(*PlayerId, bSuccess);
}

EOnlineCachedResult::Type FOnlineAchievementsSimplyStream::GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement)
{
    const TArray<FOnlineAchievement>* UserAchievements = PlayerAchievements.Find(PlayerId.AsShared());
    if (UserAchievements)
    {
        for (const FOnlineAchievement& Achievement : *UserAchievements)
        {
            if (Achievement.Id == AchievementId)
            {
                OutAchievement = Achievement;
                return EOnlineCachedResult::Success;
            }
        }
        return EOnlineCachedResult::NotFound;
    }
    return EOnlineCachedResult::NotFound;
}

EOnlineCachedResult::Type FOnlineAchievementsSimplyStream::GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement>& OutAchievements)
{
    const TArray<FOnlineAchievement>* UserAchievements = PlayerAchievements.Find(PlayerId.AsShared());
    if (UserAchievements)
    {
        OutAchievements = *UserAchievements;
        return EOnlineCachedResult::Success;
    }
    
    OutAchievements.Empty();
    return EOnlineCachedResult::NotFound;
}

EOnlineCachedResult::Type FOnlineAchievementsSimplyStream::GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc)
{
    const FOnlineAchievementDesc* FoundDesc = AchievementDescriptions.Find(AchievementId);
    if (FoundDesc)
    {
        OutAchievementDesc = *FoundDesc;
        return EOnlineCachedResult::Success;
    }
    return EOnlineCachedResult::NotFound;
}

#if !UE_BUILD_SHIPPING
bool FOnlineAchievementsSimplyStream::ResetAchievements(const FUniqueNetId& PlayerId)
{
    PlayerAchievements.Remove(PlayerId.AsShared());
    UE_LOG_ONLINE_ACHIEVEMENTS(Display, TEXT("ResetAchievements: Cleared cached achievements for user %s"), *PlayerId.ToDebugString());
    return true;
}
#endif
