// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/OnlineAchievementsInterface.h"
#include "OnlineSubsystemSimplyStreamTypes.h"

class FOnlineSubsystemSimplyStream;

/**
 * Interface for managing achievements on SimplyStream platform
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlineAchievementsSimplyStream : public IOnlineAchievements, public TSharedFromThis<FOnlineAchievementsSimplyStream, ESPMode::ThreadSafe>
{
public:
    FOnlineAchievementsSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
    virtual ~FOnlineAchievementsSimplyStream() = default;

    // IOnlineAchievements interface
    virtual void WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate = FOnAchievementsWrittenDelegate()) override;
    virtual void QueryAchievements(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate()) override;
    virtual void QueryAchievementDescriptions(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate()) override;
    virtual EOnlineCachedResult::Type GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement) override;
    virtual EOnlineCachedResult::Type GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement>& OutAchievements) override;
    virtual EOnlineCachedResult::Type GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc) override;
#if !UE_BUILD_SHIPPING
    virtual bool ResetAchievements(const FUniqueNetId& PlayerId) override;
#endif

private:
    /** Reference to the main subsystem */
    FOnlineSubsystemSimplyStream* Subsystem;

    /** Cached achievement data per user */
    TMap<FUniqueNetIdRef, TArray<FOnlineAchievement>> PlayerAchievements;

    /** Cached achievement descriptions */
    TMap<FString, FOnlineAchievementDesc> AchievementDescriptions;

    /** HTTP completion handlers */
    void OnQueryAchievementsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef PlayerId, FOnQueryAchievementsCompleteDelegate Delegate);
    void OnQueryAchievementDescriptionsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef PlayerId, FOnQueryAchievementsCompleteDelegate Delegate);
    void OnWriteAchievementsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef PlayerId, FOnAchievementsWrittenDelegate Delegate);
};

typedef TSharedPtr<FOnlineAchievementsSimplyStream, ESPMode::ThreadSafe> FOnlineAchievementsSimplyStreamPtr;
