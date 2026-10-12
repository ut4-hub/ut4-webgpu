// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	SimplyStreamPlatform.h: Setup for the SimplyStream platform
==================================================================================*/

#pragma once


#include "GenericPlatform/GenericPlatform.h"
#include "Misc/Build.h"
#include "SimplyStreamAssertionMacros.h"
#include <cstdint>

/**
 * SimplyStream specific types
 **/
struct FSimplyStreamTypes : public FGenericPlatformTypes
{
	typedef char16_t					WIDECHAR;
	typedef WIDECHAR					TCHAR;
	//typedef uint64_t					SIZE_T;
};
typedef FSimplyStreamTypes FPlatformTypes;

// Platform defines, overrides from the default (except first 3)
#ifdef PLATFORM_WASM
#undef PLATFORM_WASM
#endif
#define PLATFORM_WASM										1
#define PLATFORM_CAN_SUPPORT_EDITORONLY_DATA				0
#define PLATFORM_DESKTOP									0
#define PLATFORM_LITTLE_ENDIAN								1
#define PLATFORM_SUPPORTS_PRAGMA_PACK						1
#define PLATFORM_USE_LS_SPEC_FOR_WIDECHAR					1
#define PLATFORM_HAS_BSD_TIME								1
#define PLATFORM_MAX_FILEPATH_LENGTH_DEPRECATED				PATH_MAX
#define PLATFORM_TCHAR_IS_CHAR16							1
#define PLATFORM_WCHAR_IS_4_BYTES							1
#define PLATFORM_COMPILER_HAS_IF_CONSTEXPR					1
#define PLATFORM_HAS_BSD_SOCKETS							1
#define PLATFORM_HAS_BSD_SOCKET_FEATURE_MSG_DONTWAIT		1
#define PLATFORM_HAS_NO_EPROCLIM							1
#define PLATFORM_SUPPORTS_TEXTURE_STREAMING					1
#define PLATFORM_REQUIRES_FILESERVER						1
#define PLATFORM_SUPPORTS_TBB								0
#define PLATFORM_SUPPORTS_MIMALLOC							0
#define PLATFORM_ENABLE_VECTORINTRINSICS					SIMPLYSTREAM_SIMD
// wndr 5.8: wasm is neither x86 nor ARM, but the x86 family now pulls in 5.8's real-x86-only code
// (cpuid asm / AES-NI / MXCSR) that emscripten can't compile. Use the ARM/NEON family instead —
// emscripten translates NEON intrinsics to wasm-simd128 (UnrealMathNeon, gated by the _NEON macro) —
// and turn the SSE feature flags off.
#define PLATFORM_ENABLE_VECTORINTRINSICS_NEON				SIMPLYSTREAM_SIMD
#define PLATFORM_MAYBE_HAS_SSE4_1							0
#define PLATFORM_ALWAYS_HAS_SSE4_1							0
#define PLATFORM_ALWAYS_HAS_SSE4_2							0
#define PLATFORM_USES_GLES									1
#define PLATFORM_BUILTIN_VERTEX_HALF_FLOAT					0
#define PLATFORM_SUPPORTS_STACK_SYMBOLS						1
#define PLATFORM_USE_FULL_TASK_GRAPH						1
#define PLATFORM_SUPPORTS_LLM								0 // disabled because of bug in 5.4.2
#define PLATFORM_SUPPORTS_OPUS_CODEC						1
#define PLATFORM_SUPPORTS_VORBIS_CODEC						1
#define PLATFORM_HAS_FPlatformVirtualMemoryBlock			1
#define USE_NAIVE_TLockFreeFixedSizeAllocator_TLSCacheBase	0
#define PLATFORM_EXCEPTIONS_DISABLED						1
#define PLATFORM_SUPPORTS_GEOMETRY_SHADERS					0
#define PLATFORM_SUPPORTS_TESSELLATION_SHADERS				0
#define PLATFORM_COMPILER_SUPPORTS_BUILTIN_BITCAST			1
#define PLATFORM_SUPPORTS_VECTOR_CONTROL_REGISTERS			0

// wndr 5.8/wasm: SimplyStreamPlatform.h does not include Clang/ClangPlatform.h, so clang's
// UE_NO_PROFILE_ATTRIBUTE is undefined for this platform. Any code that uses it (e.g. the DataRegistry
// plugin's UDataRegistry::TimerUpdate, which Lyra pulls in) then fails to compile. Mirror the clang
// definition — emscripten is clang and supports no_profile_instrument_function.
#if defined(__has_attribute)
#	if __has_attribute(no_profile_instrument_function)
#		define UE_NO_PROFILE_ATTRIBUTE __attribute__((no_profile_instrument_function))
#	endif
#endif
#ifndef UE_NO_PROFILE_ATTRIBUTE
#	define UE_NO_PROFILE_ATTRIBUTE
#endif
#define FAST_BOOT_HACKS										1
#define PLATFORM_IS_ANSI_MALLOC_THREADSAFE					1
#define PLATFORM_RETURN_ADDRESS()							0
#define PLATFORM_RETURN_ADDRESS_POINTER()					0
#define TRACE_PRIVATE_THREAD_YIELD							1
#ifdef PLATFORM_CPU_X86_FAMILY
#undef PLATFORM_CPU_X86_FAMILY
#endif
#define PLATFORM_CPU_X86_FAMILY 							0
#ifdef PLATFORM_CPU_ARM_FAMILY
#undef PLATFORM_CPU_ARM_FAMILY
#endif
#define PLATFORM_CPU_ARM_FAMILY 							1

// wndr 5.8/wasm: platform-side compat shim for stock ARM/NEON code that doesn't build under emscripten
// (no engine-source edits — everything lives in Platforms/SimplyStream). See the header for details.
#include "SimplyStreamWasmCompat.h"

void LogCallstack();
void LogError(const char* str);
// WT->WS fallback grace window (seconds, FPlatformTime domain): while now < this, the
// transport fallback owns the connection lifecycle — session-layer HTTP leaves triggered
// by the superseded pending-connection teardown must be SUPPRESSED or they wreck the
// backend session the retry is about to re-register into (field: leave,leave -> join 'Failed
// to join session' -> Lyra failure path -> travel-during-travel -> frozen loading screen).
extern double GWndrNetFallbackGraceUntil;
void LogPlatformBreak(); // logs "PLATFORM_BREAK" + the calling thread's callstack; NEVER traps/throws/pauses
#define PLATFORM_BREAK() ::LogPlatformBreak()

#ifdef __EMSCRIPTEN_PTHREADS__
	#define PLATFORM_USE_PTHREADS							1
	#define PLATFORM_SUPPORTS_EARLY_MOVIE_PLAYBACK			1 // movies will start before engine is initalized
#else
	#define PLATFORM_USE_PTHREADS							0
#endif

// Function type macros.
#define FORCEINLINE								inline __attribute__((__always_inline__))	/* Force code to be inline */
#define FORCENOINLINE							__attribute__((noinline))					/* Force code to NOT be inline */
#define FUNCTION_CHECK_RETURN_END				__attribute__ ((warn_unused_result))		/* Warn that callers should not ignore the return value. */
#define FUNCTION_NO_RETURN_END					__attribute__ ((noreturn))					/* Indicate that the function never returns. */

// Optimization macros
#define PRAGMA_DISABLE_OPTIMIZATION_ACTUAL		_Pragma("clang optimize off")
#define PRAGMA_ENABLE_OPTIMIZATION_ACTUAL		_Pragma("clang optimize on")

#define ABSTRACT								abstract

#define DLLEXPORT			__attribute__((visibility("default")))
#define DLLIMPORT			__attribute__((visibility("default")))

// Alignment.
#define GCC_PACK(n)								__attribute__((packed,aligned(n)))
#define GCC_ALIGN(n)							__attribute__((aligned(n)))
#define RENDERCORE_ATTRIBUTE_UNALIGNED			GCC_ALIGN(1)

// Operator new/delete handling.
// operator new/delete operators
// As of 10.9 we need to use _NOEXCEPT & cxx_noexcept compatible definitions
#if __has_feature(cxx_noexcept)
#define OPERATOR_NEW_THROW_SPEC
#else
#define OPERATOR_NEW_THROW_SPEC					throw (std::bad_alloc)
#endif
#define OPERATOR_DELETE_THROW_SPEC				noexcept
#define OPERATOR_NEW_NOTHROW_SPEC				noexcept
#define OPERATOR_DELETE_NOTHROW_SPEC			noexcept


#define MAXUINT8								((uint8)~((uint8)0))
#define SIMPLYSTREAM_MAX_PATH							1024
