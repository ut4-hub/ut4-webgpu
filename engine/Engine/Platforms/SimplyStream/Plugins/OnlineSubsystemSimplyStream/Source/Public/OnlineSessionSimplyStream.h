// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemSimplyStreamTypes.h"

class FOnlineSubsystemSimplyStream;

struct FPendingRegisterPlayersOperation : public TSharedFromThis<FPendingRegisterPlayersOperation, ESPMode::ThreadSafe>
{
	FPendingRegisterPlayersOperation(FName InSessionName, const TArray<FUniqueNetIdRef>& InPlayers)
		: SessionName(InSessionName)
		, Players(InPlayers)
		, Results()
		, Errors()
		, BackendRegistrationKeys()
		, PendingRequests(0)
	{
		Results.Init(false, Players.Num());
		Errors.Init(FString(), Players.Num());
		BackendRegistrationKeys.Init(FString(), Players.Num());
	}

	FName SessionName;
	TArray<FUniqueNetIdRef> Players;
	TArray<bool> Results;
	TArray<FString> Errors;
	TArray<FString> BackendRegistrationKeys;
	int32 PendingRequests;
};

struct FPendingUnregisterPlayersOperation : public TSharedFromThis<FPendingUnregisterPlayersOperation, ESPMode::ThreadSafe>
{
	FPendingUnregisterPlayersOperation(FName InSessionName, const TArray<FUniqueNetIdRef>& InPlayers)
		: SessionName(InSessionName)
		, Players(InPlayers)
		, Results()
		, Errors()
		, PendingRequests(0)
	{
		Results.Init(false, Players.Num());
		Errors.Init(FString(), Players.Num());
	}

	FName SessionName;
	TArray<FUniqueNetIdRef> Players;
	TArray<bool> Results;
	TArray<FString> Errors;
	int32 PendingRequests;
};

struct FPendingFindSessionsBatch
{
	TArray<TWeakPtr<FOnlineSessionSearch>> Searches;
};

struct FCachedFindSessionsBatch
{
	TArray<FOnlineSessionSearchResult> Results;
	double CachedAtSeconds = 0.0;
};

DECLARE_MULTICAST_DELEGATE_FourParams(FOnRegisterPlayersDetailed, FName /*SessionName*/, const FUniqueNetId& /*PlayerId*/, bool /*bSuccess*/, const FString& /*Error*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FOnUnregisterPlayersDetailed, FName /*SessionName*/, const FUniqueNetId& /*PlayerId*/, bool /*bSuccess*/, const FString& /*Error*/);

class FNamedOnlineSessionSimplyStream : public FNamedOnlineSession
{
public:
    FNamedOnlineSessionSimplyStream(FName InSessionName, const FOnlineSessionSettings& InSessionSettings)
        : FNamedOnlineSession(InSessionName, InSessionSettings)
    {
    }

    FNamedOnlineSessionSimplyStream(FName InSessionName, const FOnlineSession& Session)
        : FNamedOnlineSession(InSessionName, Session)
    {
    }
};

/**
 * Session search results for SimplyStream matchmaking
 */
class FOnlineSessionSearchResultSimplyStream : public FOnlineSessionSearchResult
{
public:
    FOnlineSessionSearchResultSimplyStream()
        : FOnlineSessionSearchResult()
        , MatchmakingGenerated(false)
        , AverageSkillRating(0)
        , SkillRatingSpread(0)
    {}

    /** Whether this session was created via matchmaking */
    bool MatchmakingGenerated;
    
    /** Average skill rating of players in the session */
    int32 AverageSkillRating;
    
    /** Skill rating spread (max - min) in the session */
    int32 SkillRatingSpread;
    
    /** Preferred regions of players */
    TArray<FString> PlayerRegions;
};

/**
 * Enhanced session interface for SimplyStream with matchmaking support
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlineSessionSimplyStream : public IOnlineSession, public TSharedFromThis<FOnlineSessionSimplyStream, ESPMode::ThreadSafe>
{
public:
    FOnlineSessionSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
    virtual ~FOnlineSessionSimplyStream() = default;

    // IOnlineSession interface
    virtual FUniqueNetIdPtr CreateSessionIdFromString(const FString& SessionIdStr) override;
    virtual class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings) override;
    virtual class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSession& Session) override;
    virtual FNamedOnlineSession* GetNamedSession(FName SessionName) override;
    virtual void RemoveNamedSession(FName SessionName) override;
    virtual EOnlineSessionState::Type GetSessionState(FName SessionName) const override;
    virtual bool HasPresenceSession() override;
    virtual bool IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId) override;
    virtual bool StartMatchmaking(const TArray<FUniqueNetIdRef>& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
    virtual bool CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName) override;
    virtual bool CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName) override;
    virtual bool FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
    virtual bool FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
    virtual bool FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate) override;
	virtual bool CancelFindSessions() override;
	virtual bool PingSearchResults(const FOnlineSessionSearchResult& SearchResult) override;
	virtual bool FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<FUniqueNetIdRef>& FriendList) override;
	virtual bool SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray<FUniqueNetIdRef>& Friends) override;
	virtual bool SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray<FUniqueNetIdRef>& Friends) override;
	virtual bool GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType) override;
	virtual bool GetResolvedConnectString(const class FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo) override;
	virtual FOnlineSessionSettings* GetSessionSettings(FName SessionName) override;
	virtual int32 GetNumSessions() override;
	virtual void DumpSessionState() override;

	// SimplyStream specific detailed delegates
	FOnRegisterPlayersDetailed& OnRegisterPlayersDetailed() { return RegisterPlayersDetailedEvent; }
	FOnUnregisterPlayersDetailed& OnUnregisterPlayersDetailed() { return UnregisterPlayersDetailedEvent; }
    virtual bool CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
    virtual bool CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
    virtual bool StartSession(FName SessionName) override;
    virtual bool UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData = true) override;
    virtual bool EndSession(FName SessionName) override;
    virtual bool DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate = FOnDestroySessionCompleteDelegate()) override;
    virtual bool JoinSession(int32 PlayerNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
    virtual bool JoinSession(const FUniqueNetId& PlayerId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
    virtual bool RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited) override;
    virtual bool RegisterPlayers(FName SessionName, const TArray<FUniqueNetIdRef>& Players, bool bWasInvited = false) override;
    virtual bool UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId) override;
    virtual bool UnregisterPlayers(FName SessionName, const TArray<FUniqueNetIdRef>& Players) override;
    virtual void RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate) override;
    virtual void UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate) override;

    // SimplyStream-specific matchmaking methods
    bool JoinMatchmakingQueue(const FUniqueNetId& UserId, const FString& ConfigId, const FString& QueueType, const FString& GameMode = TEXT("default"));
    bool LeaveMatchmakingQueue(const FUniqueNetId& UserId);
    bool GetMatchmakingStatus(const FUniqueNetId& UserId);
    bool RequestBackfill(const FUniqueNetId& UserId, const FString& MatchId, int32 SlotsNeeded);

    // Initialize/shutdown
    virtual bool Init();
    virtual void Tick(float DeltaTime);
    virtual void Shutdown();

private:
    /** Reference to the main subsystem */
    FOnlineSubsystemSimplyStream* SimplyStreamSubsystem;

    /** Current sessions */
    TArray<FNamedOnlineSessionSimplyStream> Sessions;

    /** Active matchmaking requests */
    TMap<FUniqueNetIdRef, FString> ActiveMatchmakingRequests; // UserId -> QueueId

    /** Session lock for thread safety */
    mutable FCriticalSection SessionLock;

    mutable FCriticalSection FindSessionsLock;
    TMap<FString, FPendingFindSessionsBatch> PendingFindSessions;
    TMap<FString, FCachedFindSessionsBatch> CachedFindSessions;

	TSet<FString> BackendRegisteredPlayerKeys;
	TSet<FString> BackendRegisterInFlightPlayerKeys;

    FOnRegisterPlayersDetailed RegisterPlayersDetailedEvent;
    FOnUnregisterPlayersDetailed UnregisterPlayersDetailedEvent;

    /**
     * Advertise this host's reachable endpoint on the backend session (PATCH connectionInfo) so joiners can
     * resolve a travel address. Only a dedicated server has one (a browser client cannot listen); the runtime
     * provides it via -SimplyStreamServerUrl=<host[:port]> or SIMPLYSTREAM_SERVER_URL. No-op when unset.
     */
    void RegisterServerConnectionInfo(const FString& SessionId);

    /**
     * Fire-and-forget POST .../sessions/<id>/leave — used to back out of a membership acquired by a join
     * that turned out to be unusable (e.g. the session advertises no endpoint), so the player doesn't stay
     * counted in a session they never traveled to.
     */
    void LeaveSessionById(const FString& SessionId, const FUniqueNetIdPtr& PlayerId);

    /** Apply Unreal-local registration side effects without implying another backend join. */
    bool ApplyLocalPlayerRegistration(FNamedOnlineSession& Session, const FUniqueNetIdRef& PlayerId);

    /** HTTP completion handlers */
    void OnCreateSessionRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, FString AuthToken);
    void PollCreateSessionAllocation(FName SessionName, const FString& AuthToken, const FString& AllocationId, int32 RemainingAttempts);
    void OnCreateSessionAllocationPollComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, FString AuthToken, FString AllocationId, int32 RemainingAttempts);
    void OnDestroySessionRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, FOnDestroySessionCompleteDelegate CompletionDelegate);
    void OnLeaveSessionRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, FOnDestroySessionCompleteDelegate CompletionDelegate);
    void OnJoinSessionRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, FOnlineSessionSearchResult DesiredSession, FUniqueNetIdPtr PlayerId);
    void OnRegisterPlayersRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, TSharedPtr<FPendingRegisterPlayersOperation> Operation, int32 PlayerIndex, FUniqueNetIdPtr PlayerId);
    void OnUnregisterPlayersRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, TSharedPtr<FPendingUnregisterPlayersOperation> Operation, int32 PlayerIndex, FUniqueNetIdPtr PlayerId);
    void OnUpdateSessionRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, TSharedPtr<FOnlineSessionSettings> PreviousSettings);
    void OnEndSessionRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName);
    void OnFindSessionsRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString QueryKey);
    void OnFindSessionByIdRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, FOnSingleSessionResultCompleteDelegate CompletionDelegate);
    void OnSendSessionInviteRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FName SessionName, FUniqueNetIdRef FriendId);
    void OnStartMatchmakingComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FName SessionName);
    void OnCancelMatchmakingComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId);

    /** Helper methods */
    FUniqueNetIdPtr GetUniquePlayerId(int32 LocalUserNum) const;
    FNamedOnlineSession* GetPresenceSession();
    FNamedOnlineSessionSimplyStream* GetNamedSessionSimplyStream(FName SessionName);
    int32 GetLocalUserIdx(const FUniqueNetId& UserId) const;
    void CompleteRegisterPlayersOperation(const TSharedPtr<FPendingRegisterPlayersOperation>& Operation);
    void CompleteUnregisterPlayersOperation(const TSharedPtr<FPendingUnregisterPlayersOperation>& Operation);
    void CompleteFindSessionsOperation(const FString& QueryKey, TArray<FOnlineSessionSearchResult>&& Results, bool bWasSuccessful);
    void TriggerFindSessionsCompleteNextTick(bool bWasSuccessful);
    void RegisterVoice(const FUniqueNetId& PlayerId);
    void UnregisterVoice(const FUniqueNetId& PlayerId);
    void BroadcastRegisterPlayersDetailed(FName SessionName, const TArray<FUniqueNetIdRef>& Players, bool bWasSuccessful, const FString& Error);
    void BroadcastUnregisterPlayersDetailed(FName SessionName, const TArray<FUniqueNetIdRef>& Players, bool bWasSuccessful, const FString& Error);
    void TriggerFindFriendSessionFailure(int32 LocalUserNum);
    void TrackPresenceForPlayer(const FUniqueNetId& PlayerId);
    void UntrackPresenceForPlayer(const FUniqueNetId& PlayerId);
};

typedef TSharedPtr<FOnlineSessionSimplyStream, ESPMode::ThreadSafe> FOnlineSessionSimplyStreamPtr;
