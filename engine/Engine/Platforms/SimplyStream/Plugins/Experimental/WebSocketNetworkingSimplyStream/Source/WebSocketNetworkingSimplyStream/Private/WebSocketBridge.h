// Copyright Epic Games, Inc. All Rights Reserved.
//
// JS<->wasm bridge for the dedicated-server WebSocket path. The Node runner (WasmServer/node_test.mjs)
// provides, in the runtime module scope: init_wss(Module) (creates a `ws` WebSocketServer on process.env.PORT
// and returns its index), ws_send(Module, ws, data, len), and it calls back into the wasm via the exported
// connCB / recvCB. This header exposes the C++-facing side; all emscripten specifics live in WebSocketBridge.cpp
// so they don't leak into the rest of the module.
#pragma once

#include "CoreMinimal.h"

class FWebSocketServer;

namespace SimplyStreamWebSocketBridge
{
	/**
	 * Create the Node 'ws' server (init_wss, on the JS main thread) and register Server to receive its
	 * connCB/recvCB callbacks. Returns the server index (wss_pos), or -1 on failure.
	 */
	int32 CreateServer(FWebSocketServer* Server);

	/** Stop routing JS callbacks to the server registered at WssPos. */
	void DestroyServer(int32 WssPos);

	/** Send bytes out over one accepted ws connection (ws_send, on the JS main thread). */
	void Send(int32 WsPos, const uint8* Data, int32 Size);
}
