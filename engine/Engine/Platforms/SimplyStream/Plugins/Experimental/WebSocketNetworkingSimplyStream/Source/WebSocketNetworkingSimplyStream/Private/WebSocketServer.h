// Copyright Epic Games, Inc. All Rights Reserved.
//
// SimplyStream dedicated-server WebSocket listener (USE_LIBWEBSOCKET=0 only). The actual listening socket
// is the Node 'ws' WebSocketServer created by the runtime runner (init_wss); this class bridges its
// connect/receive callbacks into the UE net driver and routes outgoing sends back out (see WebSocketBridge).
#pragma once
#include "IWebSocketServer.h"
#include "WebSocketNetworkingPrivate.h"
#include "HAL/CriticalSection.h"
#include "Containers/Map.h"

class FWebSocket;

class FWebSocketServer : public IWebSocketServer
{
public:
	FWebSocketServer() = default;

	//~ Begin IWebSocketServer interface
	virtual ~FWebSocketServer() override;
	virtual void	EnableHTTPServer(TArray<FWebSocketHttpMount> DirectoriesToServe, bool bSecure, const FWebSocketServerCertificates& Certificates) override;
	virtual bool	Init(uint32 Port, FWebSocketClientConnectedCallBack, FString BindAddress = TEXT("")) override;
	virtual void	SetFilterConnectionCallback(FWebSocketFilterConnectionCallback InFilterConnectionCallback) override;
	virtual void	SetNetworkProtocol(IWebSocketServer::ENetworkProtocol Protocol) override { NetworkProtocol = Protocol; }
	virtual void	Tick() override;
	virtual FString Info() override;
	//~ End IWebSocketServer interface

	/** Bridge callbacks — invoked on the JS main thread by connCB/recvCB/closeCB; only touch the locked inbox. */
	void OnBridgeConnect(int32 WsPos);
	void OnBridgeData(int32 WsPos, const uint8* Data, int32 Size);
	void OnBridgeClose(int32 WsPos);

	/** Called (game thread) by a server-side FWebSocket dtor so we stop routing to it. */
	void NotifySocketDestroyed(int32 WsPos);

	/** Callback for a new websocket connection to the server */
	FWebSocketClientConnectedCallBack ConnectedCallBack;

	/** Handler called to filter incoming websocket connections (unused on the Node bridge for now). */
	FWebSocketFilterConnectionCallback FilterConnectionCallback;

	uint32 ServerPort = 0;

private:
	/** Index of our Node 'ws' server (from init_wss). -1 until Init() succeeds. */
	int32 WssPos = -1;

	IWebSocketServer::ENetworkProtocol NetworkProtocol = IWebSocketServer::ENetworkProtocol::IPv4;

	/** Inbox filled on the JS main thread (OnBridge*), drained on the game thread in Tick(). */
	FCriticalSection BridgeLock;
	TArray<int32> PendingConnects;                       // ws_pos of newly-accepted connections
	TArray<TPair<int32, TArray<uint8>>> PendingData;     // (ws_pos, whole message) in arrival order — boundaries preserved
	TArray<int32> PendingCloses;                         // ws_pos of closed connections

	/** Game-thread owned routing table: ws_pos -> accepted socket (not owned; the net connection owns it). */
	TMap<int32, FWebSocket*> Connections;
};
