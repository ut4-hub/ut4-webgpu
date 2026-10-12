'use strict';
// SimplyStream WebTransport client shim. Emscripten has no native WebTransport C API, so the wasm net
// driver bridges to the browser global `WebTransport` through these js-library functions. We use DATAGRAMS
// (unreliable/unordered — UDP-like), matching UE's net model.
//
// __proxy:'sync' runs each function on the MAIN browser thread, so the transport and its async datagram
// reader loop live where the event loop always pumps. Callbacks identify the transport by its integer
// HANDLE (not a wasm pointer), so the C++ side can drop a transport from its registry on teardown and any
// in-flight callback for that handle becomes a safe no-op (no use-after-free).
mergeInto(LibraryManager.library, {
	$WTState: { transports: [] }, // handle -> { transport, writer, open } | null

	ss_wt_connect__proxy: 'sync',
	ss_wt_connect__deps: ['$WTState'],
	ss_wt_connect: function(urlPtr, hashPtr, hashLen) {
		try {
			if (typeof WebTransport === 'undefined') {
				return -1; // C side treats -1 as immediate failure (no async callback)
			}
			// wasm64: pointers arrive as BigInt; UTF8ToString asserts on bigint and ABORTS the whole app
			// (observed on the first-ever client join travel). Number() it like every other pointer here.
			var url = UTF8ToString(Number(urlPtr));
			var opts = {};
			hashLen = Number(hashLen);
			if (hashLen > 0) {
				var src = HEAPU8.subarray(Number(hashPtr), Number(hashPtr) + hashLen);
				opts.serverCertificateHashes = [{ algorithm: 'sha-256', value: new Uint8Array(src) }];
			}
			var t = new WebTransport(url, opts);
			var entry = {
				transport: t,
				writer: null,
				open: false,
				rx: 0,
				tx: 0,
				droppedTx: 0,
				firstTxAt: 0,
				lastRxAt: 0,
				failed: false,
				watchdog: null
			};
			var handle = WTState.transports.length;
			WTState.transports.push(entry);
			var failTransport = function(reason, error) {
				if (entry.failed) return;
				entry.failed = true;
				if (entry.watchdog) {
					clearInterval(entry.watchdog);
					entry.watchdog = null;
				}
				console.warn('[SimplyStream WT] failing transport', handle, reason, error && (error.message || error));
				if (Module._wtClientErrorCB) Module._wtClientErrorCB(handle);
				try { t.close({ closeCode: 4000, reason: String(reason).slice(0, 80) }); } catch (_) {}
			};

			t.ready.then(function() {
				entry.open = true;
				try { entry.writer = t.datagrams.writable.getWriter(); } catch (e) {
					console.warn('[SimplyStream WT] datagram writer unavailable', handle, e && (e.message || e));
				}
				console.warn('[SimplyStream WT] open', handle, url);
				// The UE stateless connect handshake is request/response. If the browser can send datagrams but
				// receives none back, WT is not usable for gameplay; fail fast so the net driver can re-travel over
				// its secure WebSocket fallback instead of waiting for the generic 120s PendingNetGame timeout.
				entry.watchdog = setInterval(function() {
					if (!entry.open || entry.failed || entry.rx > 0 || entry.tx === 0 || entry.firstTxAt === 0) return;
					if ((Date.now() - entry.firstTxAt) > 7000) {
						failTransport('no inbound datagrams after client send');
					}
				}, 1000);
				if (Module._wtClientOpenCB) Module._wtClientOpenCB(handle);
				(async function() {
					try {
						var reader = t.datagrams.readable.getReader();
						for (;;) {
							var r = await reader.read();
							if (r.done) break;
							var data = r.value; // Uint8Array
							var len = data.byteLength;
							if (len <= 0) continue;
							entry.rx++;
							entry.lastRxAt = Date.now();
							if (entry.rx <= 8 || (entry.rx % 100) === 0) {
								console.warn('[SimplyStream WT] rx', handle, '#' + entry.rx, len + 'b');
							}
							try {
								var buf = Module._malloc(len);
								HEAPU8.set(data, Number(buf)); // bare heap view (matches HEAPU8 use above; Module.HEAP8 may be unattached)
								if (Module._wtClientRecvCB) {
									Module._wtClientRecvCB(handle, WNDR_JS_POINTER(Number(buf)), len);
								} else {
									throw new Error('Module._wtClientRecvCB is unavailable');
								}
							} catch (e) {
								console.error('[SimplyStream WT] recv callback failed', handle, e && (e.stack || e.message || e));
								failTransport('recv callback failed', e);
								break;
							}
						}
					} catch (e) {
						console.error('[SimplyStream WT] datagram reader failed', handle, e && (e.stack || e.message || e));
						failTransport('datagram reader failed', e);
					}
				})();
			}).catch(function(e) {
				console.warn('[SimplyStream WT] ready failed', handle, e && (e.message || e));
				failTransport('ready failed', e);
			});

			t.closed.then(function() {
				if (entry.watchdog) {
					clearInterval(entry.watchdog);
					entry.watchdog = null;
				}
				console.warn('[SimplyStream WT] closed', handle);
				if (Module._wtClientCloseCB) Module._wtClientCloseCB(handle);
			}).catch(function(e) {
				console.warn('[SimplyStream WT] close/error', handle, e && (e.message || e));
				failTransport('closed with error', e);
			});

			return handle;
		} catch (e) {
			return -1;
		}
	},

	ss_wt_send__proxy: 'sync',
	ss_wt_send__deps: ['$WTState'],
	ss_wt_send: function(handle, ptr, len) {
		var entry = WTState.transports[handle];
		if (!entry || !entry.open || !entry.writer) {
			if (entry && ++entry.droppedTx <= 4) {
				console.warn('[SimplyStream WT] send dropped before open/writer', handle, 'open=' + !!entry.open, 'writer=' + !!entry.writer);
			}
			return;
		}
		len = Number(len);
		// Copy out of the (shared) wasm heap: write() is async and the heap may change underneath it.
		var copy = new Uint8Array(HEAPU8.buffer, Number(ptr), len).slice();
		entry.tx++;
		if (entry.firstTxAt === 0) entry.firstTxAt = Date.now();
		if (entry.tx <= 8 || (entry.tx % 100) === 0) {
			console.warn('[SimplyStream WT] tx', handle, '#' + entry.tx, len + 'b');
		}
		try {
			var written = entry.writer.write(copy);
			if (written && typeof written.catch === 'function') {
				written.catch(function(e) {
					console.warn('[SimplyStream WT] send rejected', handle, e && (e.message || e));
					if (Module._wtClientErrorCB) Module._wtClientErrorCB(handle);
				});
			}
		} catch (e) {
			console.warn('[SimplyStream WT] send failed', handle, e && (e.message || e));
			if (Module._wtClientErrorCB) Module._wtClientErrorCB(handle);
		}
	},

	ss_wt_close__proxy: 'sync',
	ss_wt_close__deps: ['$WTState'],
	ss_wt_close: function(handle) {
		var entry = WTState.transports[handle];
		if (!entry) return;
		try { if (entry.writer) { entry.writer.close().catch(function(){}); } } catch (e) {}
		try { entry.transport.close(); } catch (e) {}
		WTState.transports[handle] = null;
	},
});
