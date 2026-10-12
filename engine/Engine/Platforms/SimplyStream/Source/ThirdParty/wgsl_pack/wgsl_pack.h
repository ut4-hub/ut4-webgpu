#pragma once

#include <string>
#include <vector>
#include <array>
#include <string_view>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <charconv>

#ifndef WGSL_PACK_API
#define WGSL_PACK_API
#endif

#if defined(WGSL_PACK_API) && !defined(DLLEXPORT) && !defined(DLLIMPORT)
#undef WGSL_PACK_API
#if defined(__EMSCRIPTEN__)
// wasm is a single monolithic link unit — no cross-module export needed.
#define WGSL_PACK_API
#elif defined(__APPLE__) || defined(__linux__)
// wndr: UE builds each module as its own shared object with -fvisibility=hidden, so the
// public API (pack/unpack/GetToken/...) was emitted GLOBAL-but-HIDDEN and never landed in the
// .so dynamic symbol table — every consumer module (WebGPUShaderFormat) failed to link the
// instant wgsl_pack.cpp was recompiled (cook #1 only linked because it reused W0's cached
// object). Force default visibility so the symbols are actually exported.
#define WGSL_PACK_API __attribute__((visibility("default")))
#elif defined(_WIN32)
#define WGSL_PACK_API __declspec(dllexport)
#else
#define WGSL_PACK_API
#endif
#endif

enum WGSLTokenType : uint16_t {
    WGSL_CHAR,
    WGSL_IDENTIFIER,
    WGSL_SHORTSWIZZLE,
    WGSL_SWIZZLE,
    WGSL_NEWLINE,
    WGSL_NUMBER4,
    WGSL_NUMBER8,
    WGSL_UNSIGNED_NUMBER4,
    WGSL_UNSIGNED_NUMBER8,
    WGSL_FLOAT4,
    WGSL_FLOAT8,
    WGSL_U, WGSL_I,
    WGSL_IN,
    // high frequency types
    WGSL_LET, WGSL_VAR, WGSL_IF, WGSL_DOT, WGSL_FMA,
    WGSL_VEC2, WGSL_VEC3, WGSL_VEC4,
    WGSL_VEC2I, WGSL_VEC3I, WGSL_VEC4I,
    WGSL_VEC2U, WGSL_VEC3U, WGSL_VEC4U,
    WGSL_VEC2F, WGSL_VEC3F, WGSL_VEC4F,
    WGSL_VEC2H, WGSL_VEC3H, WGSL_VEC4H,
    WGSL_BITCAST, WGSL_CLAMP, WGSL_SELECT, WGSL_INVERSE_SQRT,
    // Types
    WGSL_BOOL, WGSL_F32, WGSL_F16, WGSL_I32, WGSL_U32,
    WGSL_ARRAY, WGSL_ATOMIC,
    // Matrix Types
    WGSL_MAT2X2F, WGSL_MAT2X2H, WGSL_MAT2X3F, WGSL_MAT2X3H,
    WGSL_MAT2X4F, WGSL_MAT2X4H, WGSL_MAT3X2F, WGSL_MAT3X2H, WGSL_MAT3X3F,
    WGSL_MAT3X3H, WGSL_MAT3X4F, WGSL_MAT3X4H, WGSL_MAT4X2F, WGSL_MAT4X2H,
    WGSL_MAT4X3F, WGSL_MAT4X3H, WGSL_MAT4X4F, WGSL_MAT4X4H, WGSL_MAT2X2,
    WGSL_MAT2X3, WGSL_MAT2X4, WGSL_MAT3X2, WGSL_MAT3X3, WGSL_MAT3X4,
    WGSL_MAT4X2, WGSL_MAT4X3, WGSL_MAT4X4,
    // Operators
    WGSL_LEFT_PARAM, WGSL_RIGHT_PARAM, WGSL_LEFT_BRACKET, WGSL_RIGHT_BRACKET,
    WGSL_LEFT_ANGLE, WGSL_RIGHT_ANGLE, WGSL_SEMICOLON, WGSL_COMMA, WGSL_PERIOD,
    WGSL_ASSIGN, WGSL_MINUS, WGSL_PLUS, WGSL_MULTIPLY, WGSL_DIVIDE, WGSL_NOT,
    WGSL_OR, WGSL_AND, WGSL_XOR, WGSL_TILDE, WGSL_MODULO, WGSL_QUESTION,
    WGSL_COLON, WGSL_AT, WGSL_HASH, WGSL_DOLLAR, WGSL_BANG, WGSL_PIPE,
    WGSL_AMPERSAND, WGSL_CARET, WGSL_PERCENT, WGSL_LEFT_CURLY, WGSL_RIGHT_CURLY,
    WGSL_UNDERSCORE, WGSL_INCREMENT, WGSL_DECREMENT,
    // Additional
    WGSL_NULLUC, WGSL_SELF, WGSL_ABSTRACT, WGSL_ACTIVE, WGSL_ALIGNAS,
    WGSL_ALIGNOF, WGSL_AS, WGSL_ASM, WGSL_ASM_FRAGMENT, WGSL_ASYNC, WGSL_ATTRIBUTE,
    WGSL_AUTO, WGSL_AWAIT, WGSL_BECOME, WGSL_BINDING_ARRAY, WGSL_CAST, WGSL_CATCH,
    WGSL_CLASS, WGSL_CO_AWAIT, WGSL_CO_RETURN, WGSL_CO_YIELD, WGSL_COHERENT,
    WGSL_COLUMN_MAJOR, WGSL_COMMON, WGSL_COMPILE, WGSL_COMPILE_FRAGMENT,
    WGSL_CONCEPT, WGSL_CONST_CAST, WGSL_CONSTEVAL, WGSL_CONSTEXPR, WGSL_CONSTINIT,
    WGSL_CRATE, WGSL_DEBUGGER, WGSL_DECLTYPE, WGSL_DELETE, WGSL_DEMOTE,
    WGSL_DEMOTE_TO_HELPER, WGSL_DO, WGSL_DYNAMIC_CAST, WGSL_ENUM, WGSL_EXPLICIT,
    WGSL_EXPORT, WGSL_EXTENDS, WGSL_EXTERNAL, WGSL_FILTER, WGSL_FINAL, WGSL_FINALLY,
    WGSL_FRIEND, WGSL_FROM, WGSL_FXGROUP, WGSL_GET, WGSL_GOTO, WGSL_GROUPSHARED,
    WGSL_HIGH, WGSL_IMPLEMENTS, WGSL_IMPORT, WGSL_INLINE, WGSL_INSTANCEOF,
    WGSL_INTERFACE, WGSL_LAYOUT, WGSL_LOW, WGSL_MACRO, WGSL_MATCH, WGSL_MEDIUM,
    WGSL_MODULE, WGSL_MUTABLE, WGSL_NAMESPACE, WGSL_NODISCARD, WGSL_NOEXCEPT,
    WGSL_NOINLINE, WGSL_NOINTERPOLATION, WGSL_NON_COHERENT, WGSL_NONCOHERENT,
    WGSL_NOPERSPECTIVE, WGSL_NULLLC, WGSL_NULLPTR, WGSL_OF, WGSL_OPERATOR,
    WGSL_PACKAGE, WGSL_PACKOFFSET, WGSL_PARTITION, WGSL_PASS, WGSL_PATCH,
    WGSL_PIXELFRAGMENT, WGSL_PRECISE, WGSL_PRECISION, WGSL_PREMERGE, WGSL_PRIV,
    WGSL_PROTECTED, WGSL_PUB, WGSL_PUBLIC, WGSL_READONLY, WGSL_REF, WGSL_REGARDLESS,
    WGSL_REGISTER, WGSL_REINTERPRET_CAST, WGSL_REQUIRE, WGSL_RESOURCE,
    WGSL_RESTRICT, WGSL_SET, WGSL_SHARED, WGSL_SIZEOF, WGSL_SMOOTH, WGSL_SNORM,
    WGSL_STATIC, WGSL_STATIC_CAST, WGSL_STD, WGSL_SUBROUTINE, WGSL_SUPER,
    WGSL_TARGET, WGSL_TEMPLATE, WGSL_THIS, WGSL_THREAD_LOCAL, WGSL_THROW,
    WGSL_TRAIT, WGSL_TRY, WGSL_TYPEDEF, WGSL_TYPEID, WGSL_TYPENAME, WGSL_TYPEOF,
    WGSL_UNION, WGSL_UNLESS, WGSL_UNORM, WGSL_UNSAFE, WGSL_UNSIZED, WGSL_USE,
    WGSL_USING, WGSL_VARYING, WGSL_VIRTUAL, WGSL_VOLATILE, WGSL_WGSL, WGSL_WHERE,
    WGSL_WITH, WGSL_WRITEONLY, WGSL_YIELD, WGSL_LINEAR, WGSL_FLAT, WGSL_PERSPECTIVE,
    // Other Types
    WGSL_PTR, WGSL_SAMPLER, WGSL_SAMPLER_COMPARISON,
    WGSL_TEXTURE_1D, WGSL_TEXTURE_2D, WGSL_TEXTURE_2D_ARRAY, WGSL_TEXTURE_3D,
    WGSL_TEXTURE_CUBE, WGSL_TEXTURE_CUBE_ARRAY, WGSL_TEXTURE_MULTISAMPLED_2D,
    WGSL_TEXTURE_STORAGE_1D, WGSL_TEXTURE_STORAGE_2D, WGSL_TEXTURE_STORAGE_2D_ARRAY,
    WGSL_TEXTURE_STORAGE_3D, WGSL_TEXTURE_DEPTH_2D, WGSL_TEXTURE_DEPTH_2D_ARRAY,
    WGSL_TEXTURE_DEPTH_CUBE, WGSL_TEXTURE_DEPTH_CUBE_ARRAY,
    WGSL_TEXTURE_DEPTH_MULTISAMPLED_2D,
    // Keywords
    WGSL_BREAK, WGSL_CASE, WGSL_CONTINUE, WGSL_CONTINUING, WGSL_DEFAULT,
    WGSL_DISCARD, WGSL_ELSE, WGSL_ENABLE, WGSL_FALLTHROUGH, WGSL_FALSE,
    WGSL_ALIAS, WGSL_CONST, WGSL_DIAGNOSTIC,
    WGSL_FN, WGSL_FOR, WGSL_LOOP, WGSL_OVERRIDE, WGSL_RETURN,
    WGSL_STRUCT, WGSL_SWITCH, WGSL_TRUE, WGSL_TYPE, WGSL_WHILE,
    WGSL_CONST_ASSERT, WGSL_STATIC_ASSERT, WGSL_DIAGNOSTIC_CONTROL, WGSL_REQUIRES,
    // Built-in functions
    WGSL_ABS, WGSL_ACOS, WGSL_ACOSH, WGSL_ALL, WGSL_ANY, WGSL_ARRAY_LENGTH,
    WGSL_ASIN, WGSL_ASINH, WGSL_ATAN, WGSL_ATAN2, WGSL_ATANH,
    WGSL_ATOMIC_ADD, WGSL_ATOMIC_AND, WGSL_ATOMIC_COMPARE_EXCHANGE_WEAK,
    WGSL_ATOMIC_EXCHANGE, WGSL_ATOMIC_LOAD, WGSL_ATOMIC_MAX, WGSL_ATOMIC_MIN,
    WGSL_ATOMIC_OR, WGSL_ATOMIC_STORE, WGSL_ATOMIC_SUB, WGSL_ATOMIC_XOR,
    WGSL_CEIL, WGSL_COS, WGSL_COSH, WGSL_COUNT_LEADING_ZEROS, WGSL_COUNT_ONE_BITS,
    WGSL_COUNT_TRAILING_ZEROS, WGSL_CROSS, WGSL_DEGREES, WGSL_DETERMINANT,
    WGSL_DISTANCE, WGSL_EXP, WGSL_EXP2, WGSL_EXTRACT_BITS, WGSL_FACE_FORWARD,
    WGSL_FIRST_LEADING_BIT, WGSL_FIRST_TRAILING_BIT, WGSL_FLOOR, WGSL_FRAC,
    WGSL_FREXP, WGSL_INSERT_BITS, WGSL_LDEXP, WGSL_LENGTH, WGSL_LOG, WGSL_LOG2,
    WGSL_MAX, WGSL_MIN, WGSL_MIX, WGSL_MODF, WGSL_NORMALIZE, WGSL_POW,
    WGSL_QUANTIZE_TO_F16, WGSL_RADIANS, WGSL_REFLECT, WGSL_REFRACT,
    WGSL_REVERSE_BITS, WGSL_ROUND, WGSL_SATURATE, WGSL_SIGN, WGSL_SIN,
    WGSL_SINH, WGSL_SMOOTHSTEP, WGSL_SQRT, WGSL_STEP, WGSL_TAN, WGSL_TANH,
    WGSL_TRANSPOSE, WGSL_TRUNC, WGSL_UNPACK2X16_FLOAT, WGSL_UNPACK2X16_SNORM,
    WGSL_UNPACK2X16_UNORM, WGSL_UNPACK4X8_SNORM, WGSL_UNPACK4X8_UNORM,
    WGSL_UNPACK4X_I8, WGSL_UNPACK4X_U8, WGSL_PACK2X16_FLOAT, WGSL_PACK2X16_SNORM,
    WGSL_PACK2X16_UNORM, WGSL_PACK4X8_SNORM, WGSL_PACK4X8_UNORM,
    WGSL_PACK4X_I8, WGSL_PACK4X_U8, WGSL_WORKGROUP_BARRIER,
    // Access modes
    WGSL_READ, WGSL_WRITE, WGSL_READ_WRITE,
    // Address spaces
    WGSL_PRIVATE, WGSL_WORKGROUP, WGSL_UNIFORM, WGSL_STORAGE,
    // Builtin values
    WGSL_VERTEX_INDEX, WGSL_INSTANCE_INDEX, WGSL_POSITION, WGSL_FRONT_FACING,
    WGSL_FRAG_DEPTH, WGSL_LOCAL_INVOCATION_ID, WGSL_LOCAL_INVOCATION_INDEX,
    WGSL_GLOBAL_INVOCATION_ID, WGSL_WORKGROUP_ID, WGSL_NUM_WORKGROUPS,
    WGSL_SAMPLE_INDEX, WGSL_SAMPLE_MASK, WGSL_CLIP_DISTANCES,
    WGSL_SUBGROUP_INVOCATION_ID, WGSL_SUBGROUP_SIZE,
    // Attributes
    WGSL_ALIGN, WGSL_BINDING, WGSL_BLEND_SRC, WGSL_BUILTIN, WGSL_COLOR, WGSL_COMPUTE,
    WGSL_FRAGMENT, WGSL_GROUP, WGSL_ID, WGSL_INPUT_ATTACHMENT_INDEX, WGSL_INTERPOLATE,
    WGSL_INVARIANT, WGSL_LOCATION, WGSL_MUST_USE, WGSL_SIZE,
    WGSL_VERTEX, WGSL_WORKGROUP_SIZE,
    // Texture formats
    WGSL_R32UINT, WGSL_R32SINT, WGSL_R32FLOAT, WGSL_R8UNORM, WGSL_RGBA8UNORM, WGSL_RGBA8SNORM,
    WGSL_RGBA8UINT, WGSL_BGRA8UNORM, WGSL_RGBA8SINT, WGSL_RG32UINT, WGSL_RG32SINT,
    WGSL_RG32FLOAT, WGSL_RGBA16UINT, WGSL_RGBA16SINT, WGSL_RGBA16FLOAT,
    WGSL_RGBA32UINT, WGSL_RGBA32SINT, WGSL_RGBA32FLOAT,
    // Texture methods
    WGSL_TEXTURE_STORE, WGSL_TEXTURE_LOAD, WGSL_TEXTURE_SIZE,
    WGSL_TEXTURE_DIMENSIONS, WGSL_TEXTURE_NUM_LAYERS, WGSL_TEXTURE_NUM_LEVELS,
    WGSL_TEXTURE_NUM_SAMPLES, WGSL_TEXTURE_SAMPLE, WGSL_TEXTURE_SAMPLE_BIAS,
    WGSL_TEXTURE_SAMPLE_COMPARE, WGSL_TEXTURE_SAMPLE_COMPARE_LEVEL,
    WGSL_TEXTURE_SAMPLE_GRAD, WGSL_TEXTURE_SAMPLE_LEVEL,
    WGSL_TEXTURE_GATHER, WGSL_TEXTURE_GATHER_COMPARE, WGSL_TEXTURE_GATHER_BIAS,
    WGSL_TEXTURE_GATHER_BIAS_COMPARE, WGSL_TEXTURE_GATHER_COMPARE_LEVEL,
    WGSL_TEXTURE_GATHER_LEVEL,
    // Derivative functions
    WGSL_DPDX, WGSL_DPDY, WGSL_FWIDTH, WGSL_DPDX_COARSE, WGSL_DPDY_COARSE,
    WGSL_FWIDTH_COARSE, WGSL_DPDX_FINE, WGSL_DPDY_FINE, WGSL_FWIDTH_FINE,
    // Special identifiers
    WGSL_MAIN, WGSL_DERIVATIVE_UNIFORMITY, WGSL_SUBGROUP_UNIFORMITY,
    WGSL_READONLY_AND_READWRITE_STORAGE_TEXTURES,
    WGSL_PACKED_4X8_INTEGER_DOT_PRODUCT, WGSL_UNRESTRICTED_POINTER_PARAMETERS,
    WGSL_POINTER_COMPOSITE_ACCESS,
    WGSL_OLD_VALUE,
    WGSL_ERROR, WGSL_WARNING, WGSL_INFO,
    WGSL_NEW, WGSL_NIL, WGSL_NULL, WGSL_MOD,
    WGSL_OFF,
    // Append new serialized tokens here so existing pWGSL token ids stay stable.
    WGSL_WORKGROUP_UNIFORM_LOAD,
    // wndr: standard `enable`-directive extension names (f16/clip_distances live above).
    // Reserving these stops the packer from renaming the extension token after `enable`.
    WGSL_SUBGROUPS, WGSL_DUAL_SOURCE_BLENDING, WGSL_PRIMITIVE_INDEX, WGSL_ATOMIC_VEC2U_MIN_MAX,
    WGSL_TOKEN_MAX = WGSL_ATOMIC_VEC2U_MIN_MAX,
};

// Utility: skip one token (advance offset), returning token type and payload span
struct TokenSpan { WGSLTokenType type; size_t start; size_t end; }; // [start,end)
using IR = std::vector<uint8_t>;

// Context for transforms that need access to original identifier names
struct PackContext {
    std::unordered_map<uint16_t, std::string> id_to_original_name;
};

// Header format - always present
struct WgslPackHeader {
    uint32_t magic;           // 'WGSL' magic number
    uint32_t version;         // Version number
    uint32_t unpacked_size;   // Size of unpacked WGSL text
    uint32_t flags;           // Feature flags
    uint32_t name_count;      // Number of stored names (0 if no names)
    // Followed by name data if name_count > 0: [id(2 bytes)][name_length(2 bytes)][name_data...]
    
    static constexpr uint32_t MAGIC = 0x4C534757; // 'WGSL'
    static constexpr uint32_t VERSION = 2;
    static constexpr uint32_t FLAG_HAS_NAMES = 0x01;
};

// Transform types - use std::function to support lambdas with captures
using IRTransform = std::function<void(const IR& in, IR& out, const PackContext& ctx)>;
using IRTransformInplace = std::function<void(IR&, const PackContext& ctx)>;

#define WGSL_PACK_API_C extern "C" WGSL_PACK_API

// Public API for external code
WGSL_PACK_API IR pack(const std::string_view wgsl, PackContext* out_context = nullptr, bool preserve_names = false);
WGSL_PACK_API void apply_transforms(IR& in_out, const std::vector<IRTransformInplace>& transforms, const PackContext& context);
WGSL_PACK_API IR apply_non_inplace_transforms(const IR& in, const std::vector<IRTransform>& transforms, const PackContext& context);
WGSL_PACK_API_C size_t _unpack(const uint8_t* data, size_t size, char* out, bool restore_names);
WGSL_PACK_API size_t get_unpacked_size(const uint8_t* data, size_t size);  // Just reads from header
WGSL_PACK_API void update_header_size(IR& data);  // Recalculates and updates header size after transforms
WGSL_PACK_API void wgsl_pack_init();

// Helper functions for writing transforms
WGSL_PACK_API TokenSpan PeekOneTokenSpan(const uint8_t* d, size_t size, size_t offset);
WGSL_PACK_API WGSLTokenType GetToken(const uint8_t* d, size_t& offset);
WGSL_PACK_API uint16_t GetVar(const uint8_t* d, size_t& offset);
WGSL_PACK_API void AppendToken(IR& out, WGSLTokenType t);
WGSL_PACK_API void AppendChars(IR& out, std::string_view chars);
WGSL_PACK_API void AppendUnsignedNumber4(IR& out, uint32_t value);
WGSL_PACK_API void AppendUnsignedNumber8(IR& out, uint64_t value);

// Helper to check if tokens have same encoded size (for in-place transforms)
static constexpr size_t GetTokenEncodedSize(WGSLTokenType type) {
    uint16_t v = static_cast<uint16_t>(type);
    return (v < 0x80) ? 1 : 2;
}

// Helper to encode a token in-place (for in-place transforms)
static inline void EncodeTokenInPlace(uint8_t* buf, WGSLTokenType type) {
    uint16_t v = static_cast<uint16_t>(type);
    if (v < 0x80) {
        buf[0] = static_cast<uint8_t>(v);
    } else {
        buf[0] = static_cast<uint8_t>(0x80 | (v & 0x7F));
        buf[1] = static_cast<uint8_t>((v >> 7) & 0xFF);
    }
}

class WGSL_PACK_API WgslPack {
public:
	WgslPack() {
		wgsl_pack_init();
	}
	IR Pack(const std::string_view wgsl, PackContext* context = nullptr, bool preserve_names = false) {
		return ::pack(wgsl, context, preserve_names);
	}
	size_t Unpack(const uint8_t* data, size_t size, char* out) {
		return ::_unpack(data, size, out, true);
	}
	size_t GetUnpackedSize(const uint8_t* data, size_t size) {
		return ::get_unpacked_size(data, size);
	}
	size_t GetUnpackedSize(const IR& data) {
		return ::get_unpacked_size(data.data(), data.size());
	}
};

// Utility: Fast circular buffer for token history
template<size_t N>
struct TokenHistory {
    struct Entry { WGSLTokenType type; uint16_t id; };
    Entry history[N] = {};
    int idx = 0;

    void push(WGSLTokenType type, uint16_t id = 0) {
        history[idx] = { type, id };
        idx = (idx + 1) % N;
    }

    Entry& operator[](int back) { return history[(idx + N - 1 - back) % N]; }
    const Entry& operator[](int back) const { return history[(idx + N - 1 - back) % N]; }
};

// Utility: Check if identifier name matches target set
inline bool isTargetIdentifier(uint16_t id, const PackContext& ctx, const std::unordered_set<std::string>& targets) {
    auto it = ctx.id_to_original_name.find(id);
    return it != ctx.id_to_original_name.end() && targets.count(it->second) > 0;
}

// Pattern matching DSL - Zero-overhead, no std::function
struct SingleToken {
    WGSLTokenType token;
    explicit SingleToken(WGSLTokenType t) : token(t) {}
    bool matches(const typename TokenHistory<5>::Entry& entry) const {
        return entry.type == token;
    }
};

struct TokenSet {
    std::initializer_list<WGSLTokenType> tokens;
    TokenSet(std::initializer_list<WGSLTokenType> t) : tokens(t) {}
    bool matches(const typename TokenHistory<5>::Entry& entry) const {
        return std::find(tokens.begin(), tokens.end(), entry.type) != tokens.end();
    }
};

struct IdentifierCheck {
    const PackContext* ctx;
    const std::unordered_set<std::string>* targets;
    IdentifierCheck(const PackContext& c, const std::unordered_set<std::string>& t) : ctx(&c), targets(&t) {}
    bool matches(const typename TokenHistory<5>::Entry& entry) const {
        return entry.type == WGSL_IDENTIFIER && isTargetIdentifier(entry.id, *ctx, *targets);
    }
};

inline bool isNumberToken(WGSLTokenType type) {
	return type == WGSL_NUMBER4 || type == WGSL_NUMBER8 || type == WGSL_UNSIGNED_NUMBER4 || type == WGSL_UNSIGNED_NUMBER8;
}
struct NumberCheck {
    uint8_t target_value;
    NumberCheck(uint8_t val) : target_value(val) {}
    bool matches(const typename TokenHistory<5>::Entry& entry) const {
        return (isNumberToken(entry.type)) &&
            entry.id == target_value;
    }
};

// Variant-like pattern element
struct TokenPattern {
    enum Type { SINGLE, SET, ID_CHECK, NUMBER_CHECK } type;
    union {
        SingleToken single;
        TokenSet set;
        IdentifierCheck id_check;
        NumberCheck number_check;
    };

    TokenPattern(WGSLTokenType t) : type(SINGLE), single(t) {}
    TokenPattern(std::initializer_list<WGSLTokenType> tokens) : type(SET), set(tokens) {}
    TokenPattern(const IdentifierCheck& check) : type(ID_CHECK), id_check(check) {}
    TokenPattern(const NumberCheck& num_check) : type(NUMBER_CHECK), number_check(num_check) {}

    // Copy constructor
    TokenPattern(const TokenPattern& other) : type(other.type) {
        switch (type) {
        case SINGLE: single = other.single; break;
        case SET: set = other.set; break;
        case ID_CHECK: id_check = other.id_check; break;
        case NUMBER_CHECK: number_check = other.number_check; break;
        }
    }

    // Destructor (unions with non-trivial members need explicit handling)
    ~TokenPattern() {
        // All our union members are trivially destructible
    }

    bool matches(const typename TokenHistory<5>::Entry& entry) const {
        switch (type) {
        case SINGLE: return single.matches(entry);
        case SET: return set.matches(entry);
        case ID_CHECK: return id_check.matches(entry);
        case NUMBER_CHECK: return number_check.matches(entry);
        }
        return false;
	}
};

// Simple pattern matcher - works with any TokenHistory size
template<size_t N>
bool matches(const TokenHistory<N>& hist, std::initializer_list<TokenPattern> pattern) {
    if (pattern.size() > N) return false;

    int i = 0;
    for (const auto& p : pattern) {
        if (!p.matches(hist[i++])) return false;
    }
    return true;
}

// Overload for the specific TokenHistory<5> we're using
inline bool matches(const TokenHistory<5>& hist, std::initializer_list<TokenPattern> pattern) {
    if (pattern.size() > 5) return false;

    int i = 0;
    for (const auto& p : pattern) {
        if (!p.matches(hist[i++])) return false;
    }
    return true;
}

// Overload for vector<TokenPattern>
inline bool matches(const TokenHistory<5>& hist, const std::vector<TokenPattern>& pattern) {
    if (pattern.size() > 5) return false;

    int i = 0;
    for (const auto& p : pattern) {
        if (!p.matches(hist[i++])) return false;
    }
    return true;
}

// Helper functions for readability
inline TokenPattern anyOf(std::initializer_list<WGSLTokenType> tokens) {
    return TokenPattern(tokens);
}

inline TokenPattern targetId(const PackContext& ctx, const std::unordered_set<std::string>& targets) {
    return TokenPattern(IdentifierCheck(ctx, targets));
}

inline TokenPattern groupNumber(uint8_t value) {
    return TokenPattern(NumberCheck(value));
}

inline TokenPattern bindingNumber(uint8_t value) {
    return TokenPattern(NumberCheck(value));
}

// Transform to convert Tint's verbose tint_pad_* fields back to concise @size attributes
static auto createPaddingToSizeTransform() {
    return [](const IR& in, IR& out, const PackContext& ctx) {
        const uint8_t* d = in.data();
        size_t off = 0;
        
        while (off < in.size()) {
            auto span = PeekOneTokenSpan(d, in.size(), off);
            
            if (span.type != WGSL_STRUCT) {
                    out.insert(out.end(), d + span.start, d + span.end);
                off = span.end;
                continue;
            }

            size_t struct_start_off = off;
            
            // 1. Find the start and end of the struct body.
            size_t body_start_off = 0, struct_end_off = 0;
            size_t find_body_off = span.end;
            while (find_body_off < in.size()) {
                auto s = PeekOneTokenSpan(d, in.size(), find_body_off);
                if (s.type == WGSL_LEFT_CURLY) { body_start_off = s.end; break; }
                if (s.type == WGSL_SEMICOLON) { break; } // Forward declaration.
                find_body_off = s.end;
            }

            if (body_start_off == 0) { // No body found, copy original token and continue.
                    out.insert(out.end(), d + span.start, d + span.end);
                off = span.end;
                continue;
                }
                
            // 2. Pre-scan the body to find the max padding offset.
            uint32_t max_offset = 0;
            bool has_padding = false;
            size_t scan_off = body_start_off;
            int depth = 1;
            while (scan_off < in.size() && depth > 0) {
                auto s = PeekOneTokenSpan(d, in.size(), scan_off);
                if (s.type == WGSL_LEFT_CURLY) depth++;
                else if (s.type == WGSL_RIGHT_CURLY) {
                    if (--depth == 0) struct_end_off = s.start;
                }
                    
                if (s.type == WGSL_IDENTIFIER) {
                    size_t id_off = s.start;
                    GetToken(d, id_off);
                    auto it = ctx.id_to_original_name.find(GetVar(d, id_off));
                    if (it != ctx.id_to_original_name.end() && it->second.starts_with("tint_pad_")) {
                        uint32_t field_offset;
                        std::string_view offset_sv = std::string_view(it->second).substr(9);
                        if (std::from_chars(offset_sv.data(), offset_sv.data() + offset_sv.size(), field_offset).ec == std::errc()) {
                            has_padding = true;
                            max_offset = std::max(max_offset, field_offset);
                        }
                    }
                }
                scan_off = s.end;
            }
                                    
            // 3. Reconstruct the struct.
            out.insert(out.end(), d + struct_start_off, d + body_start_off);
                                    
            if (has_padding) {
                uint32_t struct_size = max_offset + 4;
                                    AppendToken(out, WGSL_AT);
                                    AppendToken(out, WGSL_SIZE);
                                    AppendToken(out, WGSL_LEFT_PARAM);
                AppendUnsignedNumber4(out, struct_size);
                                    AppendToken(out, WGSL_RIGHT_PARAM);
				AppendChars(out, "_pad");
                AppendToken(out, WGSL_COLON);
                                    AppendToken(out, WGSL_U32);
                                    AppendToken(out, WGSL_COMMA);
                            }
                            
            // 4. Process body, copying non-padding fields.
            scan_off = body_start_off;
            while (scan_off < struct_end_off) {
                auto s = PeekOneTokenSpan(d, in.size(), scan_off);
                bool is_padding_field = false;
                if (s.type == WGSL_IDENTIFIER) {
                    size_t id_off = s.start;
                    GetToken(d, id_off);
                    auto it = ctx.id_to_original_name.find(GetVar(d, id_off));
                    is_padding_field = (it != ctx.id_to_original_name.end() && it->second.starts_with("tint_pad_"));
                            }
                            
                if (is_padding_field) {
                    size_t next_off = s.end;
                    depth = 0;
                    while (next_off < in.size()) {
                        auto skip_s = PeekOneTokenSpan(d, in.size(), next_off);
                        if (skip_s.type == WGSL_LEFT_PARAM || skip_s.type == WGSL_LEFT_BRACKET || skip_s.type == WGSL_LEFT_ANGLE) depth++;
                        else if (skip_s.type == WGSL_RIGHT_PARAM || skip_s.type == WGSL_RIGHT_BRACKET || skip_s.type == WGSL_RIGHT_ANGLE) depth--;
                        else if (depth == 0 && (skip_s.type == WGSL_COMMA || skip_s.type == WGSL_SEMICOLON)) { next_off = skip_s.end; break; }
                        else if (depth == 0 && skip_s.type == WGSL_RIGHT_CURLY) { next_off = skip_s.start; break; }
                        next_off = skip_s.end;
                    }
                    scan_off = next_off;
                } else {
                    out.insert(out.end(), d + s.start, d + s.end);
                    scan_off = s.end;
                }
            }
            
            // 5. Copy the final brace and advance main offset.
            out.insert(out.end(), d + struct_end_off, d + scan_off);
            off = scan_off;
        }
    };
}

template<WGSLTokenType FROM, WGSLTokenType TO>
inline void replaceTokenInPlace(uint8_t* data, size_t offset);

// Advanced pattern builder for ultra-concise transforms
template<WGSLTokenType FROM, WGSLTokenType TO>
class PatternTransformBuilder {
    std::vector<TokenPattern> pattern_;
    const PackContext* ctx_ = nullptr;

public:
    PatternTransformBuilder& when(std::initializer_list<TokenPattern> p) {
        pattern_ = p;
        return *this;
    }

    PatternTransformBuilder& withContext(const PackContext& ctx) {
        ctx_ = &ctx;
        return *this;
    }

    void apply(IR& in_out) const {
        uint8_t* d = in_out.data();
        TokenHistory<5> hist;
        size_t off = 0;

        while (off < in_out.size()) {
            size_t tokStart = off;
            auto span = PeekOneTokenSpan(d, in_out.size(), off);

            uint16_t id = 0;
            if (span.type == WGSL_IDENTIFIER) {
                size_t id_off = span.start;
                GetToken(d, id_off);
                id = GetVar(d, id_off);
            }

            // Check pattern BEFORE pushing current token to history
            if (span.type == FROM) {
                // Push current token to create complete pattern
                hist.push(span.type, id);

                if (matches(hist, pattern_)) {
                    replaceTokenInPlace<FROM, TO>(d, tokStart);
                }
            }
            else {
                // For non-target tokens, just update history
                hist.push(span.type, id);
            }

            off = span.end;
        }
    }
};

// Factory function for the builder
template<WGSLTokenType FROM, WGSLTokenType TO>
PatternTransformBuilder<FROM, TO> replace() {
    return PatternTransformBuilder<FROM, TO>();
}

// Specialized pattern builder for number value replacement - ultra-concise
template<uint8_t FROM_VAL, uint8_t TO_VAL>
class NumberReplaceBuilder {
    std::vector<TokenPattern> pattern_;
    
public:
    NumberReplaceBuilder& when(std::initializer_list<TokenPattern> p) {
        pattern_ = p;
        return *this;
    }
    
    void apply(IR& in_out) const {
        uint8_t* d = in_out.data();
        TokenHistory<5> hist;
        size_t off = 0;
        
        while (off < in_out.size()) {
            size_t tokStart = off;
            auto span = PeekOneTokenSpan(d, in_out.size(), off);
            
            uint8_t number_value = 0;
            if (isNumberToken(span.type)) {
                size_t num_off = span.start;
                GetToken(d, num_off);
                number_value = d[num_off];
            }
            
            hist.push(span.type, number_value);
            
            if (matches(hist, pattern_)) {
                size_t num_off = tokStart;
                GetToken(d, num_off);
                d[num_off] = TO_VAL;
            }
            
            off = span.end;
        }
    }
};

// Factory for number replacement
template<uint8_t FROM_VAL, uint8_t TO_VAL>
NumberReplaceBuilder<FROM_VAL, TO_VAL> replaceNumber() {
    return NumberReplaceBuilder<FROM_VAL, TO_VAL>();
}

// Runtime version of number replacement - ultra-concise!
class RuntimeNumberReplaceBuilder {
    std::vector<TokenPattern> pattern_;
    uint8_t from_val_, to_val_;
    
public:
    RuntimeNumberReplaceBuilder(uint8_t from_val, uint8_t to_val) 
        : from_val_(from_val), to_val_(to_val) {}
    
    RuntimeNumberReplaceBuilder& when(std::initializer_list<TokenPattern> p) {
        pattern_ = p;
        return *this;
    }
    
    void apply(IR& in_out) const {
        uint8_t* d = in_out.data();
        TokenHistory<5> hist;
        size_t off = 0;
        
        while (off < in_out.size()) {
            size_t tokStart = off;
            auto span = PeekOneTokenSpan(d, in_out.size(), off);
            
            uint8_t number_value = 0;
            if (isNumberToken(span.type)) {
                size_t num_off = span.start;
                GetToken(d, num_off);
                number_value = d[num_off];
            }
            
            hist.push(span.type, number_value);
            
            if (matches(hist, pattern_)) {
                size_t num_off = tokStart;
                GetToken(d, num_off);
                d[num_off] = to_val_;
            }
            
            off = span.end;
        }
    }
};

// Factory for runtime number replacement  
inline RuntimeNumberReplaceBuilder replaceNumber(uint8_t from_val, uint8_t to_val) {
    return RuntimeNumberReplaceBuilder(from_val, to_val);
}

// Utility: Simple in-place token replacement with compile-time validation
template<WGSLTokenType FROM, WGSLTokenType TO>
inline void replaceTokenInPlace(uint8_t* data, size_t offset) {
    static_assert(GetTokenEncodedSize(FROM) == GetTokenEncodedSize(TO));
    EncodeTokenInPlace(data + offset, TO);
}

// Constexpr-friendly binding map optimized for 0-100 bindings
struct BindingMap {
    std::array<std::pair<uint8_t, uint8_t>, 100> pairs{};
    size_t count = 0;
    
    constexpr BindingMap(std::initializer_list<std::pair<uint8_t, uint8_t>> init_list) {
        for (const auto& pair : init_list) {
            if (count < 100) {
                pairs[count++] = pair;
            }
        }
        std::sort(pairs.begin(), pairs.begin() + count);
    }

    BindingMap(const std::pair<uint8_t, uint8_t>* data, size_t size) {
        for (size_t i = 0; i < size && i < 100; ++i) {
            pairs[count++] = data[i];
        }
        std::sort(pairs.begin(), pairs.begin() + count);
    }
    
    constexpr uint8_t find(uint8_t key) const noexcept {
        // Binary search is better for larger arrays (50+ elements)
        if (count == 0) return 255;
        
        auto it = std::lower_bound(pairs.begin(), pairs.begin() + count, 
            std::make_pair(key, uint8_t(0)));
        if (it != pairs.begin() + count && it->first == key) {
            return it->second;
        }
        return 255; // Not found sentinel
    }
};

static auto createBindingBatchRemapTransform(const BindingMap& binding_map) {
    return [binding_map](IR& in_out, const PackContext& ctx) {
        uint8_t* d = in_out.data();
        TokenHistory<5> hist;
        size_t off = 0;

        while (off < in_out.size()) {
            size_t tokStart = off;
            auto span = PeekOneTokenSpan(d, in_out.size(), off);

            uint8_t number_value = 0;
            if (isNumberToken(span.type)) {
                size_t num_off = span.start;
                GetToken(d, num_off);
                number_value = d[num_off];
            }

            hist.push(span.type, number_value);

            // Check if this binding needs remapping
            uint8_t mapped_value = binding_map.find(number_value);
            if (mapped_value != 255 &&
                matches(hist, {
                    anyOf({WGSL_NUMBER4, WGSL_NUMBER8, WGSL_UNSIGNED_NUMBER4, WGSL_UNSIGNED_NUMBER8}), // [0] any binding number
                    WGSL_LEFT_PARAM,                     // [1] (
                    WGSL_BINDING,                        // [2] binding  
                    WGSL_AT                              // [3] @
                    })) {
                size_t num_off = tokStart;
                GetToken(d, num_off);
                d[num_off] = mapped_value; // Replace with mapped value
            }

            off = span.end;
        }
        };
}

// Helper function to create BindingMap from initializer list
constexpr auto makeBindingMap(std::initializer_list<std::pair<uint8_t, uint8_t>> pairs) {
    return BindingMap(pairs);
}

static auto createAttributeLocationBatchRemapTransform(const BindingMap& location_map) {
    return [location_map](IR& in_out, const PackContext& ctx) {
        uint8_t* d = in_out.data();
        size_t off = 0;
        
        // First pass: Find "fn main(" - much faster without history tracking
        while (off < in_out.size()) {
            auto span = PeekOneTokenSpan(d, in_out.size(), off);
            
            if (span.type == WGSL_LEFT_PARAM) {
                // Check if previous tokens are "main" and "fn"
                size_t temp_off = 0;
                TokenHistory<3> temp_hist;
                
                while (temp_off <= off) {
                    auto temp_span = PeekOneTokenSpan(d, in_out.size(), temp_off);
                    
                        temp_hist.push(temp_span.type, 0);
                    
                    if (temp_off == off) break;
                    temp_off = temp_span.end;
                }
                
                // Check pattern: fn, main, (
                if (temp_hist[2].type == WGSL_FN && 
                    temp_hist[1].type == WGSL_MAIN &&
                    temp_hist[0].type == WGSL_LEFT_PARAM) {
                        off = span.end;
                        break;
                    }
                }
            
            off = span.end;
        }
        
        // Second pass: Remap locations only after "fn main("
        TokenHistory<5> hist;
        while (off < in_out.size()) {
            size_t tokStart = off;
            auto span = PeekOneTokenSpan(d, in_out.size(), off);

            uint8_t number_value = 0;
            if (span.type == WGSL_NUMBER4 || span.type == WGSL_NUMBER8 || 
                span.type == WGSL_UNSIGNED_NUMBER4 || span.type == WGSL_UNSIGNED_NUMBER8) {
                size_t num_off = span.start;
                GetToken(d, num_off);
                number_value = d[num_off];
            }

            hist.push(span.type, number_value);

            // Check if this location needs remapping
            uint8_t mapped_value = location_map.find(number_value);
            if (mapped_value != 255 &&
                matches(hist, {
                    anyOf({WGSL_NUMBER4, WGSL_NUMBER8, WGSL_UNSIGNED_NUMBER4, WGSL_UNSIGNED_NUMBER8}), // [0] location number
                    WGSL_LEFT_PARAM,                     // [1] (
                    WGSL_LOCATION,                       // [2] location  
                    WGSL_AT                              // [3] @
                    })) {
                size_t num_off = tokStart;
                GetToken(d, num_off);
                d[num_off] = mapped_value; // Replace with mapped value
            }

            off = span.end;
        }
    };
}

static auto createGroupRemapTransform(uint8_t from_group, uint8_t to_group) {
    return [from_group, to_group](IR& in_out, const PackContext& ctx) {
        replaceNumber(from_group, to_group)
            .when({
                groupNumber(from_group),  // [0] target group number
                WGSL_LEFT_PARAM,          // [1] (
                WGSL_GROUP,               // [2] group
                WGSL_AT                   // [3] @
                })
            .apply(in_out);
        };
}

static auto createBindingRemapTransform(uint8_t from_binding, uint8_t to_binding) {
    return [from_binding, to_binding](IR& in_out, const PackContext& ctx) {
        replaceNumber(from_binding, to_binding)
            .when({
                bindingNumber(from_binding),  // [0] target binding number
                WGSL_LEFT_PARAM,              // [1] (
                WGSL_BINDING,                 // [2] binding
                WGSL_AT                       // [3] @
                })
            .apply(in_out);
        };
}

static auto createLocationReorderTransform() {
    return [](IR& in_out, const PackContext& ctx) {
        uint8_t* d = in_out.data();
        TokenHistory<5> hist;
        size_t off = 0;
        int appearance_order = 0;

        while (off < in_out.size()) {
            size_t tokStart = off;
            auto span = PeekOneTokenSpan(d, in_out.size(), off);

            hist.push(span.type, 0);

            // Ultra-concise pattern: ANY @location number gets replaced with order
            if ((span.type == WGSL_NUMBER4 || span.type == WGSL_NUMBER8) &&
                matches(hist, {
                    anyOf({WGSL_NUMBER4, WGSL_NUMBER8}), // [0] any number
                    WGSL_LEFT_PARAM,                     // [1] (
                    WGSL_LOCATION,                       // [2] location
                    WGSL_AT                              // [3] @
                    })) {
                size_t num_off = tokStart;
                GetToken(d, num_off);
                d[num_off] = static_cast<uint8_t>(appearance_order++);
            }

            off = span.end;
        }
        };
}

static auto createLocationRemapTransform(const std::unordered_set<int>& target_locations) {
    return [target_locations](IR& in_out, const PackContext& ctx) {
        uint8_t* d = in_out.data();
        TokenHistory<5> hist;
        size_t off = 0;
        int new_index = 0;

        while (off < in_out.size()) {
            size_t tokStart = off;
            auto span = PeekOneTokenSpan(d, in_out.size(), off);

            uint8_t number_value = 0;
            if (span.type == WGSL_NUMBER4 || span.type == WGSL_NUMBER8) {
                size_t num_off = span.start;
                GetToken(d, num_off);
                number_value = d[num_off];
            }

            hist.push(span.type, number_value);

            // Concise pattern matching with target filtering
            if ((span.type == WGSL_NUMBER4 || span.type == WGSL_NUMBER8) &&
                target_locations.count(static_cast<int>(number_value)) > 0 &&
                matches(hist, {
                    anyOf({WGSL_NUMBER4, WGSL_NUMBER8}), // [0] any number
                    WGSL_LEFT_PARAM,                     // [1] (
                    WGSL_LOCATION,                       // [2] location
                    WGSL_AT                              // [3] @
                    })) {
                size_t num_off = tokStart;
                GetToken(d, num_off);
                d[num_off] = static_cast<uint8_t>(new_index++);
            }

            off = span.end;
        }
        };
}

struct TokenBitset {
    static constexpr size_t kTokenCount = static_cast<size_t>(WGSL_TOKEN_MAX) + 1;
    std::array<uint64_t, (kTokenCount + 63) / 64> bits{};  // Support entire token enum
    constexpr void set(WGSLTokenType t) {
        const size_t idx = static_cast<size_t>(t);
        bits[idx >> 6] |= (1ull << (idx & 63));
    }
    constexpr bool has(WGSLTokenType t) const {
        const size_t idx = static_cast<size_t>(t);
        return (bits[idx >> 6] >> (idx & 63)) & 1ull;
    }
};


// Pattern: <non-excluded-format>, read_write  -->  <non-excluded-format>, write
static auto createNarrowReadWriteTransform(std::initializer_list<WGSLTokenType> excluded_formats = {
    WGSL_R32FLOAT, WGSL_R32UINT, WGSL_R32SINT  // Default excluded formats
    }) {
    // Build exclusion bitset at compile time if possible
    TokenBitset excluded{};
    for (auto fmt : excluded_formats) {
        excluded.set(fmt);
    }

    return [excluded](IR& in_out, const PackContext& ctx) {
        static_assert(GetTokenEncodedSize(WGSL_READ_WRITE) == GetTokenEncodedSize(WGSL_WRITE));

        uint8_t* d = in_out.data();
        TokenHistory<3> hist;
        size_t off = 0;

        while (off < in_out.size()) {
            size_t tokStart = off;
            auto span = PeekOneTokenSpan(d, in_out.size(), off);

            // Check pattern BEFORE updating history
            // Pattern: format, comma, read_write -> format, comma, write
			if (span.type == WGSL_READ_WRITE) {
                // hist[0] is previous token (comma), hist[1] is token before that (format)
                if (hist[0].type == WGSL_COMMA) {
                    const WGSLTokenType format_token = hist[1].type;
                    if (format_token >= WGSL_R32UINT && format_token <= WGSL_RGBA32FLOAT) {
                        if (!excluded.has(format_token)) {
						EncodeTokenInPlace(d + tokStart, WGSL_WRITE);
					}
				}
			}
            }

            hist.push(span.type, 0);
            off = span.end;
        }
        };
}

template<WGSLTokenType FROM, WGSLTokenType TO>
static auto createTextureFormatTransform(const std::unordered_set<std::string>& targets) {
    return [targets](IR& in_out, const PackContext& ctx) {
        replace<FROM, TO>()
            .when({
                FROM,                                             // [0] current token
                WGSL_LEFT_ANGLE,                                  // [1] previous token
                anyOf({WGSL_TEXTURE_STORAGE_3D, WGSL_TEXTURE_STORAGE_2D, WGSL_TEXTURE_STORAGE_2D_ARRAY}), // [2]
                WGSL_COLON,                                       // [3] 
                targetId(ctx, targets)                            // [4] oldest token
                })
            .apply(in_out);
        };
}
