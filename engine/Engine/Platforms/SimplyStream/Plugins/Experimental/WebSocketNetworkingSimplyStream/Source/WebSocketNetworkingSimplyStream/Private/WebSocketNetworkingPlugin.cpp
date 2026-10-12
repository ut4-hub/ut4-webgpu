// Copyright Epic Games, Inc. All Rights Reserved.

#include "IWebSocketNetworkingModule.h"
#include "WebSocketServer.h"
#include "Modules/ModuleManager.h"
#include "WebSocket.h"



class FWebSocketNetworkingPlugin : public IWebSocketNetworkingModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}


public:
	TUniquePtr<IWebSocketServer> CreateServer() override
	{
		return MakeUnique<FWebSocketServer>();
	}

	TSharedPtr<INetworkingWebSocket> CreateConnection(const FInternetAddr& ServerAddress) override
	{
		return MakeShared<FWebSocket>(ServerAddress);
	}

};

// Module name MUST match the .Build.cs class name and the .uplugin module entry. The /Script package
// for UWebSocketNetDriver/UWebSocketConnection is keyed off this name → /Script/WebSocketNetworkingSimplyStream.
IMPLEMENT_MODULE(FWebSocketNetworkingPlugin, WebSocketNetworkingSimplyStream)

DEFINE_LOG_CATEGORY(LogWebSocketNetworking);
