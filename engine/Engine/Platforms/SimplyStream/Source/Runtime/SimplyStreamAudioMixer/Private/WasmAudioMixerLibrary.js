// SimplyStream wasm audio mixer — Web Audio + AudioWorklet backend.
// Browser-side glue (paired with WasmAudioMixer.cpp). Registered with emcc as
// `--js-library` via SimplyStreamAudioMixer.Build.cs.
//
// The C++ side (WasmAudioMixer.cpp) implements IAudioMixerPlatformInterface
// and forwards into the C functions declared here. We keep all browser-side
// handles (AudioContext, worklet node) on the main thread. The audio ring itself
// lives in WASM linear memory (a SharedArrayBuffer in this pthread build) and is
// OWNED by the C side (WasmAudioMixer.cpp): its SubmitBuffer writes the ring
// directly on the engine's null-device pump pthread, and the AudioWorklet reads the
// same bytes via the wasm Memory. No per-buffer JS call, no cross-thread proxying.
// (The earlier design called a JS wndr_audio_submit from that pthread, but
// $WndrAudio is per-worker JS state — only the main thread's was initialized — so
// the call no-op'd and audio stayed silent.)
//
// Ring layout in wasm memory at ringPtr (32-bit aligned):
//   [0]    atomic int32 writeIdx (float-samples, mod bufSamples) — written by C, read by worklet
//   [1]    atomic int32 readIdx                                   — written by worklet, read by C
//   [2]    int32 bufSamples (capacity in float-samples; const after init)
//   [3]    int32 numChannels (interleaved layout L,R,L,R,…)
//   [16..] float32 audio ring (bufSamples samples)
'use strict';

mergeInto(LibraryManager.library, {
	$WndrAudio__deps: [],
	$WndrAudio: {
		ctx: null,
		worklet: null,
		sab: null,
		header: null,           // Int32Array view of the indices/meta
		ring: null,             // Float32Array view of the audio body
		bufSamples: 0,          // Float-sample capacity of the ring
		numChannels: 2,
		sampleRate: 0,
		started: false,
		queuedSubmits: 0,       // For diagnostics — overruns
		gestureHooked: false,

		// Install a one-shot "first user gesture resumes the AudioContext" hook.
		// Browsers start the context SUSPENDED under the autoplay policy; while
		// suspended the AudioWorklet's process() never runs, so the ring the C side
		// fills (on the null-device pump thread) is never drained → total silence.
		// We listen on `window` in the CAPTURE phase (and also on document) so the
		// emscripten/SDL game canvas — which commonly calls stopPropagation() on
		// pointer/key events and can also hold Pointer Lock — cannot swallow the
		// gesture before we see it. Installed at init so it does not depend on the
		// C side calling wndr_audio_start at exactly the right moment.
		installGestureResume: function() {
			var s = WndrAudio;
			if (s.gestureHooked || typeof window === 'undefined') return;
			s.gestureHooked = true;
			var onGesture = function() {
				if (!s.ctx) return;
				var p = s.ctx.resume();
				if (p && typeof p.then === 'function') {
					p.then(function() { console.log('[WasmAudio] AudioContext resumed on user gesture, state=', s.ctx.state); })
					 .catch(function(e) { console.warn('[WasmAudio] resume() failed:', e && e.message); });
				}
				if (s.ctx.state === 'running') {
					window.removeEventListener('pointerdown', onGesture, true);
					window.removeEventListener('keydown', onGesture, true);
					window.removeEventListener('touchend', onGesture, true);
				}
			};
			// Capture phase + not {once:true}: retry on each gesture until the
			// context actually reaches 'running' (a single resume() can still be
			// rejected if the event was not classified as an activating gesture).
			window.addEventListener('pointerdown', onGesture, true);
			window.addEventListener('keydown', onGesture, true);
			window.addEventListener('touchend', onGesture, true);
		},

		// Pre-built worklet processor source. Loaded as a Blob URL by the
		// main thread so we don't need to ship a separate static asset.
		processorSource: '\
class WasmAudioProcessor extends AudioWorkletProcessor {\n\
	constructor() {\n\
		super();\n\
		this.header = null;\n\
		this.ring = null;\n\
		this.bufSamples = 0;\n\
		this.numChannels = 2;\n\
		this.fadeGain = 1;\n\
		this.lastL = 0;\n\
		this.lastR = 0;\n\
		this.port.onmessage = (e) => {\n\
			if (e.data && e.data.type === "init") {\n\
				// Ring lives in WASM memory: header (4×i32) at ringPtr, float body at ringPtr+16.\n\
				this.header = new Int32Array(e.data.buffer, e.data.ringPtr, 4);\n\
				this.ring = new Float32Array(e.data.buffer, e.data.ringPtr + 16);\n\
				this.bufSamples = this.header[2];\n\
				this.numChannels = this.header[3];\n\
			}\n\
		};\n\
	}\n\
	process(inputs, outputs) {\n\
		const out = outputs[0];\n\
		if (!this.ring) {\n\
			for (let c = 0; c < out.length; c++) out[c].fill(0);\n\
			return true;\n\
		}\n\
		const numFrames = out[0].length;\n\
		const stride = this.numChannels;\n\
		const outCh = Math.min(out.length, stride);\n\
		const cap = this.bufSamples;\n\
		const writeIdx = Atomics.load(this.header, 0);\n\
		let readIdx = Atomics.load(this.header, 1);\n\
		let avail = (writeIdx - readIdx + cap) % cap;\n\
		// Underrun CONCEALMENT: backpressure (C side) keeps the ring full, but a long\n\
		// producer stall can still empty it mid-quantum. Rather than slam a whole\n\
		// quantum to zero (a click/pop), hold the last sample and ramp a gain to 0 —\n\
		// then back up to 1 on recovery — over ~128 samples: an inaudible declick.\n\
		let g = this.fadeGain, lL = this.lastL, lR = this.lastR;\n\
		const STEP = 1 / 128;\n\
		for (let i = 0; i < numFrames; i++) {\n\
			if (avail >= stride) {\n\
				lL = this.ring[readIdx];\n\
				lR = stride > 1 ? this.ring[readIdx + 1 >= cap ? 0 : readIdx + 1] : lL;\n\
				readIdx += stride; if (readIdx >= cap) readIdx -= cap;\n\
				avail -= stride;\n\
				g += STEP; if (g > 1) g = 1;\n\
			} else {\n\
				g -= STEP; if (g < 0) g = 0;\n\
			}\n\
			if (outCh > 0) out[0][i] = lL * g;\n\
			if (outCh > 1) out[1][i] = lR * g;\n\
		}\n\
		this.fadeGain = g; this.lastL = lL; this.lastR = lR;\n\
		Atomics.store(this.header, 1, readIdx);\n\
		return true;\n\
	}\n\
}\n\
registerProcessor("wasm-audio", WasmAudioProcessor);\n',
	},

	// Init creates an AudioContext (suspended), loads the worklet, and connects it to
	// the destination. The ring lives in WASM memory (the C side owns it); we get its
	// wasm byte offset (ringPtr) and hand the worklet the wasm Memory buffer + that
	// offset so it reads the same bytes the C side's SubmitBuffer writes — no separate
	// SAB, no per-buffer JS call. Returns sample rate on success, 0 on failure.
	wndr_audio_init__deps: ['$WndrAudio'],
	wndr_audio_init__proxy: 'sync',
	wndr_audio_init__sig: 'iiii',
	wndr_audio_init: function(ringPtr, bufSamples, numChannels) {
		ringPtr = Number(ringPtr); // MEMORY64: pointer arrives as BigInt; coerce for typed-array offsets
		var s = WndrAudio;
		if (s.ctx) return s.sampleRate;

		if (typeof AudioContext === 'undefined' && typeof webkitAudioContext === 'undefined') {
			console.error('[WasmAudio] No AudioContext available');
			return 0;
		}
		var Ctx = (typeof AudioContext !== 'undefined') ? AudioContext : webkitAudioContext;
		s.ctx = new Ctx();
		s.sampleRate = s.ctx.sampleRate | 0;
		s.numChannels = numChannels;
		s.bufSamples = bufSamples;
		s.ringPtr = ringPtr;

		// Arm the resume-on-first-gesture hook now. The context is created suspended
		// (autoplay policy); without this the worklet never runs and nothing plays.
		s.installGestureResume();

		// The ring is in WASM linear memory (a SharedArrayBuffer in this pthread build).
		// The C side (InitializeHardware) already zeroed the indices and set
		// bufSamples/numChannels in the header at ringPtr. HEAPF32.buffer IS the wasm
		// Memory; share it with the worklet (no copy) so it reads the ring directly.
		var wasmBuf = HEAPF32.buffer;
		s.header = new Int32Array(wasmBuf, ringPtr, 4); // for the pre-init readIdx snap below

		// Load worklet from a Blob URL so this stays self-contained.
		var blob = new Blob([s.processorSource], { type: 'application/javascript' });
		var url = URL.createObjectURL(blob);

		// We can't await synchronously here (proxy is sync from C's view), so kick off
		// the worklet load and finish wiring inside the .then(). The C side may already
		// be writing the ring; the worklet isn't reading yet, so drop any pre-init
		// samples by snapping readIdx=writeIdx once the worklet is connected.
		s.ctx.audioWorklet.addModule(url).then(function() {
			s.worklet = new AudioWorkletNode(s.ctx, 'wasm-audio', {
				numberOfInputs: 0,
				numberOfOutputs: 1,
				outputChannelCount: [numChannels],
			});
			s.worklet.port.postMessage({ type: 'init', buffer: wasmBuf, ringPtr: ringPtr });
			// Discard whatever the engine wrote between init and worklet-ready
			Atomics.store(s.header, 1, Atomics.load(s.header, 0));
			s.worklet.connect(s.ctx.destination);
			URL.revokeObjectURL(url);
			console.log('[WasmAudio] worklet ready @', s.sampleRate, 'Hz, ch=', numChannels, ', ring=', bufSamples, 'samples');
		}).catch(function(err) {
			console.error('[WasmAudio] AudioWorklet init failed:', err);
		});

		return s.sampleRate;
	},

	wndr_audio_get_sample_rate__deps: ['$WndrAudio'],
	wndr_audio_get_sample_rate__sig: 'i',
	wndr_audio_get_sample_rate: function() {
		return WndrAudio.sampleRate;
	},

	// Resume the AudioContext. Browsers gate this on a user gesture.
	wndr_audio_start__deps: ['$WndrAudio'],
	wndr_audio_start__proxy: 'sync',
	wndr_audio_start__sig: 'i',
	wndr_audio_start: function() {
		var s = WndrAudio;
		if (!s.ctx) return 0;
		s.started = true;
		var p = s.ctx.resume();
		if (p && typeof p.then === 'function') {
			p.catch(function(e) { console.warn('[WasmAudio] resume() pending user gesture:', e && e.message); });
		}
		// Ensure the first-gesture resume hook is armed (idempotent). Covers the
		// case where the engine calls start before/without a user gesture.
		if (s.ctx.state !== 'running') {
			s.installGestureResume();
		}
		return 1;
	},

	wndr_audio_stop__deps: ['$WndrAudio'],
	wndr_audio_stop__proxy: 'sync',
	wndr_audio_stop__sig: 'i',
	wndr_audio_stop: function() {
		var s = WndrAudio;
		if (!s.ctx) return 0;
		s.started = false;
		s.ctx.suspend();
		return 1;
	},

	// (wndr_audio_submit removed: the C side's SubmitBuffer writes the wasm-memory
	// ring directly on the null-device pump thread; the worklet reads it via the
	// wasm Memory. The old JS hot path no-op'd because $WndrAudio is per-worker.)
});
