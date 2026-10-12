// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebTransportBridge.h"
#include "WebTransport.h"
#include "WebTransportServer.h"
#include "WebTransportNetworkingPrivate.h"
#include "HAL/CriticalSection.h"

#include <stdlib.h>
#include <emscripten/emscripten.h>
#include <emscripten/em_asm.h>
#include <emscripten/threading.h>

// ---------------------------------------------------------------------------------------------------
// Exports to the JS Module (matches the platform's KEEP_IN_MODULE pattern): `used` keeps them past DCE,
// `visibility("default")` adds them to the dynamic export table alongside the toolchain's explicit
// -sEXPORTED_FUNCTIONS allowlist. The Node runner / webtransport.js shim call them as Module._wt*.
// NOTE: names are wt-prefixed so they don't collide with the WebSocket plugin's connCB/recvCB/closeCB
// (both plugins link into the same wasm).
// ---------------------------------------------------------------------------------------------------
#define KEEP_IN_MODULE extern "C" __attribute__((used, visibility("default")))

// webtransport.js shim entry points (run on the main browser thread; see __proxy:'sync' in the shim).
extern "C"
{
	int  ss_wt_connect(const char* url, const uint8* certHash, int certHashLen);
	void ss_wt_send(int handle, const void* data, int len);
	void ss_wt_close(int handle);
}

// ===================================================================================================
// SERVER (dedicated server under Node)
// ===================================================================================================
static FCriticalSection GServerRegistryLock;
static TMap<int32, FWebTransportServer*> GServerRegistry;

// ===================================================================================================
// CLIENT (browser). Keyed by the shim handle so teardown can drop the entry and stale callbacks no-op.
// ===================================================================================================
static FCriticalSection GClientRegistryLock;
static TMap<int32, FWebTransport*> GClientRegistry;

namespace SimplyStreamWebTransportBridge
{
	// --- Server ---
	int32 CreateServer(FWebTransportServer* Server)
	{
		const int32 WtsPos = MAIN_THREAD_EM_ASM_INT({
			if (typeof init_wt !== 'function') { return -1; }
			return init_wt(Module);
		});
		if (WtsPos < 0)
		{
			UE_LOGF(LogWebTransportNetworking, Error, "WebTransport bridge: init_wt unavailable (not running under the Node WebTransport runner?)");
			return -1;
		}
		FScopeLock Lock(&GServerRegistryLock);
		GServerRegistry.Add(WtsPos, Server);
		return WtsPos;
	}

	void DestroyServer(int32 WtsPos)
	{
		FScopeLock Lock(&GServerRegistryLock);
		GServerRegistry.Remove(WtsPos);
	}

	void Send(int32 SessionPos, const uint8* Data, int32 Size)
	{
		MAIN_THREAD_EM_ASM({
			if (typeof wt_send === 'function') { wt_send(Module, $0, $1, $2); }
		}, SessionPos, Data, Size);
	}

	// --- Client ---
	int32 ConnectClient(const FString& Url, const TArray<uint8>& CertHash, FWebTransport* Client)
	{
		auto UrlAnsi = StringCast<ANSICHAR>(*Url);
		const int32 Handle = ss_wt_connect(UrlAnsi.Get(), CertHash.GetData(), CertHash.Num());
		if (Handle < 0)
		{
			return -1;
		}
		FScopeLock Lock(&GClientRegistryLock);
		GClientRegistry.Add(Handle, Client);
		return Handle;
	}

	void ClientSend(int32 Handle, const uint8* Data, int32 Size)
	{
		if (Handle >= 0)
		{
			ss_wt_send(Handle, Data, Size);
		}
	}

	void ClientClose(int32 Handle)
	{
		if (Handle < 0)
		{
			return;
		}
		{
			FScopeLock Lock(&GClientRegistryLock);
			GClientRegistry.Remove(Handle);
		}
		ss_wt_close(Handle);
	}
}

// ===================================================================================================
// Exported callbacks
// ===================================================================================================
extern "C"
{
	// --- Server: the Node runner calls these on the JS main thread ---
	KEEP_IN_MODULE void wtConnCB(int wts, int session)
	{
		FScopeLock Lock(&GServerRegistryLock);
		if (FWebTransportServer* Server = GServerRegistry.FindRef(wts))
		{
			Server->OnBridgeConnect(session);
		}
	}

	KEEP_IN_MODULE void wtRecvCB(int wts, int session, void* buf, int len)
	{
		{
			FScopeLock Lock(&GServerRegistryLock);
			if (FWebTransportServer* Server = GServerRegistry.FindRef(wts))
			{
				Server->OnBridgeData(session, (const uint8*)buf, len);
			}
		}
		::free(buf);
	}

	KEEP_IN_MODULE void wtCloseCB(int wts, int session)
	{
		FScopeLock Lock(&GServerRegistryLock);
		if (FWebTransportServer* Server = GServerRegistry.FindRef(wts))
		{
			Server->OnBridgeClose(session);
		}
	}

	// --- Client: the webtransport.js shim calls these on the JS main thread, identifying the transport by
	// its handle (held under GClientRegistryLock so a torn-down client makes these a safe no-op) ---
	KEEP_IN_MODULE void wtClientOpenCB(int handle)
	{
		FScopeLock Lock(&GClientRegistryLock);
		if (FWebTransport* Client = GClientRegistry.FindRef(handle))
		{
			Client->bConnectedSignal.store(true);
		}
	}

	KEEP_IN_MODULE void wtClientRecvCB(int handle, void* buf, int len)
	{
		{
			FScopeLock Lock(&GClientRegistryLock);
			if (FWebTransport* Client = GClientRegistry.FindRef(handle))
			{
				Client->OnRawRecieve(buf, len);
			}
		}
		::free(buf);
	}

	KEEP_IN_MODULE void wtClientCloseCB(int handle)
	{
		FScopeLock Lock(&GClientRegistryLock);
		if (FWebTransport* Client = GClientRegistry.FindRef(handle))
		{
			Client->bClosedSignal.store(true);
		}
	}

	KEEP_IN_MODULE void wtClientErrorCB(int handle)
	{
		FScopeLock Lock(&GClientRegistryLock);
		if (FWebTransport* Client = GClientRegistry.FindRef(handle))
		{
			Client->bErrorSignal.store(true);
		}
	}
}
