// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class INetworkingWebTransport;

enum class EWebtransportConnectionFilterResult : uint8
{
	ConnectionAccepted,
	ConnectionRefused
};

DECLARE_DELEGATE(FWebTransportInfoCallBack);
DECLARE_DELEGATE_TwoParams(FWebTransportPacketReceivedCallBack, void* /*Data*/, int32 /*Data Size*/);
DECLARE_DELEGATE_OneParam(FWebTransportClientConnectedCallBack, INetworkingWebTransport* /*Socket*/);
DECLARE_DELEGATE_RetVal_TwoParams(EWebtransportConnectionFilterResult, FWebTransportFilterConnectionCallback, FString /*Origin*/, FString /*ClientIP*/);
						  
