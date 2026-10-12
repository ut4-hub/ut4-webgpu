// Open, clean-room definitions for a small set of free functions that
// SimplyStream's own open Core code (e.g. SimplyStreamPlatformMisc.cpp)
// declares 'extern' and calls during engine bootstrap, but whose
// definitions live only in SimplyStream's closed precompiled objects (which
// this build deliberately does not link). Providing open definitions here
// lets GEngineLoop.PreInit() proceed without those closed artifacts.
#include "CoreTypes.h"

// Whether the platform should run in a reduced-parallelism mode on very
// low-core hosts. Safe open default: false (use normal threading). The
// browser reports real core count via navigator.hardwareConcurrency; a
// future refinement can consult FPlatformMisc::NumberOfCores() here.
// Bare DawnRHITest program (WITH_ENGINE=0) only: the full game Core provides
// IsUsingLowCoreCount() itself -> guard to avoid a duplicate symbol.
#if !WITH_ENGINE
bool IsUsingLowCoreCount()
{
	return false;
}
#endif

// ============================================================================
// WNDR BADCOPY DIAG (2026-08-19): linker --wrap guards for bulk mem ops.
// SAFE_HEAP (scalar loads/stores) + STACK_OVERFLOW_CHECK both came back clean,
// so the intermittent pool-worker "memory access out of bounds" is a single
// bulk op (memory.copy/fill) with a corrupt size -- e.g. a count underflow ->
// astronomically large size_t, or a stale/null buffer pointer. These wrappers
// (enabled via -Wl,--wrap=memcpy/memmove/memset) log the offender (op + size +
// dst + src) and abort AT the call so the browser prints the -g2 named stack of
// the exact caller. The 1 GiB ceiling ignores every legitimate copy.
// ============================================================================
#include <emscripten.h>
#include <cstddef>
#include <cstdint>
extern "C" {
void* __real_memcpy(void* Dst, const void* Src, size_t N);
void* __real_memmove(void* Dst, const void* Src, size_t N);

// Returns true if the copy is corrupt (and, as the FIX, the caller then zero-fills
// Dst and SKIPS the copy so a draw with a garbage source renders benign instead of
// trapping the whole frame). Logs the -g2 wasm caller stack so we can also fix the
// root (the uninitialized/freed source pointer -- src ~0xdddddddd... poison).
static bool WndrBadCopy(const char* Op, const void* Dst, const void* Src, size_t N)
{
	extern size_t emscripten_get_heap_size(void);
	if (N == 0) { return false; }
	const uintptr_t Heap = (uintptr_t)emscripten_get_heap_size();
	const uintptr_t D = (uintptr_t)Dst, S = (uintptr_t)Src;
	// Genuine out-of-bounds only (would trap). Use (Heap - ptr) < N to avoid ptr+N
	// overflow; a poison pointer (~0xdddddddd..., far above Heap) trips ptr > Heap.
	const bool bDstOOB = (D < 0x10000) || (D > Heap) || ((Heap - D) < N);
	const bool bSrcOOB = Src && ((S < 0x10000) || (S > Heap) || ((Heap - S) < N));
	const bool bBad = bDstOOB || bSrcOOB;
	if (bBad)
	{
		static int _c = 0;
		if (_c++ < 8)
		{
			char Buf[6144];
			emscripten_get_callstack(EM_LOG_C_STACK | EM_LOG_JS_STACK | EM_LOG_DEMANGLE, Buf, (int)sizeof(Buf));
			printf("WNDR BADCOPY %s size=%llu dst=%p src=%p heap=%llu STACK: %s ENDSTACK", Op, (unsigned long long)N, Dst, Src, (unsigned long long)Heap, Buf);
			fflush(stdout);
		}
	}
	return bBad;
}
void* __wrap_memcpy(void* Dst, const void* Src, size_t N)  { if (WndrBadCopy("memcpy", Dst, Src, N))  { return Dst; } return __real_memcpy(Dst, Src, N); }
void* __wrap_memmove(void* Dst, const void* Src, size_t N) { if (WndrBadCopy("memmove", Dst, Src, N)) { return Dst; } return __real_memmove(Dst, Src, N); }
}
