// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineStatsInterface.h"
#include "OnlineSubsystemSimplyStreamTypes.h"

class FOnlineSubsystemSimplyStream;

/**
 * Interface for accessing player statistics on SimplyStream platform
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlineStatsSimplyStream : public IOnlineStats, public TSharedFromThis<FOnlineStatsSimplyStream, ESPMode::ThreadSafe>
{
public:
    FOnlineStatsSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
    virtual ~FOnlineStatsSimplyStream() = default;

    // IOnlineStats interface
    virtual void QueryStats(const FUniqueNetIdRef LocalUserId, const FUniqueNetIdRef StatsUser, const FOnlineStatsQueryUserStatsComplete& Delegate) override;
    virtual void QueryStats(const FUniqueNetIdRef LocalUserId, const TArray<FUniqueNetIdRef>& StatUsers, const TArray<FString>& StatNames, const FOnlineStatsQueryUsersStatsComplete& Delegate) override;
    virtual TSharedPtr<const FOnlineStatsUserStats> GetStats(const FUniqueNetIdRef StatsUserId) const override;
    virtual void UpdateStats(const FUniqueNetIdRef LocalUserId, const TArray<FOnlineStatsUserUpdatedStats>& UpdatedUserStats, const FOnlineStatsUpdateStatsComplete& Delegate) override;
#if !UE_BUILD_SHIPPING
    virtual void ResetStats(const FUniqueNetIdRef StatsUserId) override;
#endif

private:
    /** Reference to the main subsystem */
    FOnlineSubsystemSimplyStream* Subsystem;

    /** Cached stats data */
    TMap<FUniqueNetIdRef, TSharedRef<FOnlineStatsUserStats>> StatsCache;

    /** HTTP completion handlers */
    void OnQueryStatsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef LocalUserId, FUniqueNetIdRef StatsUser, FOnlineStatsQueryUserStatsComplete Delegate);
    void OnUpdateStatsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef LocalUserId, TArray<FOnlineStatsUserUpdatedStats> UpdatedUserStats, FOnlineStatsUpdateStatsComplete Delegate);
};

typedef TSharedPtr<FOnlineStatsSimplyStream, ESPMode::ThreadSafe> FOnlineStatsSimplyStreamPtr;
