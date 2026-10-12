// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineStatsSimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineIdentitySimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FOnlineStatsSimplyStream::FOnlineStatsSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
    : Subsystem(InSubsystem)
{
}

void FOnlineStatsSimplyStream::QueryStats(const FUniqueNetIdRef LocalUserId, const FUniqueNetIdRef StatsUser, const FOnlineStatsQueryUserStatsComplete& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, *LocalUserId);
    
    if (!IdentityInt.IsValid() || LocalUserNum == INDEX_NONE || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_STATS(Warning, TEXT("QueryStats: User not logged in"));
        Subsystem->ExecuteNextTick([Delegate, LocalUserId, StatsUser]()
        {
        Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::InvalidUser), TSharedPtr<const FOnlineStatsUserStats>());
        });
        return;
    }

    const FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_STATS(Warning, TEXT("QueryStats: No auth token"));
        Subsystem->ExecuteNextTick([Delegate, LocalUserId, StatsUser]()
        {
        Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::InvalidAuth), TSharedPtr<const FOnlineStatsUserStats>());
        });
        return;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/stats/%s"), *StatsUser->ToString()));
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineStatsSimplyStream::OnQueryStatsComplete, LocalUserId, StatsUser, Delegate);
    
    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG_ONLINE_STATS(Warning, TEXT("QueryStats: Failed to send HTTP request"));
        Subsystem->ExecuteNextTick([Delegate, LocalUserId, StatsUser]()
        {
        Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::RequestFailure), TSharedPtr<const FOnlineStatsUserStats>());
        });
    }
}

void FOnlineStatsSimplyStream::OnQueryStatsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef LocalUserId, FUniqueNetIdRef StatsUser, FOnlineStatsQueryUserStatsComplete Delegate)
{
    FOnlineError Result(EOnlineErrorResult::Unknown);
    TSharedPtr<FOnlineStatsUserStats> UserStats;

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
                    UserStats = MakeShared<FOnlineStatsUserStats>(StatsUser);

                    const TArray<TSharedPtr<FJsonValue>>* StatsArray;
                    if (ResultObject->TryGetArrayField(TEXT("stats"), StatsArray))
                    {
                        for (const TSharedPtr<FJsonValue>& StatValue : *StatsArray)
                        {
                            TSharedPtr<FJsonObject> StatObject = StatValue->AsObject();
                            if (StatObject.IsValid())
                            {
                                FString StatName = StatObject->GetStringField(TEXT("statName"));
                                double StatValueNum = StatObject->GetNumberField(TEXT("statValue"));
                                
                                FVariantData StatVariant;
                                StatVariant.SetValue(StatValueNum);
                                UserStats->Stats.Add(StatName, StatVariant);
                            }
                        }
                    }

                    // Cache the results
                    StatsCache.Add(StatsUser, UserStats.ToSharedRef());
                    Result = FOnlineError::Success();
                }
                else
                {
                    Result.SetFromErrorCode(TEXT("Invalid response format"));
                }
            }
            else
            {
                FString ErrorMsg = JsonObject->GetStringField(TEXT("error"));
                Result.SetFromErrorCode(ErrorMsg);
            }
        }
        else
        {
            Result.SetFromErrorCode(TEXT("Failed to parse response"));
        }
    }
    else
    {
        Result.SetFromErrorCode(FString::Printf(TEXT("HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0));
    }

    UE_LOG_ONLINE_STATS(Verbose, TEXT("QueryStats completed: %s"), Result.WasSuccessful() ? TEXT("Success") : *Result.GetErrorMessage().ToString());
    Delegate.ExecuteIfBound(Result, StaticCastSharedPtr<const FOnlineStatsUserStats>(UserStats));
}

void FOnlineStatsSimplyStream::QueryStats(const FUniqueNetIdRef LocalUserId, const TArray<FUniqueNetIdRef>& StatUsers, const TArray<FString>& StatNames, const FOnlineStatsQueryUsersStatsComplete& Delegate)
{
    // For bulk queries, we'll call the single user version for each user
    // In a production implementation, you might want a dedicated bulk endpoint
    
    TSharedRef<TArray<TSharedRef<const FOnlineStatsUserStats>>> AllUserStats = MakeShared<TArray<TSharedRef<const FOnlineStatsUserStats>>>();
    TSharedRef<int32> CompletedQueries = MakeShared<int32>(0);
    TSharedRef<bool> HasError = MakeShared<bool>(false);

    if (StatUsers.Num() == 0)
    {
        Subsystem->ExecuteNextTick([Delegate, AllUserStats]()
        {
            Delegate.ExecuteIfBound(FOnlineError::Success(), *AllUserStats);
        });
        return;
    }

    for (const FUniqueNetIdRef& StatUser : StatUsers)
    {
        QueryStats(LocalUserId, StatUser, FOnlineStatsQueryUserStatsComplete::CreateLambda(
            [this, StatUsers, AllUserStats, CompletedQueries, HasError, Delegate](const FOnlineError& Error, const TSharedPtr<const FOnlineStatsUserStats>& UserStats)
            {
                if (!Error.WasSuccessful())
                {
                    *HasError = true;
                }

                if (UserStats.IsValid())
                {
                    AllUserStats->Add(UserStats.ToSharedRef());
                }

                (*CompletedQueries)++;

                if (*CompletedQueries >= StatUsers.Num())
                {
                    FOnlineError FinalResult = *HasError ? FOnlineError(EOnlineErrorResult::Unknown) : FOnlineError::Success();
                    Delegate.ExecuteIfBound(FinalResult, *AllUserStats);
                }
            }));
    }
}

TSharedPtr<const FOnlineStatsUserStats> FOnlineStatsSimplyStream::GetStats(const FUniqueNetIdRef StatsUserId) const
{
    const TSharedRef<FOnlineStatsUserStats>* FoundStats = StatsCache.Find(StatsUserId);
    return FoundStats ? TSharedPtr<const FOnlineStatsUserStats>(*FoundStats) : nullptr;
}

void FOnlineStatsSimplyStream::UpdateStats(const FUniqueNetIdRef LocalUserId, const TArray<FOnlineStatsUserUpdatedStats>& UpdatedUserStats, const FOnlineStatsUpdateStatsComplete& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, *LocalUserId);
    
    if (!IdentityInt.IsValid() || LocalUserNum == INDEX_NONE || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_STATS(Warning, TEXT("UpdateStats: User not logged in"));
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::InvalidUser));
        });
        return;
    }

    FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_STATS(Warning, TEXT("UpdateStats: No auth token"));
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::InvalidAuth));
        });
        return;
    }

    if (UpdatedUserStats.Num() == 0)
    {
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(FOnlineError::Success());
        });
        return;
    }

    // For now, only support updating local user's stats
    const FOnlineStatsUserUpdatedStats& UserStatsUpdate = UpdatedUserStats[0];
    if (*UserStatsUpdate.Account != *LocalUserId)
    {
        UE_LOG_ONLINE_STATS(Warning, TEXT("UpdateStats: Can only update own stats"));
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::InvalidParams));
        });
        return;
    }

    // Build request JSON
    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> StatsArray;

    for (const TPair<FString, FOnlineStatUpdate>& StatPair : UserStatsUpdate.Stats)
    {
        TSharedPtr<FJsonObject> StatObject = MakeShared<FJsonObject>();
        StatObject->SetStringField(TEXT("statName"), StatPair.Key);
        
        FString Operation = TEXT("set");
        double Value = 0.0;
        
        const FOnlineStatValue& RawValue = StatPair.Value.GetValue();
        const EOnlineKeyValuePairDataType::Type RawType = RawValue.GetType();

        if (RawType == EOnlineKeyValuePairDataType::Double)
        {
            RawValue.GetValue(Value);
        }
        else if (RawType == EOnlineKeyValuePairDataType::Int32)
        {
            int32 IntValue = 0;
            RawValue.GetValue(IntValue);
            Value = static_cast<double>(IntValue);
        }
        else if (RawType == EOnlineKeyValuePairDataType::Int64)
        {
            int64 Int64Value = 0;
            RawValue.GetValue(Int64Value);
            Value = static_cast<double>(Int64Value);
        }

        // Determine operation type using UE's stat modification enum
        switch (StatPair.Value.GetModificationType())
        {
            case FOnlineStatUpdate::EOnlineStatModificationType::Sum:
                Operation = TEXT("increment");
                break;
            case FOnlineStatUpdate::EOnlineStatModificationType::Largest:
            case FOnlineStatUpdate::EOnlineStatModificationType::Smallest:
            case FOnlineStatUpdate::EOnlineStatModificationType::Unknown:
            case FOnlineStatUpdate::EOnlineStatModificationType::Set:
            default:
                Operation = TEXT("set");
                break;
        }

        StatObject->SetStringField(TEXT("operation"), Operation);
        StatObject->SetNumberField(TEXT("statValue"), Value);
        
        StatsArray.Add(MakeShared<FJsonValueObject>(StatObject));
    }

    RequestJson->SetArrayField(TEXT("stats"), StatsArray);

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/stats"));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineStatsSimplyStream::OnUpdateStatsComplete, LocalUserId, UpdatedUserStats, Delegate);
    
    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG_ONLINE_STATS(Warning, TEXT("UpdateStats: Failed to send HTTP request"));
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::RequestFailure));
        });
    }
}

void FOnlineStatsSimplyStream::OnUpdateStatsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef LocalUserId, TArray<FOnlineStatsUserUpdatedStats> UpdatedUserStats, FOnlineStatsUpdateStatsComplete Delegate)
{
    FOnlineError Result(EOnlineErrorResult::Unknown);

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
                Result = FOnlineError::Success();
                
                // Update cache with new values
                TSharedRef<FOnlineStatsUserStats>* CachedStats = StatsCache.Find(LocalUserId);
                if (!CachedStats)
                {
                    CachedStats = &StatsCache.Add(LocalUserId, MakeShared<FOnlineStatsUserStats>(LocalUserId));
                }

                TSharedPtr<FJsonObject> ResultObject = JsonObject->GetObjectField(TEXT("result"));
                if (ResultObject.IsValid())
                {
                    const TArray<TSharedPtr<FJsonValue>>* UpdatedStatsArray;
                    if (ResultObject->TryGetArrayField(TEXT("updatedStats"), UpdatedStatsArray))
                    {
                        for (const TSharedPtr<FJsonValue>& StatValue : *UpdatedStatsArray)
                        {
                            TSharedPtr<FJsonObject> StatObject = StatValue->AsObject();
                            if (StatObject.IsValid())
                            {
                                FString StatName = StatObject->GetStringField(TEXT("statName"));
                                double NewValue = StatObject->GetNumberField(TEXT("newValue"));
                                
                                FVariantData StatVariant;
                                StatVariant.SetValue(NewValue);
                                (*CachedStats)->Stats.Add(StatName, StatVariant);
                            }
                        }
                    }
                }
                
                UE_LOG_ONLINE_STATS(Verbose, TEXT("UpdateStats: Successfully updated stats"));
            }
            else
            {
                FString ErrorMsg = JsonObject->GetStringField(TEXT("error"));
                Result.SetFromErrorCode(ErrorMsg);
            }
        }
        else
        {
            Result.SetFromErrorCode(TEXT("Failed to parse response"));
        }
    }
    else
    {
        Result.SetFromErrorCode(FString::Printf(TEXT("HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0));
    }

    Delegate.ExecuteIfBound(Result);
}

#if !UE_BUILD_SHIPPING
void FOnlineStatsSimplyStream::ResetStats(const FUniqueNetIdRef StatsUserId)
{
    // Remove from cache
    StatsCache.Remove(StatsUserId);
    UE_LOG_ONLINE_STATS(Display, TEXT("ResetStats: Cleared cached stats for user %s"), *StatsUserId->ToDebugString());
}
#endif
