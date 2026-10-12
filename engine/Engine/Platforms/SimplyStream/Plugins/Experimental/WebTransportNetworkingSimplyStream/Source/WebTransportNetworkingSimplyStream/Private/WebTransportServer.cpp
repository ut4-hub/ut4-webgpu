// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebTransportServer.h"
#include "WebTransport.h"
#include "WebTransportBridge.h"

void FWebTransportServer::EnableHTTPServer(TArray<FWebTransportHttpMount> InDirectoriesToServe, bool bInSecure, const FWebTransportServerCertificates& InCertificates)
{
	// HTTP/static content is served by the Node runtime runner, not by this net-driver server. No-op.
}

bool FWebTransportServer::Init(uint32 Port, FWebTransportClientConnectedCallBack CallBack, FString InBindAddress)
{
	ServerPort = Port;
	ConnectedCallBack = CallBack;

	// The actual listening socket is the Node WebTransport (HTTP/3) server (init_wt); it binds the WT port.
	// `Port` here is the net driver's configured WebTransportPort and is used only for Info()/logging.
	WssPos = SimplyStreamWebTransportBridge::CreateServer(this);
	if (WssPos < 0)
	{
		UE_LOGF(LogWebTransportNetworking, Error, "FWebTransportServer::Init failed to create the Node WebTransport bridge server");
		return false;
	}

	UE_LOGF(LogWebTransportNetworking, Log, "FWebTransportServer listening via Node WebTransport bridge (wts=%d, configured port %u)", WssPos, Port);
	return true;
}

void FWebTransportServer::SetFilterConnectionCallback(FWebTransportFilterConnectionCallback InFilterConnectionCallback)
{
	FilterConnectionCallback = MoveTemp(InFilterConnectionCallback);
}

void FWebTransportServer::OnBridgeConnect(int32 WsPos)
{
	// JS main thread. Enqueue only.
	FScopeLock Lock(&BridgeLock);
	PendingConnects.Add(WsPos);
}

void FWebTransportServer::OnBridgeData(int32 WsPos, const uint8* Data, int32 Size)
{
	// JS main thread. One call == one whole WebTransport message; keep it as a discrete entry so message
	// boundaries (== UE packet boundaries) are preserved when drained on the game thread.
	TArray<uint8> Message;
	Message.Append(Data, Size);

	FScopeLock Lock(&BridgeLock);
	PendingData.Emplace(WsPos, MoveTemp(Message));
}

void FWebTransportServer::OnBridgeClose(int32 WsPos)
{
	// JS main thread. Enqueue only.
	FScopeLock Lock(&BridgeLock);
	PendingCloses.Add(WsPos);
}

void FWebTransportServer::NotifySocketDestroyed(int32 WsPos)
{
	// Game thread (server-side FWebTransport dtor).
	Connections.Remove(WsPos);
}

void FWebTransportServer::Tick()
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
		FWebTransport* Socket = new FWebTransport(this, WssPos, WsPos);
		ConnectedCallBack.ExecuteIfBound(Socket);

		// The net driver takes ownership of the socket (via UWebTransportConnection::SetWebTransport, freed in
		// its FinishDestroy) ONLY if it accepts the connection — which it signals by binding the receive
		// callback (UWebTransportNetDriver::OnWebTransportClientConnected). If it rejected the connection or no
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
		if (FWebTransport* Socket = Connections.FindRef(Pair.Key))
		{
			Socket->OnRawRecieve(Pair.Value.GetData(), Pair.Value.Num());
		}
	}

	// 3) Closes. Mark the socket closed and fire its close delegate; drop it from routing. The net driver
	//    tears down the UNetConnection (and deletes the socket) on its own timeout.
	for (int32 WsPos : Closes)
	{
		if (FWebTransport* Socket = Connections.FindRef(WsPos))
		{
			Socket->Handle.store(-1);
			Socket->OnClose();
			Connections.Remove(WsPos);
		}
	}
}

FString FWebTransportServer::Info()
{
	return FString::Printf(TEXT("127.0.0.1:%i"), ServerPort);
}

FWebTransportServer::~FWebTransportServer()
{
	// Stop JS callbacks from finding us (takes the registry lock; a callback in flight finishes first).
	if (WssPos >= 0)
	{
		SimplyStreamWebTransportBridge::DestroyServer(WssPos);
		WssPos = -1;
	}

	// Detach any still-open accepted sockets so their later dtor doesn't call back into a freed server.
	for (TPair<int32, FWebTransport*>& Pair : Connections)
	{
		if (Pair.Value)
		{
			Pair.Value->Server = nullptr;
		}
	}
	Connections.Empty();
}
