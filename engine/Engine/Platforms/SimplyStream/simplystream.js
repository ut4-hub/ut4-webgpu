// Pointer-sized wasm exports use BigInt in MEMORY64 modes and Number in native
// wasm32. MEMORY64=2 lowers the stack pointer to i32 while pointer-taking
// exports keep their i64 JavaScript signature, so probe free(nullptr) instead
// of inferring the ABI from stackSave(). Cache the result for every bridge.
let wndrJsPointerConverter;
function WNDR_JS_POINTER(value) {
	if (wndrJsPointerConverter === undefined) {
		const wasmFree = wasmExports?.free;
		if (typeof wasmFree !== 'function') {
			throw new Error('WNDR_JS_POINTER used before the wasm free export was initialized');
		}
		try {
			wasmFree(0n);
			wndrJsPointerConverter = BigInt;
		} catch (error) {
			if (!(error instanceof TypeError)) throw error;
			wasmFree(0);
			wndrJsPointerConverter = Number;
		}
	}
	return wndrJsPointerConverter(value);
}
function WNDR_JS_SIZE_T(value) {
	return WNDR_JS_POINTER(value);
}
function WNDR_JS_UINT64(value) {
	return BigInt(value);
}
function WNDR_JS_POINTER_SIZE() {
	WNDR_JS_POINTER(0);
	return wndrJsPointerConverter === BigInt ? 8 : 4;
}

function wndrNodeServerEndpoint(nodeEnv) {
	if (nodeEnv.SIMPLYSTREAM_SERVER_URL) {
		return nodeEnv.SIMPLYSTREAM_SERVER_URL;
	}

	const certPath = nodeEnv.__CERT_FULLCHAIN_PATH || nodeEnv.__CERT_CERT_PATH || '';
	const pathParts = certPath.replace(/\\/g, '/').split('/').filter(Boolean);
	const certDomain = pathParts.length > 1 ? pathParts[pathParts.length - 2] : '';
	if (certDomain && certDomain !== 'certs' && nodeEnv.PORT) {
		return certDomain + ':' + nodeEnv.PORT;
	}
	if (nodeEnv.SERVER_ID && nodeEnv.PORT) {
		return nodeEnv.SERVER_ID + '.multiplayer.simplystream.dev:' + nodeEnv.PORT;
	}
	return '';
}

function wndrInstallNodeServerEnvironment() {
	if (typeof process === 'undefined' || !process.versions?.node || typeof ENV === 'undefined') {
		return;
	}

	const nodeEnv = process.env || {};
	const backendUrl = nodeEnv.BACKEND_URL || 'https://simplystream.com';
	const values = {
		PROJECT_ID: nodeEnv.PROJECT_ID || '',
		SERVER_ID: nodeEnv.SERVER_ID || '',
		API_KEY: nodeEnv.WASM_API_KEY || nodeEnv.API_KEY || '',
		BACKEND_URL: backendUrl,
		SIMPLYSTREAM_BACKEND_URL: nodeEnv.SIMPLYSTREAM_BACKEND_URL || backendUrl,
		PORT: nodeEnv.PORT || '',
		REV: nodeEnv.REV || '',
		SERVER_NAME: nodeEnv.SERVER_NAME || '',
		CLIENT_API: nodeEnv.CLIENT_API || '',
		SIMPLYSTREAM_SERVER_URL: wndrNodeServerEndpoint(nodeEnv),
	};
	for (const [key, value] of Object.entries(values)) {
		if (value) {
			ENV[key] = value;
		}
	}
}

// Emscripten's locally patched html5 library and older Node server hosts still
// emit MEMORY64-era BigInt pointers. Normalize their exported callbacks after
// wasm instantiation, before input and network handlers are registered.
Module.preRun ||= [];
Module.preRun.push(function () {
	if (typeof _proxyKeyboardEvent === 'function') {
		const rawProxyKeyboardEvent = _proxyKeyboardEvent;
		_proxyKeyboardEvent = (eventType, eventPtr) => rawProxyKeyboardEvent(eventType, WNDR_JS_POINTER(eventPtr));
		Module._proxyKeyboardEvent = _proxyKeyboardEvent;
	}
	if (typeof _proxyMouseEvent === 'function') {
		const rawProxyMouseEvent = _proxyMouseEvent;
		_proxyMouseEvent = (eventType, eventPtr) => rawProxyMouseEvent(eventType, WNDR_JS_POINTER(eventPtr));
		Module._proxyMouseEvent = _proxyMouseEvent;
	}
	if (typeof _proxyPointerLockEvent === 'function') {
		const rawProxyPointerLockEvent = _proxyPointerLockEvent;
		_proxyPointerLockEvent = (eventType, eventPtr) => rawProxyPointerLockEvent(eventType, WNDR_JS_POINTER(eventPtr));
		Module._proxyPointerLockEvent = _proxyPointerLockEvent;
	}
	if (typeof Module._recvCB === 'function') {
		const rawRecvCB = Module._recvCB;
		Module._recvCB = (server, socket, data, size) => rawRecvCB(server, socket, WNDR_JS_POINTER(data), size);
	}
	if (typeof Module._wtRecvCB === 'function') {
		const rawWtRecvCB = Module._wtRecvCB;
		Module._wtRecvCB = (server, session, data, size) => rawWtRecvCB(server, session, WNDR_JS_POINTER(data), size);
	}
	if (typeof Module._wtClientRecvCB === 'function') {
		const rawWtClientRecvCB = Module._wtClientRecvCB;
		Module._wtClientRecvCB = (handle, data, size) => rawWtClientRecvCB(handle, WNDR_JS_POINTER(data), size);
	}
});
Module.preRun.push(wndrInstallNodeServerEnvironment);

// Dedicated servers need a Node WebSocket listener behind the platform net-driver bridge.
// Keep the bridge in the generated runtime so a host refactor cannot silently omit it.
function wndrInstallNodeWebSocketBridge() {
	if (typeof ENVIRONMENT_IS_NODE === 'undefined' || !ENVIRONMENT_IS_NODE) {
		return false;
	}

	if (typeof globalThis.init_wss !== 'function') {
		globalThis.init_wss = function (RuntimeModule) {
			try {
				const { WebSocketServer } = require('ws');
				const { createServer } = require('https');
				const { readFileSync } = require('fs');
				const keyPath = process.env.__CERT_PRIVKEY_PATH;
				const certPath = process.env.__CERT_FULLCHAIN_PATH || process.env.__CERT_CERT_PATH;
				const port = Number.parseInt(process.env.PORT || '', 10);

				if (!keyPath || !certPath || !Number.isInteger(port) || port <= 0) {
					console.error('[SimplyStream WS] missing TLS certificate paths or PORT');
					return -1;
				}
				for (const callback of ['_malloc', '_connCB', '_recvCB', '_closeCB']) {
					if (typeof RuntimeModule[callback] !== 'function') {
						console.error('[SimplyStream WS] missing wasm callback ' + callback);
						return -1;
					}
				}

				RuntimeModule.ws_array ||= [];
				RuntimeModule.wss_array ||= [];
				const httpServer = createServer({
					key: readFileSync(keyPath),
					cert: readFileSync(certPath),
				}, (_request, response) => {
					response.writeHead(404);
					response.end();
				});
				const webSocketServer = new WebSocketServer({ server: httpServer });
				RuntimeModule.wss = webSocketServer;
				RuntimeModule.wss_array.push(webSocketServer);
				const serverIndex = RuntimeModule.wss_array.length - 1;

				webSocketServer.on('connection', (socket) => {
					socket.binaryType = 'arraybuffer';
					RuntimeModule.ws_array.push(socket);
					const socketIndex = RuntimeModule.ws_array.length - 1;
					RuntimeModule._connCB(serverIndex, socketIndex);

					socket.on('message', (data) => {
						let bytes;
						if (Array.isArray(data)) {
							const size = data.reduce((total, part) => total + part.byteLength, 0);
							bytes = new Uint8Array(size);
							let offset = 0;
							for (const part of data) {
								const view = new Uint8Array(part.buffer, part.byteOffset, part.byteLength);
								bytes.set(view, offset);
								offset += view.byteLength;
							}
						} else if (data instanceof ArrayBuffer) {
							bytes = new Uint8Array(data);
						} else if (ArrayBuffer.isView(data)) {
							bytes = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
						} else {
							bytes = new Uint8Array(data);
						}

						const dataPtr = bytes.byteLength ? RuntimeModule._malloc(bytes.byteLength) : 0;
						if (bytes.byteLength) {
							RuntimeModule.HEAPU8.set(bytes, Number(dataPtr));
						}
						RuntimeModule._recvCB(serverIndex, socketIndex, dataPtr, bytes.byteLength);
					});
					socket.once('close', () => {
						RuntimeModule.ws_array[socketIndex] = null;
						RuntimeModule._closeCB(serverIndex, socketIndex);
					});
					socket.on('error', (error) => {
						console.error('[SimplyStream WS] socket error', error);
					});
				});
				webSocketServer.on('error', (error) => {
					console.error('[SimplyStream WS] server error', error);
				});
				httpServer.on('error', (error) => {
					console.error('[SimplyStream WS] HTTPS error', error);
				});
				httpServer.listen(port, '0.0.0.0', () => {
					// multiplayer-runtime uses this stable marker before promoting the server to ready.
					console.log('SimplyStream WS bridge listening on port ' + port);
				});
				return serverIndex;
			} catch (error) {
				console.error('[SimplyStream WS] failed to initialize', error);
				return -1;
			}
		};
	}

	if (typeof globalThis.ws_send !== 'function') {
		globalThis.ws_send = function (RuntimeModule, socketIndex, dataPtr, dataLength) {
			const socket = RuntimeModule.ws_array?.[Number(socketIndex)];
			if (!socket || socket.readyState !== 1) {
				return -1;
			}
			const begin = Number(dataPtr);
			const end = begin + Number(dataLength);
			socket.send(RuntimeModule.HEAPU8.slice(begin, end));
			return 0;
		};
	}

	return true;
}
wndrInstallNodeWebSocketBridge();

let fs_type = 0; // 0 = cacheapi, 1 = opfs
let fast_mode = 0; // 0 = off, 1 = on, 2 = emulated
// 0: off - old mode, stores assets compressed, saves on disk space but requires decompression on every cache fetch
// 1: on - new mode, stores assets uncompressed, decompresses on first fetch using browser (native) and stores decompressed in cache, uses more disk space
// 2: emulated - new mode, stores assets uncompressed, decompresses on first fetch using wasm and stores decompressed in cache, uses more disk space
const mb_needed_for_fast_mode = 4096;
const min_mb_needed = 1024;
class N {
	constructor(a) { this.value = a }
};
class Q {
	constructor() { this.clear() };
	enqueue(value) {
		const node = new N(value);
		if (this.head) {
			this.tail.next = node;
			this.tail = node;
		} else {
			this.head = node;
			this.tail = node;
		}
		this.size++;
	}
	dequeue() {
		const current = this.head;
		if (!current) {
			return;
		}

		this.head = this.head.next;
		this.size--;
		return current.value;
	}
	clear() {
		this.head = undefined;
		this.tail = undefined;
		this.size = 0;
	}
	size() { return this.size; }
	*[Symbol.iterator]() {
		let current = this.head;

		while (current) {
			yield current.value;
			current = current.next;
		}
	}
};
function pLimit(concurrency) {
	if (!((Number.isInteger(concurrency) ||
		concurrency === Number.POSITIVE_INFINITY) &&
		concurrency > 0)) {
		throw new TypeError(
			'Expected `concurrency` to be a number from 1 and up');
	}
	const queue = new Q();
	let activeCount = 0;

	const next = () => {
		activeCount--;

		if (queue.size > 0) {
			queue.dequeue()();
		}
	};
	const run = async (fn, resolve, args) => {
		activeCount++;

		const result = (async () => fn(...args))();

		resolve(result);

		try {
			await result;
		} catch {
		}

		next();
	};
	const enqueue = (fn, resolve, args) => {
		queue.enqueue(run.bind(undefined, fn, resolve, args));
		(async () => {
			await Promise.resolve();
			if (activeCount < concurrency && queue.size > 0) {
				queue.dequeue()();
			}
		})();
	};
	const generator = (fn, ...args) => new Promise(resolve => { enqueue(fn, resolve, args); });
	Object.defineProperties(generator, {
		activeCount: {
			get: () => activeCount,
		},
		pendingCount: {
			get: () => queue.size,
		},
		clearQueue: {
			value: () => {
				queue.clear();
			},
		},
	});
	return generator;
}
const wndrVfsFetchConcurrency = (typeof process === 'object' && process.versions && process.versions.node)
	? 32
	: 200;
Module.limit = pLimit(wndrVfsFetchConcurrency);

// ── wndr IAS boot (M5) ──────────────────────────────────────────────────────────────────────────────────────────
// When the launcher marks a revision IAS-enabled it sets `Module.ias` (before FutureLands.js runs), e.g.:
//   Module.ias = {
//     cdn:        'https://cdn.simplystream.com',            // host group base; chunks live at <cdn>/chunks/<2hex>/<hash>.iopart
//     primaryCdn: 'https://play.example.com',                // optional fallback when Edge Boost is selected
//     tocUrl:     'https://cdn.simplystream.com/<rev>/content.uondemandtoc', // the on-demand TOC for this revision
//     bootsetUrl: 'https://simplystream.dev/ias/<proj><rev>/bootset',        // analysed boot package-id list (text), OPTIONAL
//     coalesce:   65536                                       // OPTIONAL range-coalescing gap (bytes); omit to disable
//   };
// We then (a) append the IAS engine args to Module.arguments, and (b) before main, fetch + register the TOC (and the
// boot set, if any) via the wndr_ias_register_bootstrap bridge so the synchronous boot mount + Install({PackageIds})
// run. STRICTLY a no-op when Module.ias is unset — the legacy VFS boot path is untouched.
function wndrIasPrimaryUrl(url) {
	const ias = (typeof Module !== 'undefined') ? Module.ias : null;
	if (!ias || !ias.cdn || !ias.primaryCdn || ias.primaryCdn === ias.cdn) { return ''; }
	try {
		const selectedBase = new URL(ias.cdn, location.href);
		const primaryBase = new URL(ias.primaryCdn, location.href);
		const requestUrl = new URL(url, location.href);
		if (requestUrl.origin !== selectedBase.origin) { return ''; }

		const selectedPath = selectedBase.pathname.replace(/\/+$/, '');
		let suffix = requestUrl.pathname;
		if (selectedPath && selectedPath !== '/' && suffix.startsWith(selectedPath + '/')) {
			suffix = suffix.slice(selectedPath.length);
		}

		const primaryPath = primaryBase.pathname.replace(/\/+$/, '');
		return primaryBase.origin + primaryPath + suffix + requestUrl.search;
	} catch (e) {
		return '';
	}
}

Module.wndrIasFetchWithFallback = async function (url, options) {
	// Honor the backend's HttpRetryCount budget (~2) on a single host. The engine never threads the count into the
	// transport, and on the locked single-host default the CDN failover below is a no-op (fallbackUrl === url) — so
	// without this the effective retry count is ZERO. Retry transient failures (network error / 5xx) on the SAME host
	// first, then fall over to the primary CDN. 4xx are not retried (a 404/403 will not fix itself).
	const MAX_ATTEMPTS = 3;
	const tryHost = async (u) => {
		let lastErr = null;
		for (let attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
			try {
				_wndr_ias_count_fetch();
				const resp = await fetch(u, options);
				if (resp.status === 200 || resp.status === 206 || resp.status < 500) {
					return { resp }; // success, or a non-retryable status — let the caller decide
				}
				lastErr = new Error('HTTP ' + resp.status);
			} catch (e) {
				lastErr = e;
			}
			if (attempt + 1 < MAX_ATTEMPTS) {
				await new Promise((r) => setTimeout(r, 50 * (attempt + 1))); // brief backoff before the same-host retry
			}
		}
		return { err: lastErr };
	};

	const primary = await tryHost(url);
	if (primary.resp && (primary.resp.status === 200 || primary.resp.status === 206)) {
		return primary.resp;
	}

	const fallbackUrl = wndrIasPrimaryUrl(url);
	if (fallbackUrl && fallbackUrl !== url) {
		console.warn('[wndr-ias] retrying primary CDN after Edge Boost miss', url, fallbackUrl);
		const fb = await tryHost(fallbackUrl);
		if (fb.resp) { return fb.resp; }
		if (primary.resp) { return primary.resp; }
		throw (fb.err || primary.err || new Error('[wndr-ias] fetch failed'));
	}

	if (primary.resp) { return primary.resp; } // return the non-2xx response so the dispatcher reports a real status
	throw (primary.err || new Error('[wndr-ias] fetch failed'));
};

async function wndrFetchBootstrapBytes(url, options, label) {
	// Bootstrap blobs compete with the wasm and other startup content for bandwidth. A
	// wall-clock deadline can discard a healthy partial download and prevent HTTP-cache
	// population. Let fetch report transport failures; those still use the retries below.
	let lastErr = null;
	let lastResponse = null;
	for (let attempt = 0; attempt < 3; ++attempt) {
		try {
			const resp = await fetch(url, options || {});
			if (!resp.ok) {
				if (resp.status < 500) { return { resp, bytes: null, attempts: attempt + 1 }; }
				lastResponse = resp;
				lastErr = new Error('HTTP ' + resp.status);
				console.error('[wndr-IAS] boot: bootstrap fetch attempt ' + attempt + ' failed for ' + label, lastErr);
			} else {
				const buf = await resp.arrayBuffer();
				return { resp, bytes: new Uint8Array(buf), attempts: attempt + 1 };
			}
		} catch (e) {
			lastErr = e;
			console.error('[wndr-IAS] boot: bootstrap fetch attempt ' + attempt + ' failed for ' + label, e);
		}
		if (attempt + 1 < 3) {
			await new Promise((r) => setTimeout(r, 100 * (attempt + 1)));
		}
	}
	if (lastResponse) { return { resp: lastResponse, bytes: null, attempts: 3 }; }
	throw (lastErr || new Error('[wndr-IAS] boot: bootstrap fetch failed for ' + label));
}

async function wndrFetchBootstrapBytesWithFallback(url, options, label) {
	let selectedResult = null;
	let selectedError = null;
	try {
		selectedResult = await wndrFetchBootstrapBytes(url, options, label);
		if (selectedResult.resp.ok) { return selectedResult; }
	} catch (e) {
		selectedError = e;
	}

	const fallbackUrl = wndrIasPrimaryUrl(url);
	if (fallbackUrl && fallbackUrl !== url) {
		console.warn('[wndr-IAS] boot: retrying primary CDN after bootstrap miss', label);
		try {
			return await wndrFetchBootstrapBytes(fallbackUrl, options, label + ' primary fallback');
		} catch (fallbackError) {
			if (!selectedResult) { throw fallbackError; }
		}
	}

	if (selectedResult) { return selectedResult; }
	throw (selectedError || new Error('[wndr-IAS] boot: bootstrap fetch failed for ' + label));
}

function wndrIasLocalePakInfo(blob) {
	if (/-LocaleCommon\.pak$/i.test(blob.name)) { return { kind: 'common', locale: null }; }
	const match = blob.name.match(/-Locale-([A-Za-z0-9-]+)\.pak$/i);
	return match ? { kind: 'locale', locale: match[1] } : null;
}

function wndrIasDeferredPathPakInfo(blob) {
	const match = blob.name.match(/-Deferred-([A-Za-z0-9-]+)\.pak$/i);
	return match ? { group: match[1] } : null;
}

function wndrIasBootShaderBlobInfo(blob) {
	const match = blob.name.match(/\/BootShaders-SimplyStream-(ES31|SM5)\.(utoc|ucas)$/i);
	return match ? { mode: match[1].toLowerCase(), extension: match[2].toLowerCase() } : null;
}

function wndrIasBootRangeBlobInfo(blob) {
	const match = blob.name.match(/\/BootRanges-SimplyStream-(ES31|SM5)\.iasbundle$/i);
	return match ? { mode: match[1].toLowerCase() } : null;
}

const WNDR_IAS_BOOT_RANGE_CACHE = 'wndr-ias-boot-ranges-v1';

function wndrIasBootRangeCacheLock(cacheUrl) {
	return 'wndr-ias-boot-range:' + cacheUrl;
}

async function wndrWithIasBootRangeCacheLock(cacheUrl, callback) {
	if (typeof navigator !== 'undefined' && navigator.locks?.request) {
		return navigator.locks.request(wndrIasBootRangeCacheLock(cacheUrl), callback);
	}
	return callback();
}

async function wndrInstallIasBootBundle(bytes, label, cacheUrl) {
	const view = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
	if (view.byteLength < 32) { throw new Error('IAS boot-range bundle is shorter than its header'); }
	const expectedMagic = [87, 78, 68, 82, 73, 65, 66, 49]; // WNDRIAB1
	for (let index = 0; index < expectedMagic.length; ++index) {
		if (view[index] !== expectedMagic[index]) { throw new Error('invalid IAS boot-range bundle magic'); }
	}
	const data = new DataView(view.buffer, view.byteOffset, view.byteLength);
	const entryCount = data.getUint32(8, true);
	const entrySize = data.getUint32(12, true);
	const payloadBase = data.getUint32(16, true);
	const payloadBytes = data.getUint32(20, true);
	const indexEnd = 32 + entryCount * entrySize;
	if (entrySize !== 32 || indexEnd > payloadBase || payloadBase + payloadBytes !== view.byteLength) {
		throw new Error('invalid IAS boot-range bundle layout');
	}
	const partitions = new Map();
	for (let entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
		const offset = 32 + entryIndex * entrySize;
		let partitionHash = '';
		for (let byteIndex = 0; byteIndex < 20; ++byteIndex) {
			partitionHash += view[offset + byteIndex].toString(16).padStart(2, '0');
		}
		const partitionOffset = data.getUint32(offset + 20, true);
		const diskSize = data.getUint32(offset + 24, true);
		const payloadOffset = data.getUint32(offset + 28, true);
		if (!diskSize || payloadOffset + diskSize > payloadBytes) {
			throw new Error('invalid IAS boot-range bundle entry ' + entryIndex);
		}
		if (!partitions.has(partitionHash)) { partitions.set(partitionHash, []); }
		partitions.get(partitionHash).push({ partitionOffset, diskSize, payloadOffset: payloadBase + payloadOffset });
	}
	for (const entries of partitions.values()) {
		entries.sort((left, right) => left.partitionOffset - right.partitionOffset);
	}
	Module.wndrIasBootRangeBundle = { bytes: view, partitions, label: label || '' };
	Module.wndrIasBootRangeHits = 0;
	if (cacheUrl && typeof caches !== 'undefined') {
		const persist = wndrWithIasBootRangeCacheLock(cacheUrl, async function () {
			const cache = await caches.open(WNDR_IAS_BOOT_RANGE_CACHE);
			if (!await cache.match(new Request(cacheUrl))) {
				await cache.put(new Request(cacheUrl), new Response(view));
			}
		});
		if (typeof navigator !== 'undefined' && navigator.locks?.request) {
			// The worker takes the same cross-context lock before reading. Let the engine initialize while Cache API
			// persists this large immutable object instead of extending the pre-main network gate.
			Module.wndrIasBootRangePersistPromise = persist;
			void persist.catch((error) => {
				console.warn('[wndr-IAS] boot: could not persist range bundle for IAS workers', error);
			});
		} else {
			try { await persist; }
			catch (error) {
				console.warn('[wndr-IAS] boot: could not persist range bundle for IAS workers', error);
			}
		}
	}
	console.log('[wndr-IAS] boot: installed ' + entryCount + '-chunk range bundle ' + (label || ''));
	return { entries: entryCount, bytes: payloadBytes, partitions: partitions.size };
}

Module.wndrEnsureIasBootRangeBundle = async function (cacheUrl) {
	if (!cacheUrl) { return false; }
	if (Module.wndrIasBootRangeBundle?.label === cacheUrl) { return true; }
	if (Module.wndrIasBootRangeLoadUrl !== cacheUrl) {
		Module.wndrIasBootRangeLoadUrl = cacheUrl;
		Module.wndrIasBootRangeLoadPromise = (async function () {
			try {
				let response = null;
				if (typeof caches !== 'undefined') {
					response = await wndrWithIasBootRangeCacheLock(cacheUrl, async function () {
						const cache = await caches.open(WNDR_IAS_BOOT_RANGE_CACHE);
						return cache.match(new Request(cacheUrl));
					});
				}
				let bundleBytes = response ? new Uint8Array(await response.arrayBuffer()) : null;
				if (!bundleBytes) {
					const fetched = await wndrFetchBootstrapBytesWithFallback(
						cacheUrl,
						{ cache: 'force-cache' },
						'worker boot-range bundle'
					);
					if (!fetched.resp.ok || !fetched.bytes) { return false; }
					bundleBytes = fetched.bytes;
				}
				await wndrInstallIasBootBundle(bundleBytes, cacheUrl, '');
				return true;
			} catch (error) {
				console.warn('[wndr-IAS] boot: IAS worker could not load range bundle', error);
				return false;
			}
		})();
	}
	return Module.wndrIasBootRangeLoadPromise;
};

Module.wndrIasBundleLookup = function (url, rangeMin, rangeLength) {
	const bundle = Module.wndrIasBootRangeBundle;
	const match = String(url || '').match(/\/([0-9a-f]{40})\.iopart(?:$|[?#])/i);
	const min = Number(rangeMin);
	const length = Number(rangeLength);
	if (!bundle || !match || !Number.isSafeInteger(min) || min < 0 || !Number.isSafeInteger(length) || length <= 0) { return null; }
	const end = min + length;
	if (!Number.isSafeInteger(end)) { return null; }
	for (const entry of bundle.partitions.get(match[1].toLowerCase()) || []) {
		const entryEnd = entry.partitionOffset + entry.diskSize;
		if (min >= entry.partitionOffset && end <= entryEnd) {
			const payloadMin = entry.payloadOffset + min - entry.partitionOffset;
			Module.wndrIasBootRangeHits++;
			if (Module.wndrIasBootRangeHits === 1) {
				console.log('[wndr-IAS] boot: first IAS range bundle hit from ' + bundle.label);
			}
			return bundle.bytes.subarray(payloadMin, payloadMin + length);
		}
	}
	return null;
};

function wndrCanonicalLocale(locale) {
	try { return Intl.getCanonicalLocales(String(locale))[0] || ''; }
	catch (e) { return String(locale || '').replaceAll('_', '-'); }
}

function wndrSelectIasLocale(supportedLocales) {
	if (!supportedLocales.length) { return ''; }
	const byLower = new Map(supportedLocales.map((locale) => [locale.toLowerCase(), locale]));
	const find = (locale) => byLower.get(String(locale || '').toLowerCase()) || '';
	const preferences = (typeof navigator !== 'undefined' && Array.isArray(navigator.languages) && navigator.languages.length)
		? navigator.languages
		: [(typeof navigator !== 'undefined' && navigator.language) || 'en'];

	for (const rawPreference of preferences) {
		const preference = wndrCanonicalLocale(rawPreference);
		const exact = find(preference);
		if (exact) { return exact; }

		const lower = preference.toLowerCase();
		if (lower.startsWith('zh-')) {
			const traditional = /-(tw|hk|mo)(-|$)|-hant(-|$)/i.test(lower);
			const chinese = find(traditional ? 'zh-Hant' : 'zh-Hans');
			if (chinese) { return chinese; }
		}
		if (/^es-(mx|gt|hn|sv|ni|cr|pa|do|pr|co|ve|ec|pe|bo|py|uy|ar|cl)(-|$)/i.test(lower)) {
			const latinAmericanSpanish = find('es-419');
			if (latinAmericanSpanish) { return latinAmericanSpanish; }
		}
		if (lower === 'pt' || lower.startsWith('pt-')) {
			const brazilianPortuguese = find('pt-BR');
			if (brazilianPortuguese) { return brazilianPortuguese; }
		}

		const language = lower.split('-')[0];
		const languageMatch = find(language)
			|| supportedLocales.find((locale) => locale.toLowerCase().startsWith(language + '-'));
		if (languageMatch) { return languageMatch; }
	}

	return find('en') || [...supportedLocales].sort()[0];
}

function wndrExplicitWebGpuMode() {
	for (const argument of (Module.arguments || [])) {
		const match = String(argument).match(/r\.WebGPU\.ForcedMode\s*=\s*([12])/i);
		if (match) { return match[1] === '2' ? 'sm5' : 'es31'; }
	}
	return '';
}

async function wndrRequestWebGpuAdapter() {
	const xrCompatible = !!Module.webxrEnabled;
	if (!Module.wndrWebGpuAdapterPromise || Module.wndrWebGpuAdapterXrCompatible !== xrCompatible) {
		Module.wndrWebGpuAdapterXrCompatible = xrCompatible;
		Module.wndrWebGpuAdapterPromise = navigator.gpu.requestAdapter(xrCompatible ? { xrCompatible: true } : undefined);
	}
	const promise = Module.wndrWebGpuAdapterPromise;
	try {
		const adapter = await promise;
		if (!adapter && Module.wndrWebGpuAdapterPromise === promise) {
			Module.wndrWebGpuAdapterPromise = null;
		}
		return adapter;
	} catch (e) {
		if (Module.wndrWebGpuAdapterPromise === promise) {
			Module.wndrWebGpuAdapterPromise = null;
		}
		throw e;
	}
}

async function wndrSelectBootShaderMode(availableModes) {
	if (!availableModes.length) { return ''; }
	const clamp = (mode) => availableModes.includes(mode)
		? mode
		: (availableModes.includes('es31') ? 'es31' : availableModes[0]);
	const explicitMode = wndrExplicitWebGpuMode();
	if (explicitMode) { return clamp(explicitMode); }

	try {
		const parentState = (typeof window !== 'undefined' && window.parent && window.parent.state) || {};
		if (parentState.feature_level_mode === 'sm5' || parentState.feature_level_mode === 'es31') {
			return clamp(parentState.feature_level_mode);
		}
	} catch (e) { /* standalone */ }

	try {
		const adapter = await wndrRequestWebGpuAdapter();
		if (adapter) {
			const features = new Set(adapter.features);
			const result = classifyTier(adapter.limits, features);
			const tierCode = TIER_ORDER.indexOf(result.tier);
			return clamp(tierCode >= 0 && tierCode <= 1 ? 'sm5' : 'es31');
		}
	} catch (e) {
		console.warn('[wndr-IAS] boot: WebGPU mode probe failed; using the compatibility shader bootstrap', e);
	}
	return clamp('es31');
}

(function wndrIasBoot() {
	const ias = (typeof Module !== 'undefined') ? Module.ias : null;
	if (!ias || !ias.cdn || !ias.tocUrl) { return; }
	const iasArgs = ['-Ias', '-Iad', '-iax.PackageStreamingEnabled', '-WndrIasMount=' + ias.cdn, '-Ias.NoCache'];
	if (ias.coalesce)   { iasArgs.push('-WndrIasCoalesce=' + (ias.coalesce | 0)); }
	if (ias.extentBytes) { iasArgs.push('-WndrIasExtentBytes=' + (ias.extentBytes | 0)); }
	if (ias.bootsetUrl) { iasArgs.push('-WndrIasPrefetchFile=wndr_bootset.txt'); }
	// wndr IAS: bootShaderBlobs is the established config field for additional early bootstrap objects. The standard
	// IoStore global container now uses it too; its ScriptObjects chunk must be mounted before any caller caches
	// FIoDispatcherInternal::HasPackageData(). The field name is retained for backend compatibility.
	const configuredBootBlobs = Array.isArray(ias.bootShaderBlobs)
		? ias.bootShaderBlobs.filter((blob) => blob && typeof blob.name === 'string' && blob.name && typeof blob.url === 'string' && blob.url)
		: [];
	const deferredPathPakBlobs = configuredBootBlobs.filter((blob) => wndrIasDeferredPathPakInfo(blob));
	const configuredDeferredPathPakDelayMs = Number(ias.deferredPathPakDelayMs ?? 30000);
	const deferredPathPakDelayMs = Number.isFinite(configuredDeferredPathPakDelayMs)
		? Math.max(0, configuredDeferredPathPakDelayMs)
		: 30000;
	const localePakBlobs = configuredBootBlobs
		.map((blob) => ({ blob, info: wndrIasLocalePakInfo(blob) }))
		.filter((entry) => entry.info);
	const bootShaderBlobEntries = configuredBootBlobs
		.map((blob) => ({ blob, info: wndrIasBootShaderBlobInfo(blob) }))
		.filter((entry) => entry.info);
	const bootRangeBlobEntries = configuredBootBlobs
		.map((blob) => ({ blob, info: wndrIasBootRangeBlobInfo(blob) }))
		.filter((entry) => entry.info);
	const regularBootBlobs = configuredBootBlobs.filter((blob) =>
		!wndrIasLocalePakInfo(blob) && !wndrIasBootShaderBlobInfo(blob) && !wndrIasBootRangeBlobInfo(blob) && !wndrIasDeferredPathPakInfo(blob));

	const bootShaderModes = new Map();
	for (const entry of bootShaderBlobEntries) {
		if (!bootShaderModes.has(entry.info.mode)) bootShaderModes.set(entry.info.mode, {});
		bootShaderModes.get(entry.info.mode)[entry.info.extension] = entry.blob;
	}
	for (const [mode, pair] of [...bootShaderModes]) {
		if (!pair.utoc || !pair.ucas) {
			console.error('[wndr-IAS] boot: incomplete ' + mode + ' shader bootstrap; both .utoc and .ucas are required');
			bootShaderModes.delete(mode);
		}
	}
	const bootRangeModes = new Map(bootRangeBlobEntries.map((entry) => [entry.info.mode, entry.blob]));
	const availableBootModes = [...(bootShaderModes.size ? bootShaderModes.keys() : bootRangeModes.keys())].sort();
	// Explicit/launcher-selected modes need no main-context GPUAdapter. The RHI runs in a worker and cannot reuse
	// that object, so probe an adapter only when mode selection genuinely needs device capabilities.
	const bootModePromise = wndrSelectBootShaderMode(availableBootModes);
	const selectedModeBlobCount = availableBootModes.reduce((maximum, mode) =>
		Math.max(maximum, (bootShaderModes.has(mode) ? 2 : 0) + (bootRangeModes.has(mode) ? 1 : 0)), 0);

	const localePaks = localePakBlobs.filter((entry) => entry.info.kind === 'locale');
	const commonLocalePak = localePakBlobs.find((entry) => entry.info.kind === 'common')?.blob || null;
	const selectedLocale = wndrSelectIasLocale(localePaks.map((entry) => entry.info.locale));
	const selectedLocalePak = localePaks.find((entry) => entry.info.locale === selectedLocale)?.blob || null;
	const globalTocBlob = configuredBootBlobs.find((blob) => blob.name === 'Paks/global.utoc');
	const globalCasBlob = configuredBootBlobs.find((blob) => blob.name === 'Paks/global.ucas');
	const haveGlobalContainer = !!(globalTocBlob && globalCasBlob);
	if (haveGlobalContainer) { iasArgs.push('-WndrIasGlobalContainer=Paks/global.utoc'); }
	else if (globalTocBlob || globalCasBlob) {
		console.error('[wndr-IAS] boot: incomplete global container config; both Paks/global.utoc and Paks/global.ucas are required');
	}
	// wndr IAS (BootSet P4 fix): when a plugin-owned BootSet boot-container was deployed (bootTocUrl/bootCasUrl),
	// enable its synchronous mount at PreRHIInit. The .utoc/.ucas bytes are delivered as bootstrap blobs below,
	// named to suffix-match the plugin's ProjectContentDir-relative OpenRead.
	const haveBootSet = !!(ias.bootTocUrl && ias.bootCasUrl);
	if (haveBootSet) { iasArgs.push('-WndrIasBootContainer=Paks/BootSet-SimplyStream.utoc'); }
	// wndr IAS (path-pak): the cooked core .pak and selected locale companions are delivered as EARLY bootstrap blobs;
	// -WndrIasPathPaks names them (ProjectContentDir-relative) so SimplyStreamPathPak.cpp mounts them before config/ICU/
	// Slate/plugins/GSC path reads. Old singular revisions keep -WndrIasPathPak compatibility.
	const havePathPak = !!(ias.pathPakUrl && ias.pathPakName);
	const selectedPathPakNames = havePathPak ? [ias.pathPakName] : [];
	if (havePathPak && commonLocalePak) selectedPathPakNames.push(commonLocalePak.name);
	if (havePathPak && selectedLocalePak) selectedPathPakNames.push(selectedLocalePak.name);
	if (selectedPathPakNames.length > 1) {
		iasArgs.push('-WndrIasPathPaks=' + selectedPathPakNames.join(','));
		if (selectedLocale) { iasArgs.push('-culture=' + selectedLocale); }
	} else if (havePathPak) {
		iasArgs.push('-WndrIasPathPak=' + ias.pathPakName);
	}
	if (havePathPak && deferredPathPakBlobs.length) {
		iasArgs.push('-WndrIasDeferredPathPaks=' + deferredPathPakBlobs.map((blob) => blob.name).join(','));
	}
	// wndr IAS (shaders-in-pak): the cooked shader archives ship as one ShaderPak mounted alongside the .pak, so
	// FShaderCodeLibrary's FindFiles enumeration finds them. -WndrIasShaderPak names it (ProjectContentDir-relative).
	const haveShaderPak = !!(ias.shaderPakUrl && ias.shaderPakName);
	if (haveShaderPak) { iasArgs.push('-WndrIasShaderPak=' + ias.shaderPakName); }
	Module.arguments = (Module.arguments || []).concat(iasArgs);

	// Pass the launcher's projectId to the wasm command line so OnlineSubsystemSimplyStream::AutoLogin can
	// guest-login (Lyra's CommonUser v1 drives login via IOnlineIdentity::AutoLogin, not Login-with-credentials).
	// Without a projectId the guest-login POST /api/v1/multiplayer/auth/guest has no body -> "Failed to Request Login".
	try {
		const ssProjectId = (typeof window !== 'undefined' && window.parent && window.parent.state && window.parent.state.project_id) || '';
		if (ssProjectId) { Module.arguments = Module.arguments.concat(['-SimplyStreamProjectId=' + ssProjectId]); }
	} catch (e) { /* standalone (no parent launcher state) */ }

	// wndr IAS (bootstrap delivery): global mounts at FileSystemReady; BootSet and the selected shader container mount
	// at PreRHIInit. They are consumed before the late on-demand TOC and MUST be registered in the bootstrap store
	// before wasm main(). Start these fetches immediately, then use a preRun Emscripten dependency to block doRun
	// until they finish and register them SYNCHRONOUSLY at the top of onRuntimeInitialized. If the
	// run-dependency API is unavailable, the onRuntimeInitialized path fetches them alongside the TOC (a timing
	// fallback: WASM PreInit is slow enough this usually wins, but it is not guaranteed).
	const earlyBlobs = []; // {name, url}
	for (const blob of regularBootBlobs) {
		earlyBlobs.push({ name: blob.name, url: blob.url });
	}
	if (haveBootSet) {
		earlyBlobs.push({ name: 'Paks/BootSet-SimplyStream.utoc', url: ias.bootTocUrl });
		earlyBlobs.push({ name: 'Paks/BootSet-SimplyStream.ucas', url: ias.bootCasUrl });
	}
	// wndr IAS (path-pak): core and selected locale paks are registered before main so the engine's lazy
	// FPakPlatformFile mount can read their bytes synchronously from the bootstrap store.
	if (havePathPak) {
		earlyBlobs.push({ name: ias.pathPakName, url: ias.pathPakUrl });
	}
	if (havePathPak && commonLocalePak) {
		earlyBlobs.push({ name: commonLocalePak.name, url: commonLocalePak.url });
	}
	if (havePathPak && selectedLocalePak) {
		earlyBlobs.push({ name: selectedLocalePak.name, url: selectedLocalePak.url });
	}
	// wndr IAS (shaders-in-pak): fetch the ShaderPak as an early blob so it's in the bootstrap store before main() and
	// the lazy mount reads + enumerates the shader archives from it (replaces the per-archive bootShaderBlobs).
	if (haveShaderPak) {
		earlyBlobs.push({ name: ias.shaderPakName, url: ias.shaderPakUrl });
	}

	// Start all mandatory network work while Emscripten is still fetching/compiling the wasm. Previously these
	// requests started in preRun and were awaited one by one, putting the entire BootSet + PathPak transfer after
	// wasm instantiation. The run dependency below still prevents main() until every required early blob has settled.
	const nowMs = () => (typeof performance !== 'undefined' && typeof performance.now === 'function')
		? performance.now()
		: Date.now();
	const bootStats = Module.wndrIasBootStats = {
		startedAtMs: nowMs(),
		earlyBlobCount: earlyBlobs.length + selectedModeBlobCount,
		earlyBlobs: [],
		selectedLocale: selectedLocale || null,
		availableShaderModes: [...bootShaderModes.keys()].sort(),
		availableBootRangeModes: [...bootRangeModes.keys()].sort(),
		deferredPathPakCount: deferredPathPakBlobs.length,
	};
	try { performance.mark('wndr-ias-bootstrap-start'); } catch (e) { /* Performance API unavailable */ }

	const fetchEarlyBlob = async function (blob) {
		const startedAtMs = nowMs();
		try {
			const result = await wndrFetchBootstrapBytesWithFallback(
				blob.url,
				{ cache: 'force-cache' },
				'early-blob ' + blob.name
			);
			const stat = {
				name: blob.name,
				bytes: result.bytes ? result.bytes.byteLength : 0,
				durationMs: Math.round(nowMs() - startedAtMs),
				status: result.resp.status,
				attempts: result.attempts,
			};
			bootStats.earlyBlobs.push(stat);
			if (result.resp.ok && result.bytes) {
				if (blob.installIasBundle) {
					await wndrInstallIasBootBundle(result.bytes, blob.url, blob.url);
					return { name: blob.name, bytes: result.bytes, bootstrap: false };
				}
				return { name: blob.name, bytes: result.bytes, bootstrap: true };
			}
			console.error('[wndr-IAS] boot: early-blob fetch HTTP ' + result.resp.status + ' for ' + blob.name);
		} catch (e) {
			bootStats.earlyBlobs.push({
				name: blob.name,
				bytes: 0,
				durationMs: Math.round(nowMs() - startedAtMs),
				status: 0,
			});
			console.error('[wndr-IAS] boot: early-blob fetch error for ' + blob.name, e);
		}
		return null;
	};
	const fetchDeferredPathPaks = async function () {
		if (!deferredPathPakBlobs.length) { return; }
		const startedAtMs = nowMs();
		const entries = (await Promise.all(deferredPathPakBlobs.map(async function (blob) {
			try {
				const result = await wndrFetchBootstrapBytesWithFallback(
					blob.url,
					{ cache: 'force-cache' },
					'deferred-path-pak ' + blob.name
				);
				if (result.resp.ok && result.bytes) { return { name: blob.name, bytes: result.bytes }; }
				console.warn('[wndr-IAS] deferred PathPak fetch HTTP ' + result.resp.status + ' for ' + blob.name);
			} catch (e) {
				console.warn('[wndr-IAS] deferred PathPak fetch failed for ' + blob.name, e);
			}
			return null;
		}))).filter(Boolean);
		if (!entries.length) { return; }
		globalThis.__wndr_ias_bootstrap = entries;
		try {
			Module._wndr_ias_pull_bootstrap();
			if (typeof Module._wndr_ias_path_paks_ready === 'function') {
				Module._wndr_ias_path_paks_ready();
			}
		} finally {
			globalThis.__wndr_ias_bootstrap = null;
		}
		bootStats.deferredPathPakBytes = entries.reduce((total, entry) => total + entry.bytes.byteLength, 0);
		bootStats.deferredPathPakDurationMs = Math.round(nowMs() - startedAtMs);
		console.log('[wndr-IAS] registered ' + entries.length + '/' + deferredPathPakBlobs.length
			+ ' deferred PathPak(s), ' + bootStats.deferredPathPakBytes + ' B after startup delay');
	};
	if (deferredPathPakBlobs.length && typeof globalThis.addEventListener === 'function') {
		globalThis.addEventListener('simplystream-webgpu-presented-frame', function () {
			setTimeout(function () { void fetchDeferredPathPaks(); }, deferredPathPakDelayMs);
		}, { once: true });
	}
	const regularEarlyFetchPromise = Promise.all(earlyBlobs.map(fetchEarlyBlob));
	const modeEarlyFetchPromise = availableBootModes.length
		? bootModePromise.then(async function (mode) {
			const pair = bootShaderModes.get(mode);
			bootStats.selectedMode = mode;
			if (pair) {
				bootStats.selectedShaderMode = mode;
				Module.arguments.push('-WndrIasBootShaderContainer=' + pair.utoc.name);
			}
			const forcedMode = mode === 'sm5' ? 2 : 1;
			if (wndrExplicitWebGpuMode() !== mode) {
				Module.arguments.push('-dpcvars=r.WebGPU.ForcedMode=' + forcedMode);
			}
			console.log('[wndr-IAS] boot: selected ' + mode + ' startup payloads for this adapter');
			const selected = [];
			if (pair) {
				selected.push(
					fetchEarlyBlob({ name: pair.utoc.name, url: pair.utoc.url }),
					fetchEarlyBlob({ name: pair.ucas.name, url: pair.ucas.url }),
				);
			}
			const rangeBundle = bootRangeModes.get(mode);
			if (rangeBundle) {
				bootStats.selectedBootRangeMode = mode;
				Module.arguments.push('-WndrIasBootRangeUrl=' + rangeBundle.url);
				selected.push(fetchEarlyBlob({ name: rangeBundle.name, url: rangeBundle.url, installIasBundle: true }));
			}
			return Promise.all(selected);
		})
		: Promise.resolve([]);
	const earlyFetchPromise = Promise.all([regularEarlyFetchPromise, modeEarlyFetchPromise]).then(function (groups) {
		const out = groups.flat().filter(Boolean);
		bootStats.earlyReadyAtMs = nowMs();
		bootStats.earlyDurationMs = Math.round(bootStats.earlyReadyAtMs - bootStats.startedAtMs);
		bootStats.earlyBytes = out.reduce((total, blob) => total + blob.bytes.byteLength, 0);
		try {
			performance.mark('wndr-ias-bootstrap-ready');
			performance.measure('wndr-ias-bootstrap-fetch', 'wndr-ias-bootstrap-start', 'wndr-ias-bootstrap-ready');
		} catch (e) { /* Performance API unavailable */ }
		console.log('[wndr-IAS] boot: prefetched ' + out.length + '/' + bootStats.earlyBlobCount
			+ ' early blob(s), ' + bootStats.earlyBytes + ' B in ' + bootStats.earlyDurationMs
			+ ' ms (overlapped with wasm startup)');
		for (const blob of out) {
			if (blob.bootstrap === false) { blob.bytes = null; }
		}
		return out;
	});

	// Start the TOC and optional package-prefetch list alongside the other bootstrap requests. The TOC is now a
	// main() gate because the runtime mounts it at PreRHIInit before opening shader libraries.
	const tocFetchPromise = wndrFetchBootstrapBytesWithFallback(
		ias.tocUrl,
		{ cache: 'force-cache' },
		'on-demand TOC'
	).then((result) => ({ result }), (error) => ({ error }));
	const bootsetFetchPromise = ias.bootsetUrl
		? wndrFetchBootstrapBytesWithFallback(
			ias.bootsetUrl,
			{ cache: 'no-store' },
			'bootset'
			).then((result) => ({ result }), (error) => ({ error }))
		: Promise.resolve(null);

	const preMainFetchPromise = Promise.all([earlyFetchPromise, tocFetchPromise, bootsetFetchPromise]);
	Module.preRun = (Module.preRun || []);
	Module.preRun.push(function () {
		// addRunDependency is defined before run() executes preRun. If it is absent, onRuntimeInitialized keeps the
		// old best-effort fallback instead of hanging main on an unavailable gate API.
		if (typeof addRunDependency !== 'function' || typeof removeRunDependency !== 'function') { return; }
		addRunDependency('wndr-ias-pre-main');
		preMainFetchPromise
			.then(([early, toc, bootset]) => {
				globalThis.__wndr_ias_pre_main = { early, toc, bootset };
			})
			.finally(() => { removeRunDependency('wndr-ias-pre-main'); });
	});

	const prevOnInit = Module.onRuntimeInitialized;
	Module.onRuntimeInitialized = async function () {
		let earlyRegistered = false;
		let tocRegistered = false;
		let bootsetHandled = false;
		// Consume the run-dependency result synchronously, before the first await. Emscripten does not await a Promise
		// returned by onRuntimeInitialized, so deferring this registration would let main reach PreRHI without the TOC.
		try {
			const preMain = globalThis.__wndr_ias_pre_main;
			if (preMain) {
				const entries = (preMain.early || []).filter((blob) => blob.bootstrap !== false);
				earlyRegistered = true;
				const tocPrefetch = preMain.toc;
				if (tocPrefetch?.error) { throw tocPrefetch.error; }
				const tocResult = tocPrefetch?.result;
				if (!tocResult?.resp.ok || !tocResult.bytes) {
					throw new Error('[wndr-IAS] TOC fetch failed: HTTP ' + (tocResult?.resp.status || 0) + ' for ' + ias.tocUrl);
				}
				const tocBytes = tocResult.bytes;
				entries.push(
					{ name: 'Paks/global.uondemandtoc', bytes: tocBytes },
					{ name: 'Paks/content.uondemandtoc', bytes: tocBytes }
				);
				tocRegistered = true;
				bootStats.tocReadyAtMs = nowMs();
				bootStats.tocBytes = tocBytes.byteLength;

				if (ias.bootsetUrl) {
					bootsetHandled = true;
					if (preMain.bootset?.result?.resp.ok && preMain.bootset.result.bytes) {
						entries.push({ name: 'wndr_bootset.txt', bytes: preMain.bootset.result.bytes });
					} else if (preMain.bootset?.error) {
						console.warn('[wndr-IAS] boot: boot-set fetch error — booting without prefetch', preMain.bootset.error);
					} else {
						console.warn('[wndr-IAS] boot: boot-set fetch failed HTTP '
							+ (preMain.bootset?.result?.resp.status || 0) + ' — booting without prefetch');
					}
				}

				globalThis.__wndr_ias_bootstrap = entries;
				Module._wndr_ias_pull_bootstrap();
				globalThis.__wndr_ias_bootstrap = null;
				globalThis.__wndr_ias_pre_main = null;
				Module.wndrIasBootRangeBundle = null;
				console.log('[wndr-IAS] boot: registered TOC + ' + entries.length
					+ ' early IAS bootstrap blob(s) synchronously before main');
			}
		} catch (e) {
			earlyRegistered = false;
			tocRegistered = false;
			bootsetHandled = false;
			console.error('[wndr-IAS] boot: synchronous pre-main registration failed', e);
		}
		try {
			const entries = [];
			if (!tocRegistered) {
				const tocPrefetch = await tocFetchPromise;
				if (tocPrefetch.error) { throw tocPrefetch.error; }
				const tocResult = tocPrefetch.result;
				if (!tocResult.resp.ok || !tocResult.bytes) {
					// A non-2xx body must not be force-cached and parsed as a TOC.
					throw new Error('[wndr-IAS] TOC fetch failed: HTTP ' + tocResult.resp.status + ' for ' + ias.tocUrl);
				}
				const tocBytes = tocResult.bytes;
				bootStats.tocReadyAtMs = nowMs();
				bootStats.tocBytes = tocBytes.byteLength;
				entries.push(
					{ name: 'Paks/global.uondemandtoc', bytes: tocBytes },
					{ name: 'Paks/content.uondemandtoc', bytes: tocBytes }
				);
			}
			if (ias.bootsetUrl && !bootsetHandled) {
				// Boot set is a prefetch optimization — degrade gracefully (boot without it) on any failure.
				const bootsetPrefetch = await bootsetFetchPromise;
				if (bootsetPrefetch?.result?.resp.ok && bootsetPrefetch.result.bytes) {
					entries.push({ name: 'wndr_bootset.txt', bytes: bootsetPrefetch.result.bytes });
				} else if (bootsetPrefetch?.error) {
					console.warn('[wndr-IAS] boot: boot-set fetch error — booting without prefetch', bootsetPrefetch.error);
				} else {
					console.warn('[wndr-IAS] boot: boot-set fetch failed HTTP '
						+ (bootsetPrefetch?.result?.resp.status || 0) + ' — booting without prefetch');
				}
			}
			if (!earlyRegistered) {
				entries.push(...(await earlyFetchPromise).filter((blob) => blob.bootstrap !== false));
			}
			if (entries.length) {
				globalThis.__wndr_ias_bootstrap = entries;
				Module._wndr_ias_pull_bootstrap();
				globalThis.__wndr_ias_bootstrap = null;
				console.log('[wndr-IAS] boot: registered ' + entries.length + ' fallback bootstrap entrie(s)');
			}
		} catch (e) {
			console.error('[wndr-IAS] boot: TOC registration failed', e);
		}
		Module.wndrIasBootRangeBundle = null;
		if (prevOnInit) { return prevOnInit.apply(this, arguments); }
	};
})();
function wonder_num_logical_cores() {
	if (ENVIRONMENT_IS_NODE) return require('os').cpus().length;
	return navigator['hardwareConcurrency'];
}
function wndrVfsAssetUrl(hash) {
	const baseUrl = (Module.assetRouting && Module.assetRouting.vfsAssetBaseUrl)
		|| Module.vfs_asset_base_url
		|| (ENVIRONMENT_IS_NODE
			? (process.env.SIMPLYSTREAM_VFS_ASSET_BASE_URL
				|| 'https://simplystream.com/public/v1/runtime-assets')
			: '');
	if (!baseUrl) { return ''; }
	const key = hash.toString();
	const a = key.substring(0, key.length / 2);
	const b = key.substring(key.length / 2);
	return `${baseUrl.replace(/\/+$/, '')}/${a}/${b}`;
}
function wndrPrimaryVfsAssetUrl(hash, mode) {
	const prefix = mode === 'fetch' ? 'fetch/asset' : 'asset';
	return `https://${Module.platform_addr}/${prefix}/${Module.session_id}/${hash}`;
}

// wndrTimedFetch: fetch() has NO timeout — a stalled response body neither resolves nor
// rejects, and every VFS/asset waiter above this sits in an untimed wait (observed: game
// thread parked in View::wait_for_ready forever -> silent whole-game freeze on flaky
// connections). Bound every attempt and retry twice before letting the error propagate.
async function wndrTimedFetch(url, options, label) {
	let lastErr = null;
	for (let attempt = 0; attempt < 3; attempt++) {
		const ctl = new AbortController();
		const tmo = setTimeout(() => ctl.abort(), attempt === 0 ? 20000 : 30000);
		try {
			return await fetch(url, Object.assign({}, options || {}, { signal: ctl.signal }));
		} catch (e) {
			lastErr = e;
			console.error('[wndr-fetch]', label || '', 'attempt', attempt, 'failed:', url, e);
		} finally { clearTimeout(tmo); }
	}
	throw lastErr;
}


// [WndrHeartbeat] game-thread stall monitor (2026-07-04). Field freezes park the game
// thread in an UNTIMED wait with zero log output; the MAIN thread provably survives
// every freeze. Poll the exported frame counter; if it stops advancing while the page
// is visible, read the block-site probe (wndr_probe.h) and name the wedge in the
// console — the one log line every freeze report needs.
(function () {
	let lastFrame = -1;
	let stalledTicks = 0;
	let reportedAt = -1;
	// MUST NOT touch exports before wasm init: emscripten assertion builds ABORT the whole
	// runtime on a pre-init export call, and the stub exists so existence checks pass.
	// The ONLY airtight gate is our own flag set from onRuntimeInitialized (chained — the
	// IAS bootstrap above also chains it; assignment order in this file makes us the outer
	// wrapper).
	const prevInit = Module.onRuntimeInitialized;
	Module.onRuntimeInitialized = function () {
		Module.__wndrRuntimeReady = true;
		if (prevInit) { return prevInit.apply(this, arguments); }
	};
	setInterval(function () {
		try {
			if (!Module || Module.__wndrRuntimeReady !== true || typeof Module._wndr_game_frame !== 'function') { return; }
			const f = Module._wndr_game_frame();
			if (f > 0 && f === lastFrame && !document.hidden) {
				stalledTicks++;
				if (stalledTicks >= 3 && typeof Module._wndr_kick_named_threads === 'function') {
					// Lost-wakeup self-heal: wake the named task queues. Confirmed root-cause
					// candidate from the 2026-07-05 trace (fence task undelivered to a sleeping
					// render thread). Kick EVERY tick while stalled — Trigger is idempotent.
					Module._wndr_kick_named_threads();
					console.error('[WndrHeartbeat] kicked named thread queues (lost-wakeup self-heal attempt)');
				}
				if (stalledTicks >= 3 && reportedAt !== f) { // ~15s frozen
					reportedAt = f;
					const site = Module._wndr_probe_site ? Module._wndr_probe_site() : -1;
					const names = { 0: 'unknown/other', 1: 'WebGPU waitOnPipeline', 2: 'wgpu proxy push_and_wait', 3: 'fsQueue JS-bridge waitOnId', 4: 'VFS wait_for_ready', 5: 'INFINITE FEvent::Wait (frame-end sync / render fence / task wait)', 6: 'ParkingLot wait (FMutex / FEventCount / task wait)', 8: 'MAIN_THREAD_EM_ASM sync proxy (browser main-thread queue)', 9: 'emscripten websocket send (internal main-thread proxy)' };
					const netStage = Module._wndr_net_travel_stage ? Module._wndr_net_travel_stage() : -1;
					const netStageNames = {
						0: 'none',
						10: 'OSS JoinSession start',
						11: 'OSS JoinSession duplicate',
						12: 'OSS JoinSession endpoint ready',
						13: 'OSS JoinSession no endpoint',
						14: 'OSS JoinSession success delegate',
						15: 'OSS JoinSession failure delegate',
						16: 'OSS resolve connect string',
						17: 'OSS resolve connect string failed',
						20: 'OSS RegisterPlayers start',
						21: 'OSS RegisterPlayers duplicate',
						22: 'OSS RegisterPlayers complete',
						30: 'OSS UnregisterPlayers start',
						31: 'OSS UnregisterPlayers suppressed: fallback grace',
						32: 'OSS UnregisterPlayers suppressed: pending travel',
						33: 'OSS UnregisterPlayers HTTP leave',
						40: 'WebTransport InitConnect',
						41: 'WebTransport client open',
						42: 'WebTransport fallback scheduled',
						43: 'WebTransport network-failure retry',
						49: 'WebTransport destroy',
						50: 'PostLoadMap'
					};
					const presentStageNames = {
						0: 'idle',
						1: 'begin',
						10: 'wait blit pipeline',
						20: 'get current texture',
						30: 'record blit',
						40: 'flush submission',
						50: 'queue endOfFrame',
						60: 'wait FrameReady',
						70: 'platform present',
						80: 'done'
					};
					const tickPhaseNames = {
						0: 'idle',
						10: 'engine tick',
						20: 'platform tick',
						21: 'do events',
						22: 'snapshot',
						23: 'session events',
						30: 'schedule next'
					};
					const rf = (typeof Module._wndr_render_frame === 'function') ? Module._wndr_render_frame() : -1;
					const qd = (typeof Module._wndr_wgpu_qdepth === 'function') ? Module._wndr_wgpu_qdepth() : -1;
					const pf = (typeof window !== 'undefined' && window.__simplystreamWebGPUPresentedFrame) || -1;
					const wq = (typeof Module._wndr_wgpu_proxy_queued === 'function') ? Module._wndr_wgpu_proxy_queued() : -1;
					const ws = (typeof Module._wndr_wgpu_proxy_started === 'function') ? Module._wndr_wgpu_proxy_started() : -1;
					const wf = (typeof Module._wndr_wgpu_proxy_finished === 'function') ? Module._wndr_wgpu_proxy_finished() : -1;
					const wlq = (typeof Module._wndr_wgpu_proxy_last_queued_id === 'function') ? Module._wndr_wgpu_proxy_last_queued_id() : -1;
					const wc = (typeof Module._wndr_wgpu_proxy_current_id === 'function') ? Module._wndr_wgpu_proxy_current_id() : -1;
					const wlf = (typeof Module._wndr_wgpu_proxy_last_finished_id === 'function') ? Module._wndr_wgpu_proxy_last_finished_id() : -1;
					const submitCalls = (typeof Module._wndr_wgpu_submit_calls === 'function') ? Module._wndr_wgpu_submit_calls() : -1;
					const submitDone = (typeof Module._wndr_wgpu_submit_done === 'function') ? Module._wndr_wgpu_submit_done() : -1;
					const submitCmds = (typeof Module._wndr_wgpu_submit_cmd_buffers === 'function') ? Module._wndr_wgpu_submit_cmd_buffers() : -1;
					const submitStatus = (typeof Module._wndr_wgpu_submit_status === 'function') ? Module._wndr_wgpu_submit_status() : -1;
					const presentBegins = (typeof Module._wndr_wgpu_present_begins === 'function') ? Module._wndr_wgpu_present_begins() : -1;
					const presentEnds = (typeof Module._wndr_wgpu_present_ends === 'function') ? Module._wndr_wgpu_present_ends() : -1;
					const presentStage = (typeof Module._wndr_wgpu_present_stage === 'function') ? Module._wndr_wgpu_present_stage() : -1;
					const presentLastStage = (typeof Module._wndr_wgpu_present_last_stage === 'function') ? Module._wndr_wgpu_present_last_stage() : -1;
					const tickPhase = (typeof Module._wndr_game_tick_phase === 'function') ? Module._wndr_game_tick_phase() : -1;
					const tickEntered = (typeof Module._wndr_game_tick_entered === 'function') ? Module._wndr_game_tick_entered() : -1;
					const tickEngineDone = (typeof Module._wndr_game_tick_engine_done === 'function') ? Module._wndr_game_tick_engine_done() : -1;
					const tickPlatformDone = (typeof Module._wndr_game_tick_platform_done === 'function') ? Module._wndr_game_tick_platform_done() : -1;
					const frameSyncs = (typeof Module._wndr_browser_frame_syncs === 'function') ? Module._wndr_browser_frame_syncs() : -1;
					const xrFrameSyncs = (typeof Module._wndr_browser_xr_frame_syncs === 'function') ? Module._wndr_browser_xr_frame_syncs() : -1;
					if (typeof Module._wndr_wgpu_log_diag === 'function') {
						Module._wndr_wgpu_log_diag();
					}
					console.error('[WndrHeartbeat] GAME THREAD STALLED ~' + (stalledTicks * 5) + 's at frame ' + f
						+ ' — blockSite=' + site + ' (' + (names[site] || '?') + ')'
						+ ' netStage=' + netStage + ' (' + (netStageNames[netStage] || '?') + ')'
						+ ' gameFrame=' + f + ' renderFrame=' + rf + ' presentedFrame=' + pf + ' wgpuQueueDepth=' + qd
						+ ' wgpuProxy=' + wq + '/' + ws + '/' + wf
						+ ' wgpuProxyIds=' + wlq + '/' + wc + '/' + wlf
						+ ' wgpuSubmit=' + submitCalls + '/' + submitDone + '/' + submitCmds + ':' + submitStatus
						+ ' wgpuPresent=' + presentBegins + '/' + presentEnds + '/' + presentStage + '(' + (presentStageNames[presentStage] || '?') + ')/' + presentLastStage + '(' + (presentStageNames[presentLastStage] || '?') + ')'
						+ ' tick=' + tickEntered + '/' + tickEngineDone + '/' + tickPlatformDone + '/' + tickPhase + '(' + (tickPhaseNames[tickPhase] || '?') + ')'
						+ ' frameSync=' + frameSyncs + '/' + xrFrameSyncs);
				}
			} else {
				if (stalledTicks >= 3 && f !== lastFrame) {
					console.error('[WndrHeartbeat] RECOVERED at frame ' + f + ' after ' + (stalledTicks * 5) + 's stall — lost task-queue wakeup CONFIRMED (kick worked)');
				}
				stalledTicks = 0;
			}
			lastFrame = f;
		} catch (e) { /* never break the page */ }
	}, 5000);
})();

async function wndrFetchVfsAsset(hash, mode) {
	const boostedUrl = wndrVfsAssetUrl(hash);
	if (boostedUrl) {
		const response = await wndrTimedFetch(boostedUrl, { cache: "force-cache" }, 'vfs-boosted');
		if (response.ok) {
			return { response, boosted: true, fallback: false };
		}
		return {
			response: await fetch(wndrPrimaryVfsAssetUrl(hash, mode), { cache: "no-store" }),
			boosted: true,
			fallback: true
		};
	}

	return {
		response: await fetch(wndrPrimaryVfsAssetUrl(hash, mode), { cache: "no-store" }),
		boosted: false,
		fallback: false
	};
}
async function fetchAsset(index, hash, put_in_memory = false) {
	try {
		const result = await wndrFetchVfsAsset(hash, 'asset');
		let resp = result.response;
		if (result.fallback) {
			_failed_fetch_opt(index);
		}
		if (!resp.ok) {
			if (result.boosted && !result.fallback) {
				_failed_fetch_opt(index);
				resp = await wndrTimedFetch(wndrPrimaryVfsAssetUrl(hash, 'asset'), { cache: "no-store" }, 'asset-retry');
			}
			if (!resp.ok) {
				_failed_fetch(index);
				Module.fetchFailCallback?.(hash) || console.log('failed fetching', hash);
				return;
			}
		}
		if (put_in_memory) {
			Module.cache.put(hash, resp.clone());
			const buf = await resp.arrayBuffer();
			const u8_buffer = new Uint8Array(buf);
			const buffer = _simplystream_malloc(WNDR_JS_SIZE_T(u8_buffer.length), index);
			HEAPU8.set(u8_buffer, Number(buffer));
			_PutLevel0(index, buffer, u8_buffer.length);
		} else {
			Module.cache.put(hash, resp);
			_SetFetched(index);
		}
	} catch (err) {
		console.error('Network error fetching asset:', hash, err);
		// Unwedge C++ waiters: reset FETCHING so View::wait_for_ready can re-issue.
		_failed_fetch(index);
	}
}
async function fetch_and_decompress_cacheapi(index, hash) {
  let now = performance.now();
  const cache_resp = await caches.match(hash, { cacheName: 'cache' });
  Module.matchTime += performance.now() - now;
  if (!cache_resp) {
    try {
		fetchAsset(index, hash, true);
	} catch (e) {
		console.error('failed fetch_and_decompress ' + e);
		//throw new Error('failed fetch_and_decompress ' + e);
    }
  } else {
    now = performance.now();
    const buf = await cache_resp.arrayBuffer();
    Module.arrayBufferTime += performance.now() - now;
    now = performance.now();
    const u8_buffer = new Uint8Array(buf);
    const buffer = _simplystream_malloc(WNDR_JS_SIZE_T(u8_buffer.length), index);
    Module.mallocTime += performance.now() - now;
    now = performance.now();
    HEAPU8.set(u8_buffer, Number(buffer));
    _PutLevel0(index, buffer, u8_buffer.length);
    Module.putTime += performance.now() - now;
    //console.log('fetch_and_decompress done', v);
  }
}
async function fetch_and_decompress_opfs(index, hash) {
	let now = performance.now();
	let handle;
	Module.use_cache = false;
	if (!Module.root_dir) {
		Module.root_dir = await navigator.storage.getDirectory();
	}
	if (Module.use_cache) {
		if (!Module.opfs2_cache) {
			Module.opfs2_cache = new Map();
		}
		if (!Module.opfs2_cache.has(hash)) {
			let temp_handle = await Module.root_dir.getFileHandle(hash.toString(), { create: true });
			handle = await temp_handle.createSyncAccessHandle({ mode: 'readwrite-unsafe' });
			Module.opfs2_cache.set(hash, handle);
		} else {
			handle = Module.opfs2_cache.get(hash);
		}
	}
	else {
		async function createSyncAccessHandle_timeout(hash, timeout = 10000) {
			const controller = new AbortController();
			const id = setTimeout(() => controller.abort(), timeout);
			let temp_handle = await Module.root_dir.getFileHandle(hash.toString(), { create: true });
			const handle = await temp_handle.createSyncAccessHandle({ mode: 'readwrite-unsafe' });
			return handle;
		}
		handle = await createSyncAccessHandle_timeout(hash)
		//let temp_handle = await Module.root_dir.getFileHandle(hash.toString(), { create: true });
		//handle = await temp_handle.createSyncAccessHandle({ mode: 'readwrite-unsafe' });
	}
	Module.matchTime += performance.now() - now;
	const size = handle.getSize();
	if (size === 0) {
		try {
			let fetch_resp = (await wndrFetchVfsAsset(hash, 'fetch')).response;
			if (fetch_resp.status != 200) {
				if (Module.fetchFailCallback != undefined) {
					Module.fetchFailCallback(hash);
				} else {
					console.log('failed fetching', hash);
				}
				return;
			} else {
				const buf = await fetch_resp.arrayBuffer();
				Module.arrayBufferTime += performance.now() - now; now = performance.now();
				const u8_buffer = new Uint8Array(buf);
				handle.write(u8_buffer);
				handle.truncate(u8_buffer.length);
				handle.flush();
				if (!Module.use_cache) {
					handle.close();
				}
				now = performance.now();
				const buffer = _simplystream_malloc(WNDR_JS_SIZE_T(u8_buffer.length), index);
				Module.mallocTime += performance.now() - now; now = performance.now();
				HEAPU8.set(u8_buffer, Number(buffer));
				_PutLevel0(index, buffer, u8_buffer.length);
				Module.putTime += performance.now() - now;
			}
		} catch (e) {
			console.log('failed fetch_and_decompress', e);
		}
	} else {
		now = performance.now();
		const buf = new Uint8Array(new ArrayBuffer(size));
		const read = handle.read(buf, { at: 0 });
		Module.arrayBufferTime += performance.now() - now; now = performance.now();
		const u8_buffer = new Uint8Array(buf);
		const buffer = _simplystream_malloc(WNDR_JS_SIZE_T(u8_buffer.length), index);
		Module.mallocTime += performance.now() - now; now = performance.now();
		HEAPU8.set(u8_buffer, Number(buffer));
		_PutLevel0(index, buffer, u8_buffer.length);
		Module.putTime += performance.now() - now;
	}
}
async function init() {
	if (navigator.storage && navigator.storage.getDirectory) {
		fs_type = 1;
	}
	// if storage quota is suffecient, use fast mode
	if (navigator.storage && navigator.storage.estimate) {
		const { usage, quota } = await navigator.storage.estimate();
		Module.quota = quota;
		if (quota < min_mb_needed * 1024 * 1024) {
			console.error('SimplyStream requires at least 1GB of free storage space for optimal performance.');
		}
		else if (quota > mb_needed_for_fast_mode * 1024 * 1024) {
			fast_mode = 2;
			// disabled until proper zstd 18 support is added to cdn
			/*
			// check for zstd support
			let r = await fetch('https://' + Module.platform_addr + '/fetch/asset/zstd');
			if (r.status === 200) {
				// check for content-encoding support
				if (r.headers.get('content-encoding') === 'zstd') {
					fast_mode = 1;
					_SetFastMode();
				}
			}
			*/
		}
	}
}
async function fetch_and_decompress(index, hash) {
	const is_node = typeof process === 'object' && process.versions && process.versions.node;
	if (is_node || 1) {
		return Module.limit(fetch_and_decompress_cacheapi, index, hash);
	}
	else {
		return Module.limit(fetch_and_decompress_opfs, index, hash);
	}
}
async function clear_opfs() {
    if (!Module.root_dir) {
        Module.root_dir = await navigator.storage.getDirectory();
    }
    let v = Module.root_dir.entries();
    let done = false;
    let removed_entries = 0;
    while (done === false) {
    let r = await v.next();
    if (r.done) {
        done = true;
    } else {
        removed_entries++;
        // Module.root_dir.removeEntry(r.key);
        let f = r.value;
        if (f.isFile) {
        await f.remove();
        }
    }
    }
    console.log('Cleared OPFS', removed_entries);
}
// --- Constants & Helper Data Structures ---

const TIER_ORDER = ["high", "medium", "low", "compat"];
const FEATURE_NAMES = [
	"core-features-and-limits",
	"depth-clip-control",
	"depth32float-stencil8",
	"texture-compression-bc",
	"texture-compression-bc-sliced-3d",
	"texture-compression-etc2",
	"texture-compression-astc",
	"texture-compression-astc-sliced-3d",
	"timestamp-query",
	"indirect-first-instance",
	"shader-f16",
	"rg11b10ufloat-renderable",
	"bgra8unorm-storage",
	"float32-filterable",
	"float32-blendable",
	"clip-distances",
	"dual-source-blending",
	"subgroups",
	"texture-formats-tier1",
	"texture-formats-tier2",
	"primitive-index"
];

// --- Helper Functions ---

/**
 * Aligns an offset to the specified byte alignment.
 * @param {number} offset - Current offset.
 * @param {number} alignment - Alignment boundary (default is 8 bytes).
 * @returns {number} - Aligned offset.
 */
function alignOffset(offset, alignment = 8) {
  return offset % alignment === 0 ? offset : offset + (alignment - (offset % alignment));
}

/**
 * Classifies the GPU tier based on supported limits and features.
 * @param {object} limits - The GPU limits.
 * @param {Set} features - The set of supported features.
 * @returns {{tier: string, diagnosticInfo: object}}
 */
function classifyTier(limits, features) {
	const TIER_REQUIREMENTS = /*window?.parent?.state?.tierRequirements ||*/ {
		limits: {
			maxBindGroups: [4, 4, 4, 4],
			maxBindGroupsPlusVertexBuffers: [24, 24, 24, 24],
			maxBindingsPerBindGroup: [1000, 1000, 1000, 1000],
			maxBufferSize: [2147483644, 2147483644, 429496728, 268435456],
			maxColorAttachmentBytesPerSample: [64, 64, 32, 32],
			maxColorAttachments: [8, 8, 8, 8],
			maxComputeInvocationsPerWorkgroup: [1024, 1024, 256, 256],
			maxComputeWorkgroupSizeX: [1024, 1024, 256, 256],
			maxComputeWorkgroupSizeY: [1024, 1024, 256, 256],
			maxComputeWorkgroupSizeZ: [64, 64, 64, 64],
			maxComputeWorkgroupStorageSize: [32768, 16384, 16384, 16384],
			maxDynamicStorageBuffersPerPipelineLayout: [8, 8, 4, 4],
			maxDynamicUniformBuffersPerPipelineLayout: [10, 10, 8, 8],
			maxInterStageShaderVariables: [28, 28, 16, 16],
			maxSampledTexturesPerShaderStage: [44, 44, 16, 16],
			maxComputeWorkgroupsPerDimension: [65535, 65535, 65535, 65535],
			maxSamplersPerShaderStage: [16, 16, 16, 16],
			maxStorageBufferBindingSize: [2147483644, 2147483644, 429496728, 134217728],
			maxStorageBuffersPerShaderStage: [16, 10, 10, 8],
			maxStorageTexturesPerShaderStage: [8, 8, 4, 4],
			maxTextureArrayLayers: [2048, 2048, 256, 256],
			maxTextureDimension1D: [16384, 16384, 8192, 8192],
			maxTextureDimension2D: [16384, 16384, 8192, 8192],
			maxTextureDimension3D: [2048, 2048, 2048, 2048],
			maxUniformBufferBindingSize: [65536, 65536, 65536, 16384],
			maxUniformBuffersPerShaderStage: [12, 12, 12, 12],
			maxVertexAttributes: [30, 30, 16, 16],
			maxVertexBufferArrayStride: [2048, 2048, 2048, 2048],
			maxVertexBuffers: [8, 8, 8, 8],
		},
		features: [
			["depth32float-stencil8", "indirect-first-instance", "rg11b10ufloat-renderable"],
			["depth32float-stencil8", "indirect-first-instance"],
			["depth32float-stencil8", "indirect-first-instance"],
			[]
		]
	};

	// wndr 5.8: refine the curated tier table with THIS build's MEASURED requirements (advisory). The launcher
	// injects the per-feature-level manifest into state.webgpu_requirements; map sm5 -> tiers [0,1] (high/medium)
	// and es31 -> tiers [2,3] (low/compat), matching WebGPUDevice.cpp's tierCode>1 => ES3_1. Only measured keys
	// are overridden (measured truth replaces the curated guess for those limits); everything else keeps the
	// curated defaults. No manifest => unchanged (legacy behavior).
	try {
		const measured = (typeof window !== "undefined" && window?.parent?.state?.webgpu_requirements) || null;
		const sm5 = measured?.perFeatureLevel?.sm5?.limits || {};
		const es31 = measured?.perFeatureLevel?.es31?.limits || {};
		for (const key of new Set([...Object.keys(sm5), ...Object.keys(es31)])) {
			const base = TIER_REQUIREMENTS.limits[key] || [0, 0, 0, 0];
			const sm5v = sm5[key] ?? base[0];
			const es31v = es31[key] ?? base[2];
			TIER_REQUIREMENTS.limits[key] = [sm5v, sm5v, es31v, es31v];
		}
	} catch (e) { /* keep curated defaults on any error */ }

  const diagnosticInfo = {};
  for (let i = 0; i < TIER_ORDER.length; i++) {
    const tier = TIER_ORDER[i];
    const limitFailures = Object.entries(TIER_REQUIREMENTS.limits).reduce((failures, [key, reqValues]) => {
      if (limits[key] === undefined || limits[key] < reqValues[i]) {
        failures.push({ name: key, required: reqValues[i], actual: limits[key] });
      }
      return failures;
    }, []);
    const featureFailures = TIER_REQUIREMENTS.features[i].filter(f => !features.has(f));
    diagnosticInfo[tier] = {
      pass: limitFailures.length === 0 && featureFailures.length === 0,
      limitsFailed: limitFailures,
      featuresFailed: featureFailures,
    };
    if (diagnosticInfo[tier].pass) {
      return { tier, diagnosticInfo };
    }
  }
  return { tier: "none", diagnosticInfo };
}

/**
 * Generates a concise failure report from diagnostic information.
 * @param {object} diagnosticInfo - Diagnostic data for each tier.
 * @returns {string} - Concise failure report.
 */
function generateFailureReport(diagnosticInfo) {
  return Object.entries(diagnosticInfo)
    .map(([tier, res]) => {
      if (!res.pass) {
        const limitsReport = res.limitsFailed.map(l => `${l.name}: needs ${l.required}, has ${l.actual}`).join(', ');
        const featuresReport = res.featuresFailed.join(', ');
        return `Tier "${tier}" failed:\n${limitsReport ? `- Limits: ${limitsReport}\n` : ""}${featuresReport ? `- Missing features: ${featuresReport}\n` : ""}`;
      }
      return "";
    })
    .filter(Boolean)
    .join('\n');
}

/**
 * Populates a WASM struct with GPU limits and feature flags.
 * @param {object} limits - The GPU limits.
 * @param {Set} features - The set of supported features.
 * @returns {{buffer: ArrayBuffer, structSize: number}} - The filled buffer and its size.
 */
function populateWasmStruct(limits, features, canvas_format, tierCode) {
	// Define the struct size
	const structSize = 168;
	const buffer = new ArrayBuffer(structSize);
	const view = new DataView(buffer);
	let offset = 0;

	// Helper to write an array of 32-bit values.
	function writeUint32Array(values) {
	values.forEach(v => {
		view.setUint32(offset, v, true);
		offset += 4;
	});
	}

	// Write first block of 32-bit fields.
	writeUint32Array([
		limits.maxTextureDimension1D,
		limits.maxTextureDimension2D,
		limits.maxTextureDimension3D,
		limits.maxTextureArrayLayers,
		limits.maxBindGroups,
		limits.maxBindGroupsPlusVertexBuffers,
		limits.maxBindingsPerBindGroup,
		limits.maxDynamicUniformBuffersPerPipelineLayout,
		limits.maxDynamicStorageBuffersPerPipelineLayout,
		limits.maxSampledTexturesPerShaderStage,
		limits.maxSamplersPerShaderStage,
		limits.maxStorageBuffersPerShaderStage,
		limits.maxStorageTexturesPerShaderStage,
		limits.maxUniformBuffersPerShaderStage,
	]);

	offset = alignOffset(offset, 8);

	// Write two 64-bit fields.
	view.setBigUint64(offset, BigInt(limits.maxUniformBufferBindingSize), true); offset += 8;
	view.setBigUint64(offset, BigInt(limits.maxStorageBufferBindingSize), true); offset += 8;

	// Write three more 32-bit fields.
	writeUint32Array([
		limits.minUniformBufferOffsetAlignment,
		limits.minStorageBufferOffsetAlignment,
		limits.maxVertexBuffers,
	]);

	offset = alignOffset(offset, 8);

	// Write an additional 64-bit field.
	view.setBigUint64(offset, BigInt(limits.maxBufferSize), true);
	offset += 8;

	// Write final block of 32-bit fields.
	writeUint32Array([
		limits.maxVertexAttributes,
		limits.maxVertexBufferArrayStride,
		limits.maxInterStageShaderVariables,
		limits.maxColorAttachments,
		limits.maxColorAttachmentBytesPerSample,
		limits.maxComputeWorkgroupStorageSize,
		limits.maxComputeInvocationsPerWorkgroup,
		limits.maxComputeWorkgroupSizeX,
		limits.maxComputeWorkgroupSizeY,
		limits.maxComputeWorkgroupSizeZ,
		limits.maxComputeWorkgroupsPerDimension,
		limits.maxImmediateSize
	]);

	// Write feature flags (1 byte each).
	if (FEATURE_NAMES.length !== 21) throw new Error("FEATURE_NAMES must be 21 entries.");
	for (const f of FEATURE_NAMES) {
		const on = features.has(f) || f === "core-features-and-limits";
		view.setUint8(offset++, on ? 1 : 0);
	}

	// Write canvas format and tier code.
	view.setInt8(offset, canvas_format); offset += 1;
	view.setInt8(offset, tierCode); offset += 1;

	return { buffer, structSize };
}

// wndr webxr: immersive-vr session runtime (see Engine/Platforms/SimplyStream/Upgrade/xr-port.md).
// The JS main thread owns the XRSession. Per XR rAF: write the pose snapshot into wasm memory
// (FWebXRPoseSnapshot, WebXRSnapshot.h — byte offsets mirrored here), drive the engine frame sync,
// and copy the engine's last completed frame (staged at presented-frame time) into the projection
// layer's per-eye subimages. The engine never blocks on the XR frame (one-frame pipelining).
function createWebXRRuntime(device) {
	// Exported by the WebXR plugin (Plugins/Runtime/WebXR). Feature-detect so a build without the
	// plugin (or without -webxr) degrades to no XR instead of crashing.
	if (typeof Module._SIMPLYSTREAM_GetXRPoseBuffer !== 'function' ||
		typeof Module._SIMPLYSTREAM_GetXRFrameInfo !== 'function' ||
		typeof Module._SIMPLYSTREAM_XRFrameSync !== 'function' ||
		typeof Module._SIMPLYSTREAM_SetXRDriven !== 'function') {
		console.warn('[WebXR] wasm exports missing (WebXR plugin not in this build) — XR disabled');
		return null;
	}
	const posePtr = Number(Module._SIMPLYSTREAM_GetXRPoseBuffer());
	const frameInfoPtr = Number(Module._SIMPLYSTREAM_GetXRFrameInfo());

	// --- FWebXRPoseSnapshot byte offsets (240 B; keep in sync with WebXRSnapshot.h) ---
	const POSE_SEQ = posePtr >> 2;            // u32 index
	const POSE_STATE = (posePtr + 4) >> 2;
	const POSE_FRAME = (posePtr + 8) >> 2;
	const POSE_FLAGS = (posePtr + 12) >> 2;
	const POSE_HEAD_F = (posePtr + 16) >> 2;  // f32 index: pos[3] quat[4]
	const POSE_EYE_F = [(posePtr + 44) >> 2, (posePtr + 136) >> 2]; // pos[3] quat[4] proj[16] each
	const POSE_RECW = (posePtr + 228) >> 2;
	const POSE_RECH = (posePtr + 232) >> 2;
	const POSE_TIME_F = (posePtr + 236) >> 2;
	// --- FWebXRFrameInfo byte offsets (40 B) ---
	const FI_SEQ = frameInfoPtr >> 2;
	const FI_PTR_LO = (frameInfoPtr + 8) >> 2;
	const FI_PTR_HI = (frameInfoPtr + 12) >> 2;
	const FI_W = (frameInfoPtr + 16) >> 2;
	const FI_H = (frameInfoPtr + 20) >> 2;
	const FI_STEREO = (frameInfoPtr + 28) >> 2;

	const state = {
		session: null, binding: null, layer: null, refSpace: null,
		stagingTex: null, stagingFormat: null, stereoMode: 0,
		sessionState: 0, frameNumber: 0, recW: 0, recH: 0,
		lastXRFrame: 0, watchdog: null, button: null
	};

	function writePose(pose, timeMS) {
		const u32 = HEAPU32, f32 = HEAPF32;
		const s = Atomics.load(u32, POSE_SEQ);
		Atomics.store(u32, POSE_SEQ, (s | 1) >>> 0);
		u32[POSE_STATE] = state.sessionState;
		u32[POSE_FRAME] = state.frameNumber;
		u32[POSE_FLAGS] = pose ? 1 : 0;
		if (pose) {
			const h = pose.transform;
			f32[POSE_HEAD_F] = h.position.x; f32[POSE_HEAD_F + 1] = h.position.y; f32[POSE_HEAD_F + 2] = h.position.z;
			f32[POSE_HEAD_F + 3] = h.orientation.x; f32[POSE_HEAD_F + 4] = h.orientation.y;
			f32[POSE_HEAD_F + 5] = h.orientation.z; f32[POSE_HEAD_F + 6] = h.orientation.w;
			for (let i = 0; i < Math.min(2, pose.views.length); i++) {
				const v = pose.views[i];
				const base = POSE_EYE_F[(v.eye === 'right') ? 1 : i];
				const t = v.transform;
				f32[base] = t.position.x; f32[base + 1] = t.position.y; f32[base + 2] = t.position.z;
				f32[base + 3] = t.orientation.x; f32[base + 4] = t.orientation.y;
				f32[base + 5] = t.orientation.z; f32[base + 6] = t.orientation.w;
				const proj = v.projectionMatrix;
				for (let j = 0; j < 16; j++) { f32[base + 7 + j] = proj[j]; }
			}
		}
		u32[POSE_RECW] = state.recW;
		u32[POSE_RECH] = state.recH;
		f32[POSE_TIME_F] = timeMS || 0;
		Atomics.store(u32, POSE_SEQ, ((s | 1) + 1) >>> 0);
	}

	function readFrameInfo() {
		const u32 = HEAPU32;
		for (let attempt = 0; attempt < 4; attempt++) {
			const s1 = Atomics.load(u32, FI_SEQ);
			if (s1 & 1) continue;
			const lo = u32[FI_PTR_LO], hi = u32[FI_PTR_HI];
			const w = u32[FI_W], h = u32[FI_H], stereo = u32[FI_STEREO];
			const s2 = Atomics.load(u32, FI_SEQ);
			if (s1 === s2) return { ptr: hi * 0x100000000 + lo, w: w, h: h, stereo: stereo };
		}
		return null;
	}

	function resolveEngineTexture(fi) {
		if (!fi || !fi.ptr) return null;
		try {
			if (typeof WebGPU !== 'undefined') {
				if (typeof WebGPU.getJsObject === 'function') return WebGPU.getJsObject(fi.ptr) || null;
				if (WebGPU.Internals && WebGPU.Internals.jsObjects) return WebGPU.Internals.jsObjects[fi.ptr] || null;
			}
		} catch (e) { /* stale pointer mid-realloc — skip this frame */ }
		return null;
	}

	// Staging copy at presented-frame time: the proxy queue is FIFO and the endOfFrame task runs
	// after all of the frame's submits, so the engine texture holds a complete frame here, and the
	// main thread serializes this copy against engine submissions (WGPU_PROXY_METHOD).
	function onPresentedFrame() {
		if (!state.session) return;
		const fi = readFrameInfo();
		const tex = resolveEngineTexture(fi);
		if (!tex) return;
		try {
			if (!state.stagingTex || state.stagingTex.width !== tex.width ||
				state.stagingTex.height !== tex.height || state.stagingFormat !== tex.format) {
				if (state.stagingTex) state.stagingTex.destroy();
				state.stagingTex = device.createTexture({
					size: [tex.width, tex.height],
					format: tex.format,
					usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC
				});
				state.stagingFormat = tex.format;
				console.log('[WebXR] staging texture ' + tex.width + 'x' + tex.height + ' ' + tex.format);
			}
			const enc = device.createCommandEncoder();
			enc.copyTextureToTexture({ texture: tex }, { texture: state.stagingTex },
				{ width: tex.width, height: tex.height });
			device.queue.submit([enc.finish()]);
			state.stereoMode = fi.stereo;
		} catch (e) { /* size/format raced a realloc — next frame catches up */ }
	}

	function copyEyes(pose) {
		if (!state.stagingTex) return;
		try {
			const enc = device.createCommandEncoder();
			for (let i = 0; i < Math.min(2, pose.views.length); i++) {
				const view = pose.views[i];
				const sub = state.binding.getViewSubImage(state.layer, view);
				const vp = sub.viewport;
				if (i === 0) { state.recW = vp.width; state.recH = vp.height; }
				const viewIndex = (view.eye === 'right') ? 1 : 0;
				const srcW = state.stereoMode === 1 ? (state.stagingTex.width >> 1) : state.stagingTex.width;
				const w = Math.min(srcW, vp.width);
				const h = Math.min(state.stagingTex.height, vp.height);
				enc.copyTextureToTexture(
					{ texture: state.stagingTex, origin: { x: state.stereoMode === 1 ? viewIndex * srcW : 0, y: 0 } },
					{ texture: sub.colorTexture, origin: { x: vp.x, y: vp.y, z: sub.imageIndex || 0 } },
					{ width: w, height: h, depthOrArrayLayers: 1 });
			}
			device.queue.submit([enc.finish()]);
		} catch (e) {
			if (!state.copyErrorLogged) {
				state.copyErrorLogged = true;
				console.error('[WebXR] eye copy failed (format/usage mismatch? see probe page):', e);
			}
		}
	}

	function xrOnFrame(timeMS, frame) {
		const session = state.session;
		if (!session) return;
		session.requestAnimationFrame(xrOnFrame);
		state.frameNumber++;
		state.lastXRFrame = performance.now();
		let pose = null;
		try { pose = frame.getViewerPose(state.refSpace); } catch (e) { /* tracking loss */ }
		writePose(pose, timeMS);
		Module._SIMPLYSTREAM_XRFrameSync();
		if (pose) copyEyes(pose);
	}

	function mapVisibility(v) { return v === 'visible' ? 2 : (v === 'visible-blurred' ? 3 : 4); }

	function pickLayerFormat(binding) {
		const fi = readFrameInfo();
		const tex = resolveEngineTexture(fi);
		if (tex && tex.format) return tex.format;
		try { return binding.getPreferredColorFormat(); } catch (e) { console.warn('[WebXR] getPreferredColorFormat failed on XRGPUBinding instance', e); }
		try { return XRGPUBinding.getPreferredColorFormat(); } catch (e) { console.warn('[WebXR] static getPreferredColorFormat failed', e); }
		return 'bgra8unorm';
	}

	async function enterVR() {
		if (state.session) return;
		let session;
		try {
			try {
				session = await navigator.xr.requestSession('immersive-vr', { requiredFeatures: ['webgpu', 'local-floor'] });
			} catch (e) {
				console.warn('[WebXR] local-floor unavailable, retrying with webgpu only:', e);
				session = await navigator.xr.requestSession('immersive-vr', { requiredFeatures: ['webgpu'] });
			}
			const binding = new XRGPUBinding(session, device);
			const colorFormat = pickLayerFormat(binding);
			const layer = binding.createProjectionLayer({ colorFormat: colorFormat, scaleFactor: 1.0 });
			session.updateRenderState({ layers: [layer] });
			let refSpace;
			try { refSpace = await session.requestReferenceSpace('local-floor'); }
			catch (e) { refSpace = await session.requestReferenceSpace('local'); }

			state.session = session; state.binding = binding; state.layer = layer; state.refSpace = refSpace;
			state.sessionState = mapVisibility(session.visibilityState || 'visible');
			state.frameNumber = 0; state.copyErrorLogged = false;
			Module.xrSession = session; // consumed by Present's presented-frame scheduler
			writePose(null, 0);
			Module._SIMPLYSTREAM_SetXRDriven(1);
			console.log('[WebXR] session started, layer format ' + colorFormat);

			session.addEventListener('end', onSessionEnd);
			session.addEventListener('visibilitychange', function () {
				state.sessionState = mapVisibility(session.visibilityState);
				writePose(null, 0);
			});
			// Doffed-headset watchdog: XR rAF may stop delivering while hidden; keep the engine's
			// frame sync alive so nothing can deadlock on it.
			state.watchdog = setInterval(function () {
				if (performance.now() - state.lastXRFrame > 250) Module._SIMPLYSTREAM_XRFrameSync();
			}, 250);
			session.requestAnimationFrame(xrOnFrame);
			if (state.button) state.button.style.display = 'none';
		} catch (e) {
			console.error('[WebXR] enterVR failed:', e);
			if (state.button) { state.button.disabled = false; state.button.textContent = 'Enter VR'; }
		}
	}

	function onSessionEnd() {
		console.log('[WebXR] session ended');
		Module._SIMPLYSTREAM_SetXRDriven(0);
		if (state.watchdog) { clearInterval(state.watchdog); state.watchdog = null; }
		state.sessionState = 0;
		writePose(null, 0);
		state.session = null; state.binding = null; state.layer = null; state.refSpace = null;
		Module.xrSession = null;
		if (state.stagingTex) { try { state.stagingTex.destroy(); } catch (e) { console.warn('[WebXR] Failed to destroy staging texture', e); } state.stagingTex = null; }
		if (state.button) { state.button.style.display = ''; state.button.disabled = false; state.button.textContent = 'Enter VR'; }
	}

	// Enter-VR button: the click inside the iframe is the user activation requestSession needs
	// (the parent grants xr-spatial-tracking via the iframe allow attribute).
	(function injectButton() {
		const btn = document.createElement('button');
		btn.textContent = 'Enter VR';
		btn.style.cssText = 'position:fixed;top:12px;right:12px;z-index:10000;padding:10px 18px;' +
			'font:bold 16px sans-serif;background:#1a73e8;color:#fff;border:none;border-radius:6px;cursor:pointer;';
		btn.onclick = function () { btn.disabled = true; btn.textContent = 'Starting…'; enterVR(); };
		(document.body || document.documentElement).appendChild(btn);
		state.button = btn;
	})();

	globalThis.addEventListener('simplystream-webgpu-presented-frame', onPresentedFrame);
	console.log('[WebXR] runtime ready (pose buffer @' + posePtr + ', frame info @' + frameInfoPtr + ')');
	return { enterVR: enterVR, state: state };
}

async function initializeWebGPU(waitId) {
  if (!navigator.gpu) {
    console.error("WebGPU not supported");
    _wgpu_init_status(waitId, WNDR_JS_POINTER(0));
    return "none";
	}
  try {
    // Request the GPU adapter and device.
    // wndr webxr: XRGPUBinding requires the adapter to be requested with xrCompatible; only set when the
    // page opted in AND immersive-vr is supported (Module.webxrEnabled, see simplystream.html preInit).
    // Reuse the adapter requested during bootstrap shader-mode selection. This moves adapter discovery onto the
    // wasm/download critical path instead of paying for a second request after runtime initialization.
    const adapter = await wndrRequestWebGpuAdapter();
	function objLikeToObj(src) {
		const dst = {};
		for (const key in src) {
			dst[key] = src[key];
		}
		// wndr: GPUSupportedLimits exposes its limits as non-enumerable accessors on the prototype, so the for..in
		// above yields {} in Chrome. requiredLimits would then be empty and the device falls back to DEFAULT (low)
		// limits even on a 16-capable adapter — under-provisioning it so the SM5 GPUScene instance-cull compute
		// (11 storage buffers) fails CreateBindGroupLayout against the default maxStorageBuffersPerShaderStage.
		// Explicitly pull the numeric limits off the prototype chain so requiredLimits = the adapter's real maxima.
		if (Object.keys(dst).length === 0 && src) {
			for (let p = Object.getPrototypeOf(src); p && p !== Object.prototype; p = Object.getPrototypeOf(p)) {
				for (const key of Object.getOwnPropertyNames(p)) {
					const v = src[key];
					if (typeof v === 'number') { dst[key] = v; }
				}
			}
		}
		return dst;
	}
    const requiredLimitsObj = objLikeToObj(adapter.limits);
    const device = await adapter.requestDevice({
      requiredLimits: requiredLimitsObj,
      requiredFeatures: [...adapter.features],
    });
    const limits = adapter.limits;
    // wndr: diagnostic — confirm the device was provisioned with the adapter's real storage-buffer limit (not the
    // default). If keys==0 the requiredLimits copy failed; if device < adapter the request was clamped.
    console.log("[SimplyStream][limits] maxStorageBuffersPerShaderStage — adapter:", adapter.limits.maxStorageBuffersPerShaderStage,
        "| device(granted):", device.limits && device.limits.maxStorageBuffersPerShaderStage,
        "| requiredLimits keys:", Object.keys(requiredLimitsObj).length);
    const features = new Set(adapter.features);
    console.log("WebGPU Adapter Info:", { limits: { ...limits }, features: [...features] });

    // Classify the device tier.
    const { tier, diagnosticInfo } = classifyTier(limits, features);
    console.log("WebGPU Tier Classification Results:", diagnosticInfo);
    if (tier === "none") {
      const failureReport = generateFailureReport(diagnosticInfo);
      console.error("WebGPU initialization failed:\n" + failureReport);
      _wgpu_init_status(waitId, WNDR_JS_POINTER(0));
      return "none";
    }

    console.log(`Device initialized with tier: ${tier}`);
    Module.preinitializedWebGPUDevice = device;
    Module.preinitializedWebGPUAdapter = adapter;

    // wndr webxr: Module.webxrEnabled is only set when ?webxr=1 AND immersive-vr is supported
    // (simplystream.html preInit); the adapter above was then requested with xrCompatible.
    if (Module.webxrEnabled) {
      try {
        Module.webxr = createWebXRRuntime(device);
      } catch (e) {
        console.error('[WebXR] runtime init failed:', e);
      }
    }

    // Determine canvas format.
    const canvas_format = navigator.gpu.getPreferredCanvasFormat() === 'bgra8unorm' ? 0 : 1;
    let tierCode = TIER_ORDER.indexOf(tier);

    // wndr 5.8: the device's tier picks a feature level, with two adjustments from launcher-injected state:
    //  1) Admin FORCE (state.feature_level_mode, set per-revision or per-launch-profile in the dashboard): pin
    //     the level. Tiers 0-1 map to SM5 and 2-3 to ES3_1 (WebGPUDevice.cpp: tierCode>1 => ES3_1).
    //  2) Cooked-availability CLAMP (state.shader_formats): never select a level the build doesn't contain, so a
    //     force (or device pick) for an absent level degrades safely — e.g. forcing/landing SM5 on an ES3_1-only
    //     build clamps back to ES3_1. The launcher floor already routes truly-incapable devices to pixel.
    // NOTE: this sets GMaxRHIFeatureLevel via the tier; the more complete force (also affecting device-profile
    // init) is `-dpcvars=r.WebGPU.ForcedMode=N` in Module.arguments — switch to that if device-profile-level
    // feature-level branching is ever added.
    try {
      const st = (typeof window !== "undefined" && window?.parent?.state) || {};
      const forced = st.feature_level_mode;
      if (forced === "sm5") tierCode = 0;        // force SM5 (high)
      else if (forced === "es31") tierCode = 2;  // force ES3_1 (low)
      if (forced === "sm5" || forced === "es31") {
        console.log(`[SimplyStream] WebGPU feature level forced to '${forced}' (admin setting).`);
      }

      const formats = st.shader_formats || [];
      if (formats.length > 0) {
        const hasSM5 = formats.includes("WEBGPU_SM5") || formats.includes("WEBGPU_SM6");
        const hasES31 = formats.includes("WEBGPU_ES31");
        const before = tierCode;
        if (!hasSM5 && hasES31 && tierCode <= 1) tierCode = 2;        // level not built -> ES3_1
        else if (!hasES31 && hasSM5 && tierCode >= 2) tierCode = 1;   // level not built -> SM5
        if (tierCode !== before) {
          const from = TIER_ORDER[before], to = TIER_ORDER[tierCode];
          console.warn(`[SimplyStream] WebGPU feature-level fallback: '${from}' but the build only cooked [${formats.join(", ")}] — clamping to '${to}'.`);
        }
      }
    } catch (e) { /* no state hint -> trust the device tier */ }

    // A 10-storage-buffer/stage device can still run WebGPU SM5. Per-shader RHI/cooker
    // virtualization handles paths that would otherwise exceed the limit; do not downgrade
    // the entire feature level here.

    // Populate the WASM struct.
    const { buffer, structSize } = populateWasmStruct(limits, features, canvas_format, tierCode);
    const ptr = Module._malloc(structSize);
    new Uint8Array(HEAPU8.buffer, ptr, structSize).set(new Uint8Array(buffer));

    // Report success with the appropriate tier code.
    _wgpu_init_status(waitId, WNDR_JS_POINTER(ptr));
    return tier;
  } catch (error) {
    console.error("Failed to initialize WebGPU", error);
    _wgpu_init_status(waitId, WNDR_JS_POINTER(0));
    return "none";
  }
}
