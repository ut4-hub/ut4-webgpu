// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebSocketBridge.h"
#include "WebSocketServer.h"
#include "WebSocketNetworkingPrivate.h"
#include "HAL/CriticalSection.h"

#include <stdlib.h>
#include <emscripten/emscripten.h>
#include <emscripten/em_asm.h>
#include <emscripten/threading.h>

// The init_wss / ws_send JS functions live on the JS main thread (that's where Module.wss / Module.ws_array
// are created). MAIN_THREAD_EM_ASM proxies + blocks when called from a UE pthread and runs inline on a
// single-threaded build, so it is correct in both configurations.

// Registry mapping a server's wss_pos to its FWebSocketServer. connCB/recvCB run on the JS main thread and
// look the server up here; the lock is held across the OnBridge* call so the server can't be unregistered
// (and destroyed) mid-callback (DestroyServer takes the same lock at the top of ~FWebSocketServer).
static FCriticalSection GServerRegistryLock;
static TMap<int32, FWebSocketServer*> GServerRegistry;

namespace SimplyStreamWebSocketBridge
{
	int32 CreateServer(FWebSocketServer* Server)
	{
		const int32 WssPos = MAIN_THREAD_EM_ASM_INT({
			if (typeof init_wss !== 'function') { return -1; }
			return init_wss(Module);
		});

		if (WssPos < 0)
		{
			UE_LOGF(LogWebSocketNetworking, Error, "SimplyStream WebSocket bridge: init_wss unavailable (not running under the Node ws runner?)");
			return -1;
		}

		FScopeLock Lock(&GServerRegistryLock);
		GServerRegistry.Add(WssPos, Server);
		return WssPos;
	}

	void DestroyServer(int32 WssPos)
	{
		FScopeLock Lock(&GServerRegistryLock);
		GServerRegistry.Remove(WssPos);
	}

	void Send(int32 WsPos, const uint8* Data, int32 Size)
	{
		MAIN_THREAD_EM_ASM({
			if (typeof ws_send === 'function') { ws_send(Module, $0, $1, $2); }
		}, WsPos, Data, Size);
	}
}

// Export to the JS Module exactly like the rest of the SimplyStream platform (WebGPU's JS-called
// callbacks). `used` keeps them past DCE, `visibility("default")` puts them in the dynamic export table;
// with the default EXPORT_KEEPALIVE this exports them ALONGSIDE the toolchain's explicit
// -sEXPORTED_FUNCTIONS allowlist (SimplyStreamToolChain.cs) — which does NOT list these — so the Node
// runner's Module._connCB / _recvCB / _closeCB resolve. (Matches UE_Next427's KEEP_IN_MODULE.)
#define KEEP_IN_MODULE extern "C" __attribute__((used, visibility("default")))

// Called by the Node runner (Module._connCB) on the JS main thread when a client connects.
KEEP_IN_MODULE void connCB(int wss, int ws)
{
	FScopeLock Lock(&GServerRegistryLock);
	if (FWebSocketServer* Server = GServerRegistry.FindRef(wss))
	{
		Server->OnBridgeConnect(ws);
	}
}

// Called by the Node runner (Module._recvCB) on the JS main thread when a message arrives. `buf` was
// allocated by the runner via Module._malloc; we copy out of it (in OnBridgeData) and free it here.
KEEP_IN_MODULE void recvCB(int wss, int ws, void* buf, int len)
{
	{
		FScopeLock Lock(&GServerRegistryLock);
		if (FWebSocketServer* Server = GServerRegistry.FindRef(wss))
		{
			Server->OnBridgeData(ws, (const uint8*)buf, len);
		}
	}
	::free(buf);
}

// Called by the Node runner (Module._closeCB) on the JS main thread when a client disconnects.
KEEP_IN_MODULE void closeCB(int wss, int ws)
{
	FScopeLock Lock(&GServerRegistryLock);
	if (FWebSocketServer* Server = GServerRegistry.FindRef(wss))
	{
		Server->OnBridgeClose(ws);
	}
}
