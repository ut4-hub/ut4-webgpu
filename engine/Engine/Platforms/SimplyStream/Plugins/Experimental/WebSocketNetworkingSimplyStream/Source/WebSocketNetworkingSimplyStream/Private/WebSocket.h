// Copyright Epic Games, Inc. All Rights Reserved.
//
// SimplyStream WebSocket wrapper (USE_LIBWEBSOCKET=0 only). Adopts the proven SimplyStream/PLATFORM_WONDER
// design (UE_Next427): a REAL WebSocket on both sides, so message boundaries are preserved (1 Send = 1 WS
// message = 1 received packet) and no manual length-framing is needed.
//
//  - Client mode: emscripten WebSocket API (emscripten/websocket.h) — a true ws:// client, event-driven.
//    Requires the toolchain's `-lwebsocket.js` link flag (SimplyStreamToolChain.cs). Used by InitConnect.
//  - Server mode: a bridge to one Node 'ws' connection accepted by FWebSocketServer (init_wss/connCB/recvCB/
//    closeCB/ws_send). No emscripten socket; used by the dedicated server's accepted connections.
//
// The emscripten WS client callbacks are proxied to the game thread (the socket is created there, with the
// default CALLING_THREAD callback context); the JS bridge callbacks (connCB/recvCB/closeCB) fire on the JS
// main thread. To be correct under either delivery thread, every callback only pushes into a thread-safe
// queue / sets an atomic — the UE delegates are fired solely on the game thread in HandlePacket()/Tick().
#pragma once
#include "INetworkingWebSocket.h"
#include "WebSocketNetworkingPrivate.h"
#include "Containers/Queue.h"
#include <netinet/in.h>
#include <emscripten/websocket.h>
#include <atomic>

class FWebSocketServer;

class FWebSocket : public INetworkingWebSocket
{

public:

	// Initialize as client side socket (emscripten ws://|wss:// client).
	// ConnectHost: the resolved connect-string host (may be a DOMAIN, e.g. from OSS JoinSession connectionInfo).
	// A browser client on an HTTPS page must dial wss://<domain> (mixed-content rules + a TLS cert that matches
	// the hostname), so the domain — not the resolved IP — has to reach the emscripten WebSocket URL. Defaults
	// empty (falls back to the IP, plain ws) so existing/server-side callers are unaffected.
	FWebSocket(const FInternetAddr& ServerAddress, const FString& ConnectHost = FString());

	// Initialize as server side socket (bridged to a Node 'ws' connection accepted by FWebSocketServer).
	FWebSocket(FWebSocketServer* InServer, int32 InWssPos, int32 InWsPos);

	//~ Begin INetworkingWebSocket interface
	virtual ~FWebSocket() override;
	virtual void SetConnectedCallBack(FWebSocketInfoCallBack CallBack) override;
	virtual void SetErrorCallBack(FWebSocketInfoCallBack CallBack) override;
	virtual void SetReceiveCallBack(FWebSocketPacketReceivedCallBack CallBack) override;
	virtual void SetSocketClosedCallBack(FWebSocketInfoCallBack CallBack) override;
	virtual bool Send(const uint8* Data, uint32 Size, bool bPrependSize = true) override;
	virtual void Tick() override;
	virtual void Flush() override;
	virtual TArray<uint8> GetRawRemoteAddr(int32& OutPort) override;
	virtual FString RemoteEndPoint(bool bAppendPort) override;
	virtual FString LocalEndPoint(bool bAppendPort) override;
	virtual struct sockaddr_in* GetRemoteAddr() override { return &RemoteAddr; }
	virtual TArray<FString> GetUrlArgs() override;
	virtual FString GetUrlArgByName(const FString& Name) override;
	//~ End INetworkingWebSocket interface

// this was made public because of cross-platform build issues
public:
	void HandlePacket();
	void OnReceive(void* Data, uint32 Size);
	/** Enqueue a whole received WebSocket message (thread-safe; delivered in HandlePacket on the game thread). */
	void OnRawRecieve(void* Data, uint32 Size);
	void OnClose();

	/************************************************************************/
	/*	Various Socket callbacks											*/
	/************************************************************************/
	FWebSocketPacketReceivedCallBack  ReceivedCallback;
	FWebSocketInfoCallBack ConnectedCallBack;
	FWebSocketInfoCallBack ErrorCallBack;
	FWebSocketInfoCallBack SocketClosedCallback;

	/** Incoming whole messages (producer: emscripten/bridge callback thread; consumer: game thread). */
	TQueue<TArray<uint8>, EQueueMode::Mpsc> ReceiveQueue;
	/** Client: outgoing messages buffered until the socket opens. */
	TQueue<TArray<uint8>, EQueueMode::Mpsc> PendingSend;

	/** Connection-state signals set by emscripten callbacks (any thread), consumed in HandlePacket (game thread). */
	std::atomic<bool> bConnectedSignal{ false };
	std::atomic<bool> bClosedSignal{ false };
	std::atomic<bool> bErrorSignal{ false };
	bool bConnectedFired = false; // game-thread only

	/** Client: the emscripten WebSocket handle (used to close); -1 in server mode. */
	EMSCRIPTEN_WEBSOCKET_T ws = -1;
	/** Client: the connected socket id (set in onopen); Server: the ws_array index. -1 when not connected. */
	std::atomic<int> conn{ -1 };

	struct sockaddr_in RemoteAddr;

	/** Server side socket or client side*/
	bool IsServerSide;

	/** Server-bridge linkage (valid when IsServerSide). Server may be nulled by ~FWebSocketServer at teardown. */
	FWebSocketServer* Server = nullptr;
	int32 WssPos = -1;
	int32 WsPos = -1;

	friend class FWebSocketServer;
};
