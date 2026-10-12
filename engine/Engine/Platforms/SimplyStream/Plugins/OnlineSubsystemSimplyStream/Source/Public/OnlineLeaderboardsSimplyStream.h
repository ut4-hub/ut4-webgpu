// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineLeaderboardInterface.h"
#include "Templates/SharedPointer.h"
#include "OnlineSubsystemSimplyStreamTypes.h"

class FOnlineSubsystemSimplyStream;

/**
 * Interface for accessing leaderboards on SimplyStream platform
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlineLeaderboardsSimplyStream : public IOnlineLeaderboards, public TSharedFromThis<FOnlineLeaderboardsSimplyStream, ESPMode::ThreadSafe>
{
public:
    FOnlineLeaderboardsSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
    virtual ~FOnlineLeaderboardsSimplyStream() = default;

    // IOnlineLeaderboards interface
    virtual bool ReadLeaderboards(const TArray<FUniqueNetIdRef>& Players, FOnlineLeaderboardReadRef& ReadObject) override;
    virtual bool ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject) override;
    virtual bool ReadLeaderboardsAroundRank(int32 Rank, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
    virtual bool ReadLeaderboardsAroundUser(FUniqueNetIdRef Player, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
    virtual void FreeStats(FOnlineLeaderboardRead& ReadObject) override;
    virtual bool WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject) override;
    virtual bool FlushLeaderboards(const FName& SessionName) override;
    virtual bool WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores) override;

private:
    /** Reference to the main subsystem */
    FOnlineSubsystemSimplyStream* Subsystem;

    /** Cached leaderboard data */
    TMap<FString, TSharedRef<FOnlineLeaderboardRead>> LeaderboardCache;

    /** HTTP completion handlers */
    void OnReadLeaderboardsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FOnlineLeaderboardReadRef ReadObject);
    void OnWriteLeaderboardsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FOnlineLeaderboardWrite WriteObject, FName SessionName, FUniqueNetIdRef Player);
};

typedef TSharedPtr<FOnlineLeaderboardsSimplyStream, ESPMode::ThreadSafe> FOnlineLeaderboardsSimplyStreamPtr;
