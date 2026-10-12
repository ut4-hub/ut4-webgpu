// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineEventsInterface.h"
#include "OnlineSubsystemSimplyStreamPackage.h"
#include "OnlineSubsystemSimplyStreamTypes.h"
#include "Http.h"

class FOnlineSubsystemSimplyStream;

/**
 * Events interface for SimplyStream platform
 * Provides telemetry and analytics event tracking
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlineEventsInterfaceSimplyStream : public IOnlineEvents, public TSharedFromThis<FOnlineEventsInterfaceSimplyStream, ESPMode::ThreadSafe>
{
public:
	FOnlineEventsInterfaceSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
	virtual ~FOnlineEventsInterfaceSimplyStream() = default;

	// IOnlineEvents interface
	virtual bool TriggerEvent(const FUniqueNetId& PlayerId, const TCHAR* EventName, const FOnlineEventParms& Parms) override;
	virtual void SetPlayerSessionId(const FUniqueNetId& PlayerId, const FGuid& PlayerSessionId) override;

private:
	/** Reference to the main subsystem */
	FOnlineSubsystemSimplyStream* Subsystem;

	/** Cached player session IDs per user */
	TUniqueNetIdMap<FGuid> PlayerSessionIds;

	/** Critical section for thread-safe session ID access */
	mutable FCriticalSection SessionIdsLock;

	/** HTTP completion handler for event triggers */
	void OnTriggerEventComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString EventName);

	/** HTTP completion handler for session ID set */
	void OnSetPlayerSessionIdComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef PlayerId, FGuid SessionId);

	/** Helper to serialize event parameters to JSON */
	FString SerializeEventParms(const FOnlineEventParms& Parms);

	/** Helper methods */
	FString GetAPIBaseURL() const;
	FString GetJWTToken(int32 LocalUserNum) const;
	int32 GetLocalUserNum(const FUniqueNetId& UserId) const;
};

typedef TSharedPtr<FOnlineEventsInterfaceSimplyStream, ESPMode::ThreadSafe> FOnlineEventsInterfaceSimplyStreamPtr;
