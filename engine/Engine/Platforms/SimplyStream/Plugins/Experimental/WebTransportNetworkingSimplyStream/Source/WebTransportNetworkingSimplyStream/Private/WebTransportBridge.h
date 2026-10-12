// Copyright Epic Games, Inc. All Rights Reserved.
//
// JS<->wasm bridge for WebTransport. Two directions:
//  - SERVER (dedicated server under Node): the Node runner (node_test.mjs) provides init_wt(Module) /
//    wt_send(Module, session, data, len) and calls back the exported wtConnCB / wtRecvCB / wtCloseCB.
//  - CLIENT (browser): the webtransport.js js-library shim provides ss_wt_connect / ss_wt_send / ss_wt_close
//    (bridging to the browser global WebTransport, proxied to the main thread) and calls back the exported
//    wtClientOpenCB / wtClientRecvCB / wtClientCloseCB / wtClientErrorCB with the FWebTransport* as userData.
//
// All emscripten specifics live in WebTransportBridge.cpp.
#pragma once

#include "CoreMinimal.h"

class FWebTransportServer;
class FWebTransport;

namespace SimplyStreamWebTransportBridge
{
	// --- Server (Node) ---
	/** Create the Node WebTransport (HTTP/3) server and register Server for its callbacks. Returns the
	 *  server index (wts_pos), or -1 on failure. */
	int32 CreateServer(FWebTransportServer* Server);
	/** Stop routing JS callbacks to the server registered at WtsPos. */
	void DestroyServer(int32 WtsPos);
	/** Send a datagram out over one accepted server session (wt_send, on the JS main thread). */
	void Send(int32 SessionPos, const uint8* Data, int32 Size);

	// --- Client (browser) ---
	/** Open a browser WebTransport to Url (optionally pinning a server cert via its SHA-256 hash). Client
	 *  receives async open/recv/close/error callbacks routed back to it. Returns a shim handle, or -1. */
	int32 ConnectClient(const FString& Url, const TArray<uint8>& CertHash, FWebTransport* Client);
	/** Send a datagram on a client transport (drops if larger than maxDatagramSize). */
	void ClientSend(int32 Handle, const uint8* Data, int32 Size);
	/** Close + release a client transport. */
	void ClientClose(int32 Handle);
}
