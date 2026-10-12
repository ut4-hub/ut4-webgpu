#pragma once

// Tokens emitted inside EM_ASM blocks. Pointer-sized C++ values use BigInt in
// MEMORY64 modes and Number in native wasm32; genuine uint64 values always use
// BigInt when WASM_BIGINT is enabled.
#if defined(MEMORY64) && MEMORY64
#define WNDR_JS_POINTER(Value) BigInt(Value)
#define WNDR_JS_SIZE_T(Value) BigInt(Value)
#else
#define WNDR_JS_POINTER(Value) Number(Value)
#define WNDR_JS_SIZE_T(Value) Number(Value)
#endif

#define WNDR_JS_UINT64(Value) BigInt(Value)
