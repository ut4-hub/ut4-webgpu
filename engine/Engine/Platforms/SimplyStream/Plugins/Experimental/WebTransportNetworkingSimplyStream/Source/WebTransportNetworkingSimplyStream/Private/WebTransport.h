// Copyright Epic Games, Inc. All Rights Reserved.
//
// SimplyStream WebTransport wrapper. A real WebTransport session on both sides; we use DATAGRAMS
// (unreliable, unordered — UDP-like), which is the natural fit for UE's net layer (it runs its own
// sequencing/ack/resend). One Send = one datagram = one received packet; no length framing.
//
//  - Client mode: bridges to the browser global `WebTransport` via the webtransport.js shim
//    (SimplyStreamWebTransportBridge::ConnectClient/ClientSend/ClientClose). Emscripten has no native
//    WebTransport C API, so unlike the WebSocket client this is a JS shim, not emscripten/websocket.h.
//  - Server mode: a bridge to one Node @fails-components/webtransport session accepted by
//    FWebTransportServer (init_wt/wtConnCB/wtRecvCB/wtCloseCB/wt_send).
//
// Shim/bridge callbacks may fire on a different thread than the UE game thread, so they only push into a
// thread-safe queue / set atomics; the UE delegates fire on the game thread in HandlePacket()/Tick().
#pragma once
#include "INetworkingWebTransport.h"
#include "WebTransportNetworkingPrivate.h"
#include "Containers/Queue.h"
#include <netinet/in.h>
#include <atomic>

class FWebTransportServer;

class FWebTransport : public INetworkingWebTransport
{

public:

	// Initialize as client side session (browser WebTransport via the shim). ConnectHost: the travel URL's
	// hostname — used for the https:// dial when it's a domain (SetIp fails on domains, leaving ServerAddress
	// 0.0.0.0). Empty = dial ServerAddress.
	FWebTransport(const FInternetAddr& ServerAddress, const FString& ConnectHost = FString());

	// Initialize as server side session (bridged to a Node WebTransport session accepted by the server).
	FWebTransport(FWebTransportServer* InServer, int32 InWtsPos, int32 InWtPos);

	//~ Begin INetworkingWebTransport interface
	virtual ~FWebTransport() override;
	virtual void SetConnectedCallBack(FWebTransportInfoCallBack CallBack) override;
	virtual void SetErrorCallBack(FWebTransportInfoCallBack CallBack) override;
	virtual void SetReceiveCallBack(FWebTransportPacketReceivedCallBack CallBack) override;
	virtual void SetSocketClosedCallBack(FWebTransportInfoCallBack CallBack) override;
	virtual bool Send(const uint8* Data, uint32 Size, bool bPrependSize = true) override;
	virtual void Tick() override;
	virtual void Flush() override;
	virtual TArray<uint8> GetRawRemoteAddr(int32& OutPort) override;
	virtual FString RemoteEndPoint(bool bAppendPort) override;
	virtual FString LocalEndPoint(bool bAppendPort) override;
	virtual struct sockaddr_in* GetRemoteAddr() override { return &RemoteAddr; }
	virtual TArray<FString> GetUrlArgs() override;
	virtual FString GetUrlArgByName(const FString& Name) override;
	//~ End INetworkingWebTransport interface

// this was made public because of cross-platform build issues
public:
	void HandlePacket();
	void OnReceive(void* Data, uint32 Size);
	/** Enqueue a whole received datagram (thread-safe; delivered in HandlePacket on the game thread). */
	void OnRawRecieve(void* Data, uint32 Size);
	void OnClose();

	/** Client-only state, read by the net driver for the WebTransport->WebSocket fallback. */
	bool HasErrored() const { return bErrorSignal.load(); }
	bool IsConnected() const { return bConnectedFired; }

	/************************************************************************/
	/*	Various Socket callbacks											*/
	/************************************************************************/
	FWebTransportPacketReceivedCallBack  ReceivedCallback;
	FWebTransportInfoCallBack ConnectedCallBack;
	FWebTransportInfoCallBack ErrorCallBack;
	FWebTransportInfoCallBack SocketClosedCallback;

	/** Incoming whole datagrams (producer: shim/bridge thread; consumer: game thread). */
	TQueue<TArray<uint8>, EQueueMode::Mpsc> ReceiveQueue;
	/** Client: outgoing datagrams buffered until the session opens (the writer isn't ready before then). */
	TQueue<TArray<uint8>, EQueueMode::Mpsc> PendingSend;

	/** Connection-state signals set by shim/bridge callbacks (any thread), consumed in HandlePacket. */
	std::atomic<bool> bConnectedSignal{ false };
	std::atomic<bool> bClosedSignal{ false };
	std::atomic<bool> bErrorSignal{ false }; // sticky: set on failure, NOT cleared (the net driver reads HasErrored())
	bool bConnectedFired = false; // game-thread only
	bool bErrorFired = false;     // game-thread only: fire ErrorCallBack once without clearing bErrorSignal

	/** Client: the shim transport handle (for send/close). Server: the bridge session index. -1 if invalid. */
	std::atomic<int> Handle{ -1 };

	struct sockaddr_in RemoteAddr;

	/** Server side session or client side. */
	bool IsServerSide;

	/** Server-bridge linkage (valid when IsServerSide). Server may be nulled by ~FWebTransportServer. */
	FWebTransportServer* Server = nullptr;
	int32 WtsPos = -1;
	int32 WtPos = -1;

	friend class FWebTransportServer;
};
