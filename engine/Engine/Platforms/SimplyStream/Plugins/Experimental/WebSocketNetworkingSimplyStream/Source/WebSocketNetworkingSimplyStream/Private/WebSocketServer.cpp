// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebSocketServer.h"
#include "WebSocket.h"
#include "WebSocketBridge.h"

void FWebSocketServer::EnableHTTPServer(TArray<FWebSocketHttpMount> InDirectoriesToServe, bool bInSecure, const FWebSocketServerCertificates& InCertificates)
{
	// HTTP/static content is served by the Node runtime runner, not by this net-driver server. No-op.
}

bool FWebSocketServer::Init(uint32 Port, FWebSocketClientConnectedCallBack CallBack, FString InBindAddress)
{
	ServerPort = Port;
	ConnectedCallBack = CallBack;

	// The actual listening socket is the Node 'ws' WebSocketServer (init_wss); it binds process.env.PORT.
	// `Port` here is the net driver's configured WebSocketPort and is used only for Info()/logging.
	WssPos = SimplyStreamWebSocketBridge::CreateServer(this);
	if (WssPos < 0)
	{
		UE_LOGF(LogWebSocketNetworking, Error, "FWebSocketServer::Init failed to create the Node ws bridge server");
		return false;
	}

	UE_LOGF(LogWebSocketNetworking, Log, "FWebSocketServer listening via Node ws bridge (wss=%d, configured port %u)", WssPos, Port);
	return true;
}

void FWebSocketServer::SetFilterConnectionCallback(FWebSocketFilterConnectionCallback InFilterConnectionCallback)
{
	FilterConnectionCallback = MoveTemp(InFilterConnectionCallback);
}

void FWebSocketServer::OnBridgeConnect(int32 WsPos)
{
	// JS main thread. Enqueue only.
	FScopeLock Lock(&BridgeLock);
	PendingConnects.Add(WsPos);
}

void FWebSocketServer::OnBridgeData(int32 WsPos, const uint8* Data, int32 Size)
{
	// JS main thread. One call == one whole WebSocket message; keep it as a discrete entry so message
	// boundaries (== UE packet boundaries) are preserved when drained on the game thread.
	TArray<uint8> Message;
	Message.Append(Data, Size);

	FScopeLock Lock(&BridgeLock);
	PendingData.Emplace(WsPos, MoveTemp(Message));
}

void FWebSocketServer::OnBridgeClose(int32 WsPos)
{
	// JS main thread. Enqueue only.
	FScopeLock Lock(&BridgeLock);
	PendingCloses.Add(WsPos);
}

void FWebSocketServer::NotifySocketDestroyed(int32 WsPos)
{
	// Game thread (server-side FWebSocket dtor).
	Connections.Remove(WsPos);
}

void FWebSocketServer::Tick()
{
	TArray<int32> Connects;
	TArray<TPair<int32, TArray<uint8>>> Data;
	TArray<int32> Closes;
	{
		FScopeLock Lock(&BridgeLock);
		if (PendingConnects.Num() == 0 && PendingData.Num() == 0 && PendingCloses.Num() == 0)
		{
			return;
		}
		Connects = MoveTemp(PendingConnects);
		PendingConnects.Reset();
		Data = MoveTemp(PendingData);
		PendingData.Reset();
		Closes = MoveTemp(PendingCloses);
		PendingCloses.Reset();
	}

	// 1) New connections. connCB always precedes recvCB from JS, so process connects first; this both
	//    creates the socket and (via ConnectedCallBack -> net driver) binds its receive callback before
	//    we deliver any messages for it below.
	for (int32 WsPos : Connects)
	{
		if (Connections.Contains(WsPos))
		{
			continue; // duplicate connect guard
		}
		FWebSocket* Socket = new FWebSocket(this, WssPos, WsPos);
		ConnectedCallBack.ExecuteIfBound(Socket);

		// The net driver takes ownership of the socket (via UWebSocketConnection::SetWebSocket, freed in
		// its FinishDestroy) ONLY if it accepts the connection — which it signals by binding the receive
		// callback (UWebSocketNetDriver::OnWebSocketClientConnected). If it rejected the connection or no
		// callback is bound, nobody owns the socket: delete it now instead of leaking it and stranding a
		// routing entry that would silently swallow this peer's subsequent data.
		if (Socket->ReceivedCallback.IsBound())
		{
			Connections.Add(WsPos, Socket);
		}
		else
		{
			delete Socket;
		}
	}

	// 2) Deliver received whole messages (each preserved as its own packet). A miss means the connection
	//    was rejected or already closed -> drop. OnRawRecieve enqueues onto the socket; the connection's
	//    own Tick drains it via HandlePacket.
	for (TPair<int32, TArray<uint8>>& Pair : Data)
	{
		if (FWebSocket* Socket = Connections.FindRef(Pair.Key))
		{
			Socket->OnRawRecieve(Pair.Value.GetData(), Pair.Value.Num());
		}
	}

	// 3) Closes. Mark the socket closed and fire its close delegate; drop it from routing. The net driver
	//    tears down the UNetConnection (and deletes the socket) on its own timeout.
	for (int32 WsPos : Closes)
	{
		if (FWebSocket* Socket = Connections.FindRef(WsPos))
		{
			Socket->conn.store(-1);
			Socket->OnClose();
			Connections.Remove(WsPos);
		}
	}
}

FString FWebSocketServer::Info()
{
	return FString::Printf(TEXT("127.0.0.1:%i"), ServerPort);
}

FWebSocketServer::~FWebSocketServer()
{
	// Stop JS callbacks from finding us (takes the registry lock; a callback in flight finishes first).
	if (WssPos >= 0)
	{
		SimplyStreamWebSocketBridge::DestroyServer(WssPos);
		WssPos = -1;
	}

	// Detach any still-open accepted sockets so their later dtor doesn't call back into a freed server.
	for (TPair<int32, FWebSocket*>& Pair : Connections)
	{
		if (Pair.Value)
		{
			Pair.Value->Server = nullptr;
		}
	}
	Connections.Empty();
}
