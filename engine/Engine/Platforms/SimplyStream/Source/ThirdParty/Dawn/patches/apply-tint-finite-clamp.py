#!/usr/bin/env python3
# Strata Tint patch: clamp non-finite scalar constants instead of aborting.
#
# UE materials can const-fold to non-finite values (NaN/Inf) which WGSL const-
# expressions forbid. Tint's Scalar<T> ctor did TINT_ASSERT(std::isfinite(...))
# which abort()s the process (SIGILL) and kills the entire WebGPU cook (seen
# cooking Lyra's materials for SP_WEBGPU_ES31/SM5). This clamps NaN->0 and
# +/-Inf->+/-max so the shader compiles with a finite constant instead.
#
# Apply to a Dawn checkout before building libtint.a:
#   python3 apply-tint-finite-clamp.py [/path/to/dawn]   (default: ~/dawn)
# Idempotent: a no-op if already applied.
import os, re, sys

root = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser("~/dawn")
p = os.path.join(root, "src/tint/lang/core/constant/scalar.h")
s = open(p).read()
if "SanitizeFinite" in s:
    print("already patched:", p)
    sys.exit(0)

guard = "#define SRC_TINT_LANG_CORE_CONSTANT_SCALAR_H_\n"
if "#include <limits>" not in s:
    s = s.replace(guard, guard + "\n#include <cmath>\n#include <limits>\n", 1)

pat = re.compile(
    r"    /// Constructor\n"
    r"    /// @param t the scalar type\n"
    r"    /// @param v the scalar value\n"
    r"    Scalar\(const core::type::Type\* t, T v\) : type\(t\), value\(v\) \{\n"
    r"        if constexpr \(IsFloatingPoint<T>\) \{\n"
    r"            TINT_ASSERT\(std::isfinite\(v\.value\)\);\n"
    r"        \}\n"
    r"    \}\n"
)
m = pat.search(s)
assert m, "stock Scalar ctor with TINT_ASSERT(isfinite) not found — Dawn version drift?"
new = '''    /// Strata: UE materials can const-fold to non-finite values (NaN/Inf) that
    /// WGSL const-exprs forbid; Tint's ctor asserted isfinite and aborted the
    /// whole cook. Clamp to finite BEFORE storing (value is const).
    static T SanitizeFinite(T v) {
        if constexpr (IsFloatingPoint<T>) {
            if (!std::isfinite(v.value)) {
                using ElTy = decltype(v.value);
                v.value = std::isnan(v.value)
                    ? ElTy(0)
                    : (v.value > ElTy(0) ? std::numeric_limits<ElTy>::max()
                                         : std::numeric_limits<ElTy>::lowest());
            }
        }
        return v;
    }

    /// Constructor
    /// @param t the scalar type
    /// @param v the scalar value
    Scalar(const core::type::Type* t, T v) : type(t), value(SanitizeFinite(v)) {}
'''
s = s[: m.start()] + new + s[m.end():]
open(p, "w").write(s)
print("patched:", p)
