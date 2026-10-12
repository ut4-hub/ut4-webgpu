// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineLeaderboardsSimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineIdentitySimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/VarArgs.h"

#include <cstdarg>

FOnlineLeaderboardsSimplyStream::FOnlineLeaderboardsSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
    : Subsystem(InSubsystem)
{
}

bool FOnlineLeaderboardsSimplyStream::ReadLeaderboards(const TArray<FUniqueNetIdRef>& Players, FOnlineLeaderboardReadRef& ReadObject)
{
    if (ReadObject->LeaderboardName.IsEmpty())
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboards: Invalid leaderboard name"));
        return false;
    }

    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboards: No identity interface"));
        return false;
    }

    int32 LocalUserNum = 0;
    if (Players.Num() > 0)
    {
        LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, *Players[0]);
    }

    if (IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboards: User not logged in"));
        return false;
    }

    FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboards: No auth token"));
        return false;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    
    // Build URL with pagination parameters
    const FString LeaderboardId = ReadObject->LeaderboardName;
    FString LeaderboardURL = Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/leaderboards/%s"), *LeaderboardId);
    
    TArray<FString> LeaderboardQueryParams;
    if (!ReadObject->SortedColumn.IsEmpty())
    {
        LeaderboardQueryParams.Add(FString::Printf(TEXT("sortBy=%s"), *ReadObject->SortedColumn));
    }
    
    if (LeaderboardQueryParams.Num() > 0)
    {
        LeaderboardURL += TEXT("?") + FString::Join(LeaderboardQueryParams, TEXT("&"));
    }

    HttpRequest->SetURL(LeaderboardURL);
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(AsShared(), &FOnlineLeaderboardsSimplyStream::OnReadLeaderboardsComplete, ReadObject);
    
    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboards: Failed to send HTTP request"));
        return false;
    }

    return true;
}

void FOnlineLeaderboardsSimplyStream::OnReadLeaderboardsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FOnlineLeaderboardReadRef ReadObject)
{
    ReadObject->ReadState = EOnlineAsyncTaskState::Failed;

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
                    ReadObject->Rows.Empty();

                    const TArray<TSharedPtr<FJsonValue>>* EntriesArray;
                    if (ResultObject->TryGetArrayField(TEXT("entries"), EntriesArray))
                    {
                        for (const TSharedPtr<FJsonValue>& EntryValue : *EntriesArray)
                        {
                            TSharedPtr<FJsonObject> EntryObject = EntryValue->AsObject();
                            if (EntryObject.IsValid())
                            {
                                FString UserId = EntryObject->GetStringField(TEXT("userId"));
                                double Score = EntryObject->GetNumberField(TEXT("score"));
                                int32 Rank = EntryObject->GetIntegerField(TEXT("rank"));

                                FUniqueNetIdSimplyStreamRef UserNetId = FUniqueNetIdSimplyStream::CreateRef(UserId);
                                
                                FOnlineStatsRow NewRow(FString(ReadObject->LeaderboardName), UserNetId);
                                NewRow.Rank = Rank;
                                
                                // Add score column
                                FVariantData ScoreVariant;
                                ScoreVariant.SetValue(Score);
                                static const FString ScoreColumnName(TEXT("Score"));
                                NewRow.Columns.Add(ScoreColumnName, ScoreVariant);

                                // Add any metadata columns
                                TSharedPtr<FJsonObject> MetadataObject = EntryObject->GetObjectField(TEXT("metadata"));
                                if (MetadataObject.IsValid())
                                {
                                    for (const auto& MetadataPair : MetadataObject->Values)
                                    {
                                        FVariantData MetadataVariant;
                                        
                                        if (MetadataPair.Value->Type == EJson::String)
                                        {
                                            MetadataVariant.SetValue(MetadataPair.Value->AsString());
                                        }
                                        else if (MetadataPair.Value->Type == EJson::Number)
                                        {
                                            MetadataVariant.SetValue(MetadataPair.Value->AsNumber());
                                        }
                                        else if (MetadataPair.Value->Type == EJson::Boolean)
                                        {
                                            MetadataVariant.SetValue(MetadataPair.Value->AsBool());
                                        }
                                        
                                        NewRow.Columns.Add(FString(MetadataPair.Key), MetadataVariant);
                                    }
                                }

                                ReadObject->Rows.Add(MoveTemp(NewRow));
                            }
                        }
                    }

                    ReadObject->ReadState = EOnlineAsyncTaskState::Done;
                    
                    // Cache the results
                    LeaderboardCache.Add(FString(ReadObject->LeaderboardName), ReadObject);
                    
                    UE_LOG_ONLINE_LEADERBOARD(Verbose, TEXT("ReadLeaderboards: Retrieved %d entries"), ReadObject->Rows.Num());
                }
            }
            else
            {
                FString ErrorMsg = JsonObject->GetStringField(TEXT("error"));
                UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboards failed: %s"), *ErrorMsg);
            }
        }
    }
    else
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboards: HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }

    // Trigger delegate
    TriggerOnLeaderboardReadCompleteDelegates(ReadObject->ReadState == EOnlineAsyncTaskState::Done);
}

bool FOnlineLeaderboardsSimplyStream::ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject)
{
    // Friends leaderboards not implemented yet - would need friends interface
    UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboardsForFriends: Not implemented"));
    return false;
}

bool FOnlineLeaderboardsSimplyStream::ReadLeaderboardsAroundRank(int32 Rank, uint32 Range, FOnlineLeaderboardReadRef& ReadObject)
{
    if (ReadObject->LeaderboardName.IsEmpty())
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboardsAroundRank: Invalid leaderboard name"));
        return false;
    }

    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid() || IdentityInt->GetLoginStatus(0) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboardsAroundRank: User not logged in"));
        return false;
    }

    FString AuthToken = IdentityInt->GetAuthToken(0);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboardsAroundRank: No auth token"));
        return false;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    
    const FString LeaderboardId = FString(ReadObject->LeaderboardName);
    FString RankURL = Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/leaderboards/%s"), *LeaderboardId);

    FString RankQuery;
    RankQuery = FString::Printf(TEXT("?aroundRank=%d"), Rank);
    RankQuery += FString::Printf(TEXT("&limit=%d"), Range * 2 + 1);
    RankURL += RankQuery;

    HttpRequest->SetURL(RankURL);
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(AsShared(), &FOnlineLeaderboardsSimplyStream::OnReadLeaderboardsComplete, ReadObject);
    
    return HttpRequest->ProcessRequest();
}

bool FOnlineLeaderboardsSimplyStream::ReadLeaderboardsAroundUser(FUniqueNetIdRef Player, uint32 Range, FOnlineLeaderboardReadRef& ReadObject)
{
    if (ReadObject->LeaderboardName.IsEmpty())
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboardsAroundUser: Invalid leaderboard name"));
        return false;
    }

    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, *Player);
    
    if (!IdentityInt.IsValid() || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboardsAroundUser: User not logged in"));
        return false;
    }

    FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("ReadLeaderboardsAroundUser: No auth token"));
        return false;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    
    const FString LeaderboardId = FString(ReadObject->LeaderboardName);
    FString AroundUserURL = Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/leaderboards/%s"), *LeaderboardId);

    FString UserQuery = FString::Printf(TEXT("?aroundUser=%s"), *Player->ToString());
    UserQuery += FString::Printf(TEXT("&limit=%d"), Range * 2 + 1);
    AroundUserURL += UserQuery;

    HttpRequest->SetURL(AroundUserURL);
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(AsShared(), &FOnlineLeaderboardsSimplyStream::OnReadLeaderboardsComplete, ReadObject);
    
    return HttpRequest->ProcessRequest();
}

void FOnlineLeaderboardsSimplyStream::FreeStats(FOnlineLeaderboardRead& ReadObject)
{
    // Clear the rows
    ReadObject.Rows.Empty();
}

bool FOnlineLeaderboardsSimplyStream::WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject)
{
    if (WriteObject.LeaderboardNames.Num() == 0)
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("WriteLeaderboards: No leaderboards specified"));
        return false;
    }

    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, Player);
    
    if (!IdentityInt.IsValid() || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("WriteLeaderboards: User not logged in"));
        return false;
    }

    FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("WriteLeaderboards: No auth token"));
        return false;
    }

    // Process each leaderboard
    bool bAnyRequestSent = false;
    for (const FString& LeaderboardName : WriteObject.LeaderboardNames)
    {
        const FString LeaderboardNameStr = FString(LeaderboardName);

        // Find the score for this leaderboard
        const FVariantData* ScoreData = WriteObject.Properties.Find(LeaderboardNameStr);
        if (!ScoreData)
        {
            UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("WriteLeaderboards: No score data for leaderboard %s"), *LeaderboardNameStr);
            continue;
        }

        double Score = 0.0;
        if (ScoreData->GetType() == EOnlineKeyValuePairDataType::Double)
        {
            ScoreData->GetValue(Score);
        }
        else if (ScoreData->GetType() == EOnlineKeyValuePairDataType::Int32)
        {
            int32 IntValue = 0;
            ScoreData->GetValue(IntValue);
            Score = static_cast<double>(IntValue);
        }
        else if (ScoreData->GetType() == EOnlineKeyValuePairDataType::Int64)
        {
            int64 Int64Value = 0;
            ScoreData->GetValue(Int64Value);
            Score = static_cast<double>(Int64Value);
        }
        else
        {
            UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("WriteLeaderboards: Invalid score type for leaderboard %s"), *LeaderboardNameStr);
            continue;
        }

        // Build request JSON
        TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
        RequestJson->SetNumberField(TEXT("score"), Score);

        // Add any additional metadata
        TSharedPtr<FJsonObject> MetadataJson = MakeShared<FJsonObject>();
        for (const auto& PropertyPair : WriteObject.Properties)
        {
            if (PropertyPair.Key != LeaderboardNameStr)
            {
                FString PropertyValue;
                if (PropertyPair.Value.GetType() == EOnlineKeyValuePairDataType::String)
                {
                    PropertyPair.Value.GetValue(PropertyValue);
                    MetadataJson->SetStringField(PropertyPair.Key, PropertyValue);
                }
                else if (PropertyPair.Value.GetType() == EOnlineKeyValuePairDataType::Double)
                {
                    double DoubleValue = 0.0;
                    PropertyPair.Value.GetValue(DoubleValue);
                    MetadataJson->SetNumberField(PropertyPair.Key, DoubleValue);
                }
            }
        }
        
        if (MetadataJson->Values.Num() > 0)
        {
            RequestJson->SetObjectField(TEXT("metadata"), MetadataJson);
        }

        FString RequestContent;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
        FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

        TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
        HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/leaderboards/%s/submit"), *LeaderboardNameStr));
        HttpRequest->SetVerb(TEXT("POST"));
        HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
        HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        HttpRequest->SetContentAsString(RequestContent);

        HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(AsShared(), &FOnlineLeaderboardsSimplyStream::OnWriteLeaderboardsComplete, WriteObject, SessionName, Player.AsShared());
        
        if (HttpRequest->ProcessRequest())
        {
            bAnyRequestSent = true;
            UE_LOG_ONLINE_LEADERBOARD(Verbose, TEXT("WriteLeaderboards: Sent score %f to leaderboard %s"), Score, *LeaderboardNameStr);
        }
    }

    return bAnyRequestSent;
}

void FOnlineLeaderboardsSimplyStream::OnWriteLeaderboardsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FOnlineLeaderboardWrite WriteObject, FName SessionName, FUniqueNetIdRef Player)
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
                UE_LOG_ONLINE_LEADERBOARD(Verbose, TEXT("WriteLeaderboards: Score submitted successfully"));
                
                // Clear cached leaderboard data to force refresh
                FString LeaderboardId;
                if (WriteObject.LeaderboardNames.Num() > 0)
                {
                    LeaderboardId = FString(WriteObject.LeaderboardNames[0]);
                    LeaderboardCache.Remove(LeaderboardId);
                }
            }
            else
            {
                FString ErrorMsg = JsonObject->GetStringField(TEXT("error"));
                UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("WriteLeaderboards failed: %s"), *ErrorMsg);
            }
        }
    }
    else
    {
        UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("WriteLeaderboards: HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }

    // Nothing else to trigger yet; caller relies on HTTP result logs/cache busting above.
}

bool FOnlineLeaderboardsSimplyStream::FlushLeaderboards(const FName& SessionName)
{
    // Clear all cached leaderboard data
    LeaderboardCache.Empty();
    UE_LOG_ONLINE_LEADERBOARD(Display, TEXT("FlushLeaderboards: Cleared all cached leaderboard data"));
    return true;
}

bool FOnlineLeaderboardsSimplyStream::WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores)
{
    // Player ratings system not implemented yet
    UE_LOG_ONLINE_LEADERBOARD(Warning, TEXT("WriteOnlinePlayerRatings: Not implemented"));
    return false;
}
