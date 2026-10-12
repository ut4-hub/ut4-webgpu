// SimplyStream wasm — setjmp/longjmp shim (registered with emcc via --js-library by the SimplyStream toolchain).
//
// We deliberately build with -sSUPPORT_LONGJMP=0: emscripten's longjmp support instruments code and hurts
// performance. But some stock engine code (e.g. PngImageWrapper.cpp's libpng error handling, and JPEG/FreeType
// paths) calls setjmp() to install an error-escape that is *only* taken on a decode error. With longjmp support
// off, emscripten links those setjmp/longjmp calls to stubs that abort the program on the very first setjmp().
//
// This shim makes the happy path work at **zero runtime cost** (no instrumentation, SUPPORT_LONGJMP stays 0):
//   setjmp()  -> returns 0  (the "first return": the error branch is skipped, decoding proceeds normally)
//   longjmp() -> abort()     (only reached if a setjmp-based error actually fires, e.g. a corrupt asset)
//
// Trade-off: a library that hits its error path (corrupt PNG/JPEG, etc.) aborts instead of recovering. For a
// shipping title with cooked/validated assets that path should not occur. If you ever need real recovery for a
// setjmp-using library, that library must be reworked to not rely on setjmp/longjmp (or longjmp re-enabled,
// accepting the perf cost).
'use strict';

mergeInto(LibraryManager.library, {
	// jmp_buf pointer arg is ignored — we never need to restore context because longjmp aborts.
	setjmp: function (env) { return 0; },
	longjmp: function (env, value) {
		abort('longjmp() invoked but SUPPORT_LONGJMP=0: a setjmp-based error path was taken (e.g. a corrupt PNG/JPEG/font). ' +
			'Ship valid assets, or rework the offending library to avoid setjmp/longjmp.');
	},
	// libpng's png_jmpbuf() macro expands to `*png_set_longjmp_fn(png_ptr, longjmp, sizeof(jmp_buf))`. The wasm
	// libpng .a doesn't export png_set_longjmp_fn, so PngImageWrapper can't resolve it. Return a valid jmp_buf*
	// (the png_ptr itself is a readable buffer) — our setjmp() ignores the buffer contents anyway.
	png_set_longjmp_fn: function (png_ptr, longjmp_fn, jmp_buf_size) { return png_ptr; },
});
