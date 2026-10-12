// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

class FWebTransport;
class FWebTransportServer;
class UWorld;

DECLARE_LOG_CATEGORY_EXTERN(LogWebTransportNetworking, Warning, All);

// WebTransport->WebSocket runtime fallback state (defined in WebTransportNetDriver.cpp). When a client
// WebTransport session fails to establish, the net driver sets GWebTransportSessionFailed (so the next
// driver selection's IsAvailable() returns false) and records the travel URL; the plugin module's
// OnNetworkFailure handler re-travels to it (now over WebSocket). Process-global, game-thread only.
extern bool GWebTransportSessionFailed;
extern FString GWebTransportRetryURL;
extern TWeakObjectPtr<UWorld> GWebTransportRetryWorld;
