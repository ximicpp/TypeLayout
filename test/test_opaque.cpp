// Opaque type registration and is_fixed_enum tests.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;

// Helper: compile-time substring search inside a FixedString.
template <size_t N>
consteval bool contains(const FixedString<N>& haystack, const char* needle) noexcept {
    size_t nlen = 0;
    while (needle[nlen] != '\0') ++nlen;
    if (nlen == 0) return true;
    size_t hlen = haystack.length();
    if (nlen > hlen) return false;
    for (size_t i = 0; i + nlen <= hlen; ++i) {
        bool match = true;
        for (size_t j = 0; j < nlen; ++j) {
            if (haystack.value[i + j] != needle[j]) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

// =========================================================================
// Part 1: Opaque Type Registration Tests
// =========================================================================

namespace opaque_test {
    // Simulate a shared-memory string type (opaque internals)
    struct XString {
        char data_[32];
    };

    // Simulate a shared-memory vector template (sizeof constant for all T)
    template <typename T>
    struct XVector {
        char storage_[24];
    };

    // Simulate a shared-memory map template (sizeof constant for all K,V)
    template <typename K, typename V>
    struct XMap {
        char storage_[48];
    };
} // namespace opaque_test

// Register opaque types — must be in boost::typelayout namespace
namespace boost { namespace typelayout {

TYPELAYOUT_OPAQUE_TYPE(opaque_test::XString, "xstring", 32, 1)
TYPELAYOUT_OPAQUE_CONTAINER(opaque_test::XVector, "xvector", 24, 1)
TYPELAYOUT_OPAQUE_MAP(opaque_test::XMap, "xmap", 48, 1)

}} // namespace boost::typelayout

// --- OPAQUE_TYPE tests ---

// 1. Basic signature format
static_assert(
    TypeSignature<opaque_test::XString, SignatureMode::Layout>::calculate()
        == "xstring[s:32,a:1]",
    "OPAQUE_TYPE Layout signature should be xstring[s:32,a:1]"
);

// 2. Layout and Definition produce identical signatures for opaque
static_assert(
    TypeSignature<opaque_test::XString, SignatureMode::Layout>::calculate()
        == TypeSignature<opaque_test::XString, SignatureMode::Definition>::calculate(),
    "OPAQUE_TYPE: Layout and Definition signatures must be identical"
);

// --- OPAQUE_CONTAINER tests ---

// 3. Container with int32_t element
constexpr auto xvec_i32_layout =
    TypeSignature<opaque_test::XVector<int32_t>, SignatureMode::Layout>::calculate();
static_assert(
    contains(xvec_i32_layout, "xvector[s:24,a:1]<"),
    "OPAQUE_CONTAINER Layout should start with xvector[s:24,a:1]<"
);
static_assert(
    contains(xvec_i32_layout, "i32[s:4,a:4]"),
    "OPAQUE_CONTAINER Layout should contain element signature"
);

// 4. Container mode forwarding — Definition should include enum qualified name
namespace opaque_test {
    enum class Color : uint8_t { Red, Green, Blue };
}

constexpr auto xvec_color_layout =
    TypeSignature<opaque_test::XVector<opaque_test::Color>, SignatureMode::Layout>::calculate();
constexpr auto xvec_color_def =
    TypeSignature<opaque_test::XVector<opaque_test::Color>, SignatureMode::Definition>::calculate();

// Layout: enum without qualified name
static_assert(
    contains(xvec_color_layout, "enum[s:"),
    "OPAQUE_CONTAINER Layout: element enum should not have qualified name"
);
// Definition: enum with qualified name
static_assert(
    contains(xvec_color_def, "enum<"),
    "OPAQUE_CONTAINER Definition: element enum should have qualified name"
);

// 5. Different element types produce different signatures
static_assert(
    !(TypeSignature<opaque_test::XVector<int32_t>, SignatureMode::Layout>::calculate()
        == TypeSignature<opaque_test::XVector<double>, SignatureMode::Layout>::calculate()),
    "OPAQUE_CONTAINER: different element types should produce different signatures"
);

// --- OPAQUE_MAP tests ---

// 6. Map with int32_t key and double value
constexpr auto xmap_sig =
    TypeSignature<opaque_test::XMap<int32_t, double>, SignatureMode::Layout>::calculate();
static_assert(
    contains(xmap_sig, "xmap[s:48,a:1]<"),
    "OPAQUE_MAP should start with xmap[s:48,a:1]<"
);
static_assert(
    contains(xmap_sig, "i32[s:4,a:4]"),
    "OPAQUE_MAP should contain key signature"
);
static_assert(
    contains(xmap_sig, "f64[s:8,a:8]"),
    "OPAQUE_MAP should contain value signature"
);

// 7. Map Layout == Definition for the opaque shell
constexpr auto xmap_layout =
    TypeSignature<opaque_test::XMap<int32_t, int32_t>, SignatureMode::Layout>::calculate();
constexpr auto xmap_def =
    TypeSignature<opaque_test::XMap<int32_t, int32_t>, SignatureMode::Definition>::calculate();
static_assert(
    xmap_layout == xmap_def,
    "OPAQUE_MAP with primitive K,V: Layout and Definition should be identical"
);

// =========================================================================
// Part 2: is_fixed_enum Tests
// =========================================================================

namespace enum_test {
    enum class ScopedFixed : uint32_t { A, B, C };
    enum class ScopedDefault { X, Y, Z };           // defaults to int
    enum UnscopedFixed : int16_t { U1, U2, U3 };
    enum UnscopedImplicit { I1, I2, I3 };            // compiler infers type
}

// 8. Scoped enum with explicit type
static_assert(
    is_fixed_enum<enum_test::ScopedFixed>(),
    "Scoped enum with explicit uint32_t should be fixed"
);

// 9. Scoped enum with default type (int)
static_assert(
    is_fixed_enum<enum_test::ScopedDefault>(),
    "Scoped enum with default int should be fixed"
);

// 10. Unscoped enum with explicit type
static_assert(
    is_fixed_enum<enum_test::UnscopedFixed>(),
    "Unscoped enum with explicit int16_t should be fixed"
);

// 11. Unscoped enum with implicit type — also returns true (documented limitation)
static_assert(
    is_fixed_enum<enum_test::UnscopedImplicit>(),
    "Unscoped enum with implicit type returns true (best-effort, see docs)"
);

// =========================================================================
// Part 3: Integration -- Opaque as field in a normal struct
// =========================================================================
// The Layout engine now checks for opaque TypeSignature specializations
// before recursive flattening (via has_opaque_signature concept), so
// opaque class-type fields are emitted as leaf nodes instead of being
// expanded into their internal byte members.

namespace integration_test {
    struct SharedBlock {
        int32_t id;
        opaque_test::XString name;
        double value;
    };
}

constexpr auto block_layout =
    TypeSignature<integration_test::SharedBlock, SignatureMode::Layout>::calculate();

// 12. Opaque field emitted as leaf in containing struct's Layout signature
static_assert(
    contains(block_layout, "xstring[s:32,a:1]"),
    "Integration: Layout should contain opaque xstring[s:32,a:1] as leaf"
);

// 13. Primitive fields around the opaque are still flattened normally
static_assert(
    contains(block_layout, "i32[s:4,a:4]"),
    "Integration: Layout should contain i32 field"
);
static_assert(
    contains(block_layout, "f64[s:8,a:8]"),
    "Integration: Layout should contain f64 field"
);

// =========================================================================
// Main — runtime confirmation
// =========================================================================

int main() {
    std::cout << "=== Opaque & is_fixed_enum Tests ===\n\n";

    std::cout << "XString Layout:      " << TypeSignature<opaque_test::XString, SignatureMode::Layout>::calculate().value << "\n";
    std::cout << "XVector<i32> Layout: " << xvec_i32_layout.value << "\n";
    std::cout << "XMap<i32,f64> Layout: " << xmap_sig.value << "\n";
    std::cout << "SharedBlock Layout:  " << block_layout.value << "\n";

    std::cout << "\nAll static_assert tests passed at compile time.\n";
    return 0;
}
