// Copyright Epic Games, Inc. All Rights Reserved.
//
// SimplyStream wasm/emscripten compatibility shim.
//
// Included from SimplyStreamPlatform.h *after* the PLATFORM_* defines. It works around UE 5.8 stock
// code paths that assume a real ARM CPU and therefore do not build under emscripten — entirely from
// within Platforms/SimplyStream, so engine source stays byte-stock. Inert anywhere but emscripten.
#pragma once

#if defined(__EMSCRIPTEN__)

// (1) Reserved-identifier macro shims (silence -Wreserved-* for these intentional overrides).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-macro-identifier"
#pragma clang diagnostic ignored "-Wreserved-identifier"

//     GenericPlatformProcess.h's spin-yield uses the ARM `__builtin_arm_yield()` hint; emscripten has no
//     such builtin and wasm has no yield/pause. Advisory only -> no-op.
#ifndef __builtin_arm_yield
#define __builtin_arm_yield() ((void)0)
#endif

//     ACL/RTM detect __SSE2__ (emscripten defines it under -msimd128) and call x86 MXCSR FP-exception
//     control which emscripten doesn't provide. wasm has no FP control register -> stub to no-ops.
#ifndef _MM_SET_EXCEPTION_STATE
#define _MM_SET_EXCEPTION_STATE(x) ((void)0)
#endif
#ifndef _MM_SET_EXCEPTION_MASK
#define _MM_SET_EXCEPTION_MASK(x) ((void)0)
#endif
#ifndef _MM_GET_EXCEPTION_MASK
#define _MM_GET_EXCEPTION_MASK() (0u)
#endif
#ifndef _MM_MASK_MASK
#define _MM_MASK_MASK (0u)
#endif
#ifndef _MM_MASK_INVALID
#define _MM_MASK_INVALID (0u)
#endif
#ifndef _MM_MASK_DIV_ZERO
#define _MM_MASK_DIV_ZERO (0u)
#endif
#ifndef _MM_MASK_OVERFLOW
#define _MM_MASK_OVERFLOW (0u)
#endif

#pragma clang diagnostic pop

// (2) UnrealMathNeon.h's VectorStoreHalf4 passes a float16x4_t to vst1_u8 (which wants uint8x8_t),
//     relying on native arm_neon.h's implicit vector reinterpret. emscripten's strictly-typed SIMDe
//     arm_neon.h rejects that. Provide a lenient overloaded wrapper that reinterprets explicitly, so
//     the NEON/SIMD path is preserved (no scalar fallback).
#if defined(__cplusplus) && PLATFORM_CPU_ARM_FAMILY && defined(SIMPLYSTREAM_SIMD) && SIMPLYSTREAM_SIMD
#pragma push_macro("Nb")
#undef Nb
#include <arm_neon.h>
#pragma pop_macro("Nb")
__attribute__((unused)) static inline void wndr_vst1_u8(uint8_t* Ptr, uint8x8_t Val)   { simde_vst1_u8(Ptr, Val); }
__attribute__((unused)) static inline void wndr_vst1_u8(uint8_t* Ptr, float16x4_t Val) { simde_vst1_u8(Ptr, simde_vreinterpret_u8_f16(Val)); }
#ifdef vst1_u8
#undef vst1_u8
#endif
#define vst1_u8(Ptr, Val) wndr_vst1_u8((uint8_t*)(Ptr), (Val))

//     Same issue: VectorVMPlatformARM.h's VVM_floatToHalf passes a float16x4_t to vst1_s16 (wants int16x4_t).
__attribute__((unused)) static inline void wndr_vst1_s16(int16_t* Ptr, int16x4_t Val)   { simde_vst1_s16(Ptr, Val); }
__attribute__((unused)) static inline void wndr_vst1_s16(int16_t* Ptr, float16x4_t Val) { simde_vst1_s16(Ptr, simde_vreinterpret_s16_f16(Val)); }
#ifdef vst1_s16
#undef vst1_s16
#endif
#define vst1_s16(Ptr, Val) wndr_vst1_s16((int16_t*)(Ptr), (Val))
#endif

// (3) RigLogic/trimd: select the NEON SIMD path on wasm (emscripten maps NEON intrinsics to wasm-simd128 via SIMDe).
//     trimd's platform detection only knows real ARM/x86, so force its ARM family on, enable RigLogic's NEON build,
//     and disable the x86 paths (no real __SSE2__ on wasm) so only the NEON specialization is instantiated. trimd's
//     wasm getCPUFeatures() (reporting NEON) is provided by the toolchain's Platform.h shadow. No scalar fallback.
#ifndef TRIMD_PLATFORM_ARM
#define TRIMD_PLATFORM_ARM 1
#endif
#undef RL_BUILD_WITH_SSE
#undef RL_BUILD_WITH_AVX
#ifndef RL_BUILD_WITH_NEON
#define RL_BUILD_WITH_NEON 1
#endif

#endif // __EMSCRIPTEN__
