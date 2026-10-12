// Copyright Epic Games, Inc. All Rights Reserved.
#include "WebTransport.h"
#include "WebTransportServer.h"
#include "WebTransportBridge.h"
#include "WebTransportNetworkingPrivate.h" // IWYU pragma: keep

#include "IPAddress.h"
#include "HAL/IConsoleManager.h"

#include <atomic>
#include <string.h>
#include <arpa/inet.h>

// Optional pinned server certificate hash (SHA-256, hex) for local/self-signed WebTransport dev. Empty in
// production (a CA-signed cert is validated normally). The Node runner logs the hash it generated.
static TAutoConsoleVariable<FString> CVarWtServerCertHash(
	TEXT("wt.ServerCertHash"),
	TEXT(""),
	TEXT("WebTransport server certificate SHA-256 hash (hex) to pin via serverCertificateHashes (local self-signed dev). Empty = normal CA validation."),
	ECVF_Default);

static TArray<uint8> ParseHexBytes(const FString& InHex)
{
	FString Hex = InHex;
	Hex.ReplaceInline(TEXT(":"), TEXT(""));
	Hex.ReplaceInline(TEXT(" "), TEXT(""));
	Hex.TrimStartAndEndInline();
	TArray<uint8> Bytes;
	for (int32 i = 0; i + 1 < Hex.Len(); i += 2)
	{
		Bytes.Add((uint8)FCString::Strtoi(*Hex.Mid(i, 2), nullptr, 16));
	}
	return Bytes;
}

// ---------------------------------------------------------------------------------------------------
// Client side: bridge to the browser global WebTransport via the webtransport.js shim. Datagrams only.
// ---------------------------------------------------------------------------------------------------
FWebTransport::FWebTransport(const FInternetAddr& ServerAddress, const FString& ConnectHost)
	: IsServerSide(false)
{
	memset(&RemoteAddr, 0, sizeof(RemoteAddr));
	inet_pton(AF_INET, TCHAR_TO_ANSI(*ServerAddress.ToString(false)), &RemoteAddr.sin_addr);
	RemoteAddr.sin_family = AF_INET;
	RemoteAddr.sin_port = htons(ServerAddress.GetPort());

	// WebTransport is always HTTP/3 over TLS — https://, never ws://.
	// Prefer the travel URL's hostname: a DOMAIN can't SetIp into ServerAddress (stays 0.0.0.0), and a
	// session endpoint is a cert-matched domain — dialing https://0.0.0.0:<port>/net was the observed failure.
	// Mirrors FWebSocket's ConnectHost handling.
	const FString HostPart = !ConnectHost.IsEmpty() ? FString::Printf(TEXT("%s:%d"), *ConnectHost, ServerAddress.GetPort()) : ServerAddress.ToString(true);
	const FString Url = FString::Printf(TEXT("https://%s/net"), *HostPart);
	const TArray<uint8> CertHash = ParseHexBytes(CVarWtServerCertHash.GetValueOnAnyThread());

	const int32 NewHandle = SimplyStreamWebTransportBridge::ConnectClient(Url, CertHash, this);
	Handle.store(NewHandle);
	if (NewHandle < 0)
	{
		UE_LOGF(LogWebTransportNetworking, Error, "WebTransport client connect failed for %ls (no WebTransport support / shim unavailable)", *Url);
		bErrorSignal.store(true);
	}
	else
	{
		UE_LOGF(LogWebTransportNetworking, Warning, "WebTransport %d connecting to %ls", NewHandle, *Url);
	}
}

// ---------------------------------------------------------------------------------------------------
// Server side: a bridge to one Node WebTransport session accepted by FWebTransportServer.
// ---------------------------------------------------------------------------------------------------
FWebTransport::FWebTransport(FWebTransportServer* InServer, int32 InWtsPos, int32 InWtPos)
	: bConnectedFired(true)
	, Handle(InWtPos)
	, IsServerSide(true)
	, Server(InServer)
	, WtsPos(InWtsPos)
	, WtPos(InWtPos)
{
	// Synthesize a unique loopback address per accepted session so the net driver can tell connections
	// apart (LowLevelSend matches by remote address string).
	//
	// This must be unique across the server process lifetime, not per bridge slot: the Node bridge can reuse
	// slot indices after disconnect, while Unreal may keep the old UNetConnection around until timeout. Reusing
	// the address lets a new client's stateless handshake collide with stale per-connection handshake state.
	static std::atomic<uint32> GServerWtConnSerial{ 0 };
	const uint32 Serial = GServerWtConnSerial.fetch_add(1u, std::memory_order_relaxed);
	memset(&RemoteAddr, 0, sizeof(RemoteAddr));
	RemoteAddr.sin_family = AF_INET;
	RemoteAddr.sin_addr.s_addr = htonl(0x7F000002u + (Serial / 65535u)); // 127.0.0.2 upward
	RemoteAddr.sin_port = htons((uint16)(1u + (Serial % 65535u)));       // 1..65535, never 0
}

bool FWebTransport::Send(const uint8* Data, uint32 Size, bool /*bPrependSize*/)
{
	// No framing: each Send is one datagram = one received packet. Datagrams are best-effort (may be
	// dropped/reordered) — UE's sequencing/ack layer handles that, like UDP.
	if (Size == 0)
	{
		return true;
	}

	if (IsServerSide)
	{
		const int CurrentHandle = Handle.load();
		if (CurrentHandle < 0)
		{
			return false;
		}
		SimplyStreamWebTransportBridge::Send(CurrentHandle, Data, (int32)Size);
		return true;
	}

	// Client. Buffer until the session opens (the datagram writer isn't ready before then). Datagrams are
	// unordered anyway, so this is purely "can't send yet", not an ordering guarantee.
	if (!bConnectedFired)
	{
		TArray<uint8> Buffer;
		Buffer.Append(Data, Size);
		PendingSend.Enqueue(MoveTemp(Buffer));
		return true;
	}

	SimplyStreamWebTransportBridge::ClientSend(Handle.load(), Data, (int32)Size);
	return true;
}

void FWebTransport::SetReceiveCallBack(FWebTransportPacketReceivedCallBack CallBack)
{
	ReceivedCallback = CallBack;
}

void FWebTransport::SetSocketClosedCallBack(FWebTransportInfoCallBack CallBack)
{
	SocketClosedCallback = CallBack;
}

FString FWebTransport::RemoteEndPoint(bool bAppendPort)
{
	ANSICHAR Buffer[INET_ADDRSTRLEN];
	FString Remote(ANSI_TO_TCHAR(inet_ntop(AF_INET, &RemoteAddr.sin_addr, Buffer, INET_ADDRSTRLEN)));
	if (bAppendPort)
	{
		Remote += FString::Printf(TEXT(":%i"), ntohs(RemoteAddr.sin_port));
	}
	return Remote;
}

FString FWebTransport::LocalEndPoint(bool /*bAppendPort*/)
{
	return FString(TEXT("127.0.0.1"));
}

TArray<FString> FWebTransport::GetUrlArgs()
{
	return TArray<FString>();
}

FString FWebTransport::GetUrlArgByName(const FString& /*Name*/)
{
	return FString(TEXT(""));
}

void FWebTransport::Tick()
{
	HandlePacket();
}

void FWebTransport::HandlePacket()
{
	if (!IsServerSide)
	{
		if (bConnectedSignal.load() && !bConnectedFired)
		{
			bConnectedFired = true;
			ConnectedCallBack.ExecuteIfBound();
			const int CurrentHandle = Handle.load();
			if (CurrentHandle >= 0)
			{
				TArray<uint8> Buffer;
				while (PendingSend.Dequeue(Buffer))
				{
					if (Buffer.Num() > 0)
					{
						SimplyStreamWebTransportBridge::ClientSend(CurrentHandle, Buffer.GetData(), Buffer.Num());
					}
				}
			}
		}
		// Fire the error delegate once but DO NOT clear bErrorSignal — the net driver's fast WebTransport->
		// WebSocket fallback reads HasErrored() (bErrorSignal) and must still see it on its next TickDispatch.
		if (bErrorSignal.load() && !bErrorFired)
		{
			bErrorFired = true;
			ErrorCallBack.ExecuteIfBound();
		}
		if (bClosedSignal.exchange(false))
		{
			OnClose();
		}
	}

	TArray<uint8> Message;
	while (ReceiveQueue.Dequeue(Message))
	{
		ReceivedCallback.ExecuteIfBound((void*)Message.GetData(), Message.Num());
	}
}

void FWebTransport::Flush()
{
	if (IsServerSide || !bConnectedFired)
	{
		return;
	}
	const int CurrentHandle = Handle.load();
	if (CurrentHandle < 0)
	{
		return;
	}
	TArray<uint8> Buffer;
	while (PendingSend.Dequeue(Buffer))
	{
		if (Buffer.Num() > 0)
		{
			SimplyStreamWebTransportBridge::ClientSend(CurrentHandle, Buffer.GetData(), Buffer.Num());
		}
	}
}

TArray<uint8> FWebTransport::GetRawRemoteAddr(int32& OutPort)
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

void FWebTransport::SetConnectedCallBack(FWebTransportInfoCallBack CallBack)
{
	ConnectedCallBack = CallBack;
}

void FWebTransport::SetErrorCallBack(FWebTransportInfoCallBack CallBack)
{
	ErrorCallBack = CallBack;
}

void FWebTransport::OnReceive(void* Data, uint32 Size)
{
	ReceivedCallback.ExecuteIfBound(Data, Size);
}

void FWebTransport::OnRawRecieve(void* Data, uint32 Size)
{
	TArray<uint8> Message;
	Message.Append((const uint8*)Data, Size);
	ReceiveQueue.Enqueue(MoveTemp(Message));
}

void FWebTransport::OnClose()
{
	SocketClosedCallback.ExecuteIfBound();
}

FWebTransport::~FWebTransport()
{
	ReceivedCallback.Unbind();

	if (IsServerSide)
	{
		if (Server)
		{
			Server->NotifySocketDestroyed(WtPos);
			Server = nullptr;
		}
	}
	else
	{
		SimplyStreamWebTransportBridge::ClientClose(Handle.load());
		Handle.store(-1);
	}
}
