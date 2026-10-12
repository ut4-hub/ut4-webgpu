// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Current Emscripten exposes C true/false macros. The bundled RAD Audio sources
// declare their own C enum with those names, as expected by the original build.
// Include Emscripten once, then remove only those convenience macros before the
// RAD headers are parsed.
#include <emscripten.h>

#ifdef false
#undef false
#endif

#ifdef true
#undef true
#endif

#ifndef __popcnt
#define __popcnt(Value) __builtin_popcount(Value)
#endif

// RAD's bundled platform header identifies Emscripten as little-endian but has
// no Emscripten branch for the non-native endian helpers or prefetch policy.
#ifndef RR_BSWAP16
#define RR_BSWAP16(Value) __builtin_bswap16(Value)
#endif

#ifndef RR_BSWAP32
#define RR_BSWAP32(Value) __builtin_bswap32(Value)
#endif

#ifndef RR_BSWAP64
#define RR_BSWAP64(Value) __builtin_bswap64(Value)
#endif

#ifndef RR_GET16_BE
#define RR_GET16_BE(Pointer) RR_BSWAP16(*((U16 *)(Pointer)))
#endif

#ifndef RR_GET16_BE_OFFSET
#define RR_GET16_BE_OFFSET(Pointer, Offset) RR_BSWAP16(*RR_U16_PTR_OFFSET(Pointer, Offset))
#endif

#ifndef RR_GET32_BE
#define RR_GET32_BE(Pointer) RR_BSWAP32(*((U32 *)(Pointer)))
#endif

#ifndef RR_GET32_BE_OFFSET
#define RR_GET32_BE_OFFSET(Pointer, Offset) RR_BSWAP32(*RR_U32_PTR_OFFSET(Pointer, Offset))
#endif

#ifndef RR_GET64_BE
#define RR_GET64_BE(Pointer) RR_BSWAP64(*((U64 *)(Pointer)))
#endif

#ifndef RR_GET64_BE_OFFSET
#define RR_GET64_BE_OFFSET(Pointer, Offset) RR_BSWAP64(*RR_U64_PTR_OFFSET(Pointer, Offset))
#endif

#ifndef RR_PUT16_BE
#define RR_PUT16_BE(Pointer, Value) (*((U16 *)(Pointer)) = (U16)RR_BSWAP16(Value))
#endif

#ifndef RR_PUT16_BE_OFFSET
#define RR_PUT16_BE_OFFSET(Pointer, Value, Offset) (*RR_U16_PTR_OFFSET(Pointer, Offset) = (U16)RR_BSWAP16(Value))
#endif

#ifndef RR_PUT32_BE
#define RR_PUT32_BE(Pointer, Value) (*((U32 *)(Pointer)) = (U32)RR_BSWAP32(Value))
#endif

#ifndef RR_PUT32_BE_OFFSET
#define RR_PUT32_BE_OFFSET(Pointer, Value, Offset) (*RR_U32_PTR_OFFSET(Pointer, Offset) = (U32)RR_BSWAP32(Value))
#endif

#ifndef RR_PUT64_BE
#define RR_PUT64_BE(Pointer, Value) (*((U64 *)(Pointer)) = (U64)RR_BSWAP64(Value))
#endif

#ifndef RR_PUT64_BE_OFFSET
#define RR_PUT64_BE_OFFSET(Pointer, Value, Offset) (*RR_U64_PTR_OFFSET(Pointer, Offset) = (U64)RR_BSWAP64(Value))
#endif

#ifndef RR_PREFETCH_SEQUENTIAL
#define RR_PREFETCH_SEQUENTIAL(Pointer, Offset)
#endif

#ifndef RR_PREFETCH_WRITE_INVALIDATE
#define RR_PREFETCH_WRITE_INVALIDATE(Pointer, Offset)
#endif
