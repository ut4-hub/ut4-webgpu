// Copyright Epic Games, Inc. All Rights Reserved.
#include "WebSocket.h"
#include "WebSocketServer.h"
#include "WebSocketBridge.h"
#include "WebSocketNetworkingPrivate.h" // IWYU pragma: keep

#include <string.h>
#include <arpa/inet.h>
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#include "../../../../../../Source/ThirdParty/wasm_utils/wndr_probe.h" // site-9 probe

// ---------------------------------------------------------------------------------------------------
// Client-side emscripten WebSocket callbacks. These may run on the main browser thread (the socket is
// created with createOnMainThread=EM_TRUE), so they ONLY push into thread-safe queues / set atomics; the
// UE delegates are fired on the game thread in FWebSocket::HandlePacket().
// ---------------------------------------------------------------------------------------------------
static EM_BOOL SS_WebSocket_OnOpen(int /*eventType*/, const EmscriptenWebSocketOpenEvent* Event, void* UserData)
{
	FWebSocket* Socket = (FWebSocket*)UserData;
	Socket->conn.store(Event->socket);
	Socket->bConnectedSignal.store(true);
	UE_LOGF(LogWebSocketNetworking, Verbose, "WebSocket opened");
	return EM_TRUE;
}

static EM_BOOL SS_WebSocket_OnError(int /*eventType*/, const EmscriptenWebSocketErrorEvent* /*Event*/, void* UserData)
{
	FWebSocket* Socket = (FWebSocket*)UserData;
	Socket->bErrorSignal.store(true);
	UE_LOGF(LogWebSocketNetworking, Error, "WebSocket error");
	return EM_TRUE;
}

static EM_BOOL SS_WebSocket_OnClose(int /*eventType*/, const EmscriptenWebSocketCloseEvent* /*Event*/, void* UserData)
{
	FWebSocket* Socket = (FWebSocket*)UserData;
	Socket->conn.store(-1);
	Socket->bClosedSignal.store(true);
	UE_LOGF(LogWebSocketNetworking, Warning, "WebSocket closed");
	return EM_TRUE;
}

static EM_BOOL SS_WebSocket_OnMessage(int /*eventType*/, const EmscriptenWebSocketMessageEvent* Event, void* UserData)
{
	FWebSocket* Socket = (FWebSocket*)UserData;
	if (!Event->isText)
	{
		// Binary frame — a whole UE packet. numBytes excludes the implicit trailing NUL emscripten appends.
		Socket->OnRawRecieve(Event->data, (uint32)Event->numBytes);
	}
	return EM_TRUE;
}

FWebSocket::FWebSocket(const FInternetAddr& ServerAddress, const FString& ConnectHost)
	: IsServerSide(false)
{
	memset(&RemoteAddr, 0, sizeof(RemoteAddr));
	inet_pton(AF_INET, TCHAR_TO_ANSI(*ServerAddress.ToString(false)), &RemoteAddr.sin_addr);
	RemoteAddr.sin_family = AF_INET;
	RemoteAddr.sin_port = htons(ServerAddress.GetPort());

	if (!emscripten_websocket_is_supported())
	{
		UE_LOGF(LogWebSocketNetworking, Error, "Emscripten WebSocket API not supported in this environment");
		return;
	}

	// Prefer the resolved connect-string host (may be a DOMAIN) over the numeric IP: a public server serves wss
	// under a hostname whose TLS cert must match, and a browser on an HTTPS page cannot use ws:// to a remote host.
	// A host is an IPv4 only if it PARSES like one (digits and dots) -> ws:// for local/testing (node_test.mjs
	// serves plain HTTP); anything else is a hostname -> wss://. The old first-char-is-a-letter heuristic
	// misclassified our fleet domains (they start with the server UUID, e.g. "692240ca-...") as IPs, producing
	// a ws:// dial that HTTPS pages hard-block as Mixed Content — the fallback died right here.
	const FString Host = (!ConnectHost.IsEmpty()) ? ConnectHost : ServerAddress.ToString(false);
	bool bLooksLikeIPv4 = Host.Len() > 0;
	for (const TCHAR C : Host)
	{
		if (!FChar::IsDigit(C) && C != TEXT('.'))
		{
			bLooksLikeIPv4 = false;
			break;
		}
	}
	const TCHAR* Scheme = bLooksLikeIPv4 ? TEXT("ws") : TEXT("wss");
	const FString Url = FString::Printf(TEXT("%s://%s:%d"), Scheme, *Host, ServerAddress.GetPort());
	auto UrlAnsi = StringCast<ANSICHAR>(*Url);

	EmscriptenWebSocketCreateAttributes Attr;
	emscripten_websocket_init_create_attributes(&Attr);
	Attr.url = UrlAnsi.Get();
	Attr.protocols = nullptr;
	Attr.createOnMainThread = EM_TRUE;

	ws = emscripten_websocket_new(&Attr);
	if (ws <= 0)
	{
		UE_LOGF(LogWebSocketNetworking, Error, "emscripten_websocket_new failed for %ls (rc=%d)", *Url, (int)ws);
		return;
	}
	UE_LOGF(LogWebSocketNetworking, Warning, "WebSocket %d connecting to %ls", (int)ws, *Url);

	emscripten_websocket_set_onopen_callback(ws, this, SS_WebSocket_OnOpen);
	emscripten_websocket_set_onerror_callback(ws, this, SS_WebSocket_OnError);
	emscripten_websocket_set_onclose_callback(ws, this, SS_WebSocket_OnClose);
	emscripten_websocket_set_onmessage_callback(ws, this, SS_WebSocket_OnMessage);
}

FWebSocket::FWebSocket(FWebSocketServer* InServer, int32 InWssPos, int32 InWsPos)
	: conn(InWsPos)
	, IsServerSide(true)
	, Server(InServer)
	, WssPos(InWssPos)
	, WsPos(InWsPos)
{
	// Synthesize a unique loopback address per accepted connection so the net driver can tell
	// connections apart (UWebSocketNetDriver::LowLevelSend matches by remote address string). The peer IP
	// isn't exposed through the Node bridge.
	//
	// MUST be unique across the SERVER'S LIFETIME, not per slot: the bridge reuses slot indices after a
	// disconnect, and a slot-derived address made a NEW client inherit the address of a lingering
	// half-dead connection (an abandoned earlier attempt lives ~120s until UNetConnection::Tick times it
	// out). The stateless handshake keys incoming packets by address, so the new client's game packets
	// were routed into the STALE connection's handshake state and rejected every time:
	//   "Rejecting game packet with invalid session id (1 vs 1) or connection id (2 vs 0)"
	// -> joins hung on the loading screen for up to two minutes after any abandoned attempt.
	static std::atomic<uint32> GServerWsConnSerial{ 0 };
	const uint32 Serial = GServerWsConnSerial.fetch_add(1u, std::memory_order_relaxed);
	memset(&RemoteAddr, 0, sizeof(RemoteAddr));
	RemoteAddr.sin_family = AF_INET;
	RemoteAddr.sin_addr.s_addr = htonl(0x7F000002u + (Serial / 65535u)); // 127.0.0.2 upward
	RemoteAddr.sin_port = htons((uint16)(1u + (Serial % 65535u)));       // 1..65535, never 0
}

bool FWebSocket::Send(const uint8* Data, uint32 Size, bool /*bPrependSize*/)
{
	// No length framing: each Send is one WebSocket message, and the receiver gets exactly these bytes as
	// one message (WS preserves boundaries). bPrependSize is intentionally ignored.
	if (Size == 0)
	{
		// emscripten_websocket_send_binary has nonnull data; UE packets are never empty, but guard anyway.
		return true;
	}

	if (IsServerSide)
	{
		const int CurrentConn = conn.load();
		if (CurrentConn < 0)
		{
			return false;
		}
		SimplyStreamWebSocketBridge::Send(CurrentConn, Data, (int32)Size);
		return true;
	}

	// Client. Buffer until the connection is fully established AND its backlog has been flushed — both
	// happen together in HandlePacket (game thread). Gate on bConnectedFired, NOT conn: onopen may set
	// conn from another thread before HandlePacket flushes PendingSend, and a direct send in that window
	// would jump ahead of still-buffered earlier packets (out-of-order on the reliable channel).
	if (!bConnectedFired)
	{
		TArray<uint8> Buffer;
		Buffer.Append(Data, Size);
		PendingSend.Enqueue(MoveTemp(Buffer));
		return true;
	}

	{
		FWndrProbeScope WndrProbe(9); // emscripten ws send: internal sync proxy to the main thread — see wndr_probe.h
		emscripten_websocket_send_binary(conn.load(), (void*)Data, Size);
	}
	return true;
}

void FWebSocket::SetReceiveCallBack(FWebSocketPacketReceivedCallBack CallBack)
{
	ReceivedCallback = CallBack;
}

void FWebSocket::SetSocketClosedCallBack(FWebSocketInfoCallBack CallBack)
{
	SocketClosedCallback = CallBack;
}

FString FWebSocket::RemoteEndPoint(bool bAppendPort)
{
	ANSICHAR Buffer[INET_ADDRSTRLEN];
	FString Remote(ANSI_TO_TCHAR(inet_ntop(AF_INET, &RemoteAddr.sin_addr, Buffer, INET_ADDRSTRLEN)));
	if (bAppendPort)
	{
		Remote += FString::Printf(TEXT(":%i"), ntohs(RemoteAddr.sin_port));
	}
	return Remote;
}

FString FWebSocket::LocalEndPoint(bool /*bAppendPort*/)
{
	// No way to get this from a browser; return generic localhost without port.
	return FString(TEXT("127.0.0.1"));
}

TArray<FString> FWebSocket::GetUrlArgs()
{
	return TArray<FString>();
}

FString FWebSocket::GetUrlArgByName(const FString& /*Name*/)
{
	return FString(TEXT(""));
}

void FWebSocket::Tick()
{
	HandlePacket();
}

void FWebSocket::HandlePacket()
{
	// Fire deferred client connection-state delegates on the game thread.
	if (!IsServerSide)
	{
		if (bConnectedSignal.load() && !bConnectedFired)
		{
			bConnectedFired = true;
			ConnectedCallBack.ExecuteIfBound();

			// Flush anything queued before the socket opened.
			const int CurrentConn = conn.load();
			if (CurrentConn >= 0)
			{
				TArray<uint8> Buffer;
				while (PendingSend.Dequeue(Buffer))
				{
					{
						FWndrProbeScope WndrProbe(9); // see wndr_probe.h
						emscripten_websocket_send_binary(CurrentConn, (void*)Buffer.GetData(), Buffer.Num());
					}
				}
			}
		}
		if (bErrorSignal.exchange(false))
		{
			ErrorCallBack.ExecuteIfBound();
		}
		if (bClosedSignal.exchange(false))
		{
			OnClose();
		}
	}

	// Deliver received whole messages (client and server).
	TArray<uint8> Message;
	while (ReceiveQueue.Dequeue(Message))
	{
		ReceivedCallback.ExecuteIfBound((void*)Message.GetData(), Message.Num());
	}
}

void FWebSocket::Flush()
{
	// Push any buffered outgoing messages (client). Do NOT run HandlePacket here — UE calls Flush during
	// connection close/handshake, and HandlePacket would reentrantly drain the inbound queue and fire the
	// connection delegates. Sends are otherwise async (emscripten posts them to the browser/Node).
	if (IsServerSide || !bConnectedFired)
	{
		return;
	}
	const int CurrentConn = conn.load();
	if (CurrentConn < 0)
	{
		return;
	}
	TArray<uint8> Buffer;
	while (PendingSend.Dequeue(Buffer))
	{
		if (Buffer.Num() > 0)
		{
			emscripten_websocket_send_binary(CurrentConn, (void*)Buffer.GetData(), Buffer.Num());
		}
	}
}

TArray<uint8> FWebSocket::GetRawRemoteAddr(int32& OutPort)
{
	OutPort = ntohs(RemoteAddr.sin_port);
	TArray<uint8> RawBuffer;
	uint32 IntAddr = RemoteAddr.sin_addr.s_addr;
	RawBuffer.Add((IntAddr >> 0) & 0xFF);
	RawBuffer.Add((IntAddr >> 8) & 0xFF);
	RawBuffer.Add((IntAddr >> 16) & 0xFF);
	RawBuffer.Add((IntAddr >> 24) & 0xFF);
	return RawBuffer;
}

void FWebSocket::SetConnectedCallBack(FWebSocketInfoCallBack CallBack)
{
	ConnectedCallBack = CallBack;
}

void FWebSocket::SetErrorCallBack(FWebSocketInfoCallBack CallBack)
{
	ErrorCallBack = CallBack;
}

void FWebSocket::OnReceive(void* Data, uint32 Size)
{
	ReceivedCallback.ExecuteIfBound(Data, Size);
}

void FWebSocket::OnRawRecieve(void* Data, uint32 Size)
{
	TArray<uint8> Message;
	Message.Append((const uint8*)Data, Size);
	ReceiveQueue.Enqueue(MoveTemp(Message));
}

void FWebSocket::OnClose()
{
	SocketClosedCallback.ExecuteIfBound();
}

FWebSocket::~FWebSocket()
{
	ReceivedCallback.Unbind();

	if (IsServerSide)
	{
		// Detach from the owning server's routing table (game thread). Server may already be gone (it nulls
		// our pointer in its dtor), so guard.
		if (Server)
		{
			Server->NotifySocketDestroyed(WsPos);
			Server = nullptr;
		}
	}
	else if (ws > 0)
	{
		emscripten_websocket_close(ws, 1000, "closing");
		emscripten_websocket_delete(ws);
		ws = -1;
	}
}
