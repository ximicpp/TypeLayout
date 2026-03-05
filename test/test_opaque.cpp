// Opaque type registration and is_fixed_enum tests.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include "test_util.hpp"
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;
using boost::typelayout::test::contains;

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
    TypeSignature<opaque_test::XString>::calculate()
        == "O!xstring[s:32,a:1]",
    "OPAQUE_TYPE signature should be O!xstring[s:32,a:1]"
);

// --- OPAQUE_CONTAINER tests ---

// 2. Container with int32_t element
constexpr auto xvec_i32_layout =
    TypeSignature<opaque_test::XVector<int32_t>>::calculate();
static_assert(
    contains(xvec_i32_layout, "O!xvector[s:24,a:1]<"),
    "OPAQUE_CONTAINER Layout should start with O!xvector[s:24,a:1]<"
);
static_assert(
    contains(xvec_i32_layout, "i32[s:4,a:4]"),
    "OPAQUE_CONTAINER Layout should contain element signature"
);

// 3. Container with enum element
namespace opaque_test {
    enum class Color : uint8_t { Red, Green, Blue };
}

constexpr auto xvec_color_layout =
    TypeSignature<opaque_test::XVector<opaque_test::Color>>::calculate();

// Layout: enum without qualified name
static_assert(
    contains(xvec_color_layout, "enum[s:"),
    "OPAQUE_CONTAINER: element enum should use layout encoding"
);

// 4. Different element types produce different signatures
static_assert(
    !(TypeSignature<opaque_test::XVector<int32_t>>::calculate()
        == TypeSignature<opaque_test::XVector<double>>::calculate()),
    "OPAQUE_CONTAINER: different element types should produce different signatures"
);

// --- OPAQUE_MAP tests ---

// 5. Map with int32_t key and double value
constexpr auto xmap_sig =
    TypeSignature<opaque_test::XMap<int32_t, double>>::calculate();
static_assert(
    contains(xmap_sig, "O!xmap[s:48,a:1]<"),
    "OPAQUE_MAP should start with O!xmap[s:48,a:1]<"
);
static_assert(
    contains(xmap_sig, "i32[s:4,a:4]"),
    "OPAQUE_MAP should contain key signature"
);
static_assert(
    contains(xmap_sig, "f64[s:8,a:8]"),
    "OPAQUE_MAP should contain value signature"
);

// 6. Map with same K,V types produces valid signature
constexpr auto xmap_same_kv =
    TypeSignature<opaque_test::XMap<int32_t, int32_t>>::calculate();
static_assert(
    contains(xmap_same_kv, "O!xmap[s:48,a:1]<"),
    "OPAQUE_MAP with same K,V: should produce valid signature"
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

// 7. Scoped enum with explicit type
static_assert(
    is_fixed_enum<enum_test::ScopedFixed>(),
    "Scoped enum with explicit uint32_t should be fixed"
);

// 8. Scoped enum with default type (int)
static_assert(
    is_fixed_enum<enum_test::ScopedDefault>(),
    "Scoped enum with default int should be fixed"
);

// 9. Unscoped enum with explicit type
static_assert(
    is_fixed_enum<enum_test::UnscopedFixed>(),
    "Unscoped enum with explicit int16_t should be fixed"
);

// 10. Unscoped enum with implicit type -- also returns true (documented limitation)
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
    TypeSignature<integration_test::SharedBlock>::calculate();

// 11. Opaque field emitted as leaf in containing struct's Layout signature
static_assert(
    contains(block_layout, "O!xstring[s:32,a:1]"),
    "Integration: Layout should contain opaque O!xstring[s:32,a:1] as leaf"
);

// 12. Primitive fields around the opaque are still flattened normally
static_assert(
    contains(block_layout, "i32[s:4,a:4]"),
    "Integration: Layout should contain i32 field"
);
static_assert(
    contains(block_layout, "f64[s:8,a:8]"),
    "Integration: Layout should contain f64 field"
);

// =========================================================================
// Part 4: Opaque base class handling (F4 fix)
// =========================================================================
// The Layout engine must also respect opaque types used as base classes,
// emitting them as leaf nodes instead of flattening their internals.

namespace opaque_base_test {
    struct DerivedFromOpaque : opaque_test::XString {
        int32_t extra;
    };
}

constexpr auto derived_opaque_layout =
    TypeSignature<opaque_base_test::DerivedFromOpaque>::calculate();

// 13. Opaque base class emitted as leaf in Layout signature
static_assert(
    contains(derived_opaque_layout, "O!xstring[s:32,a:1]"),
    "F4 fix: opaque base class should appear as O!xstring[s:32,a:1] leaf"
);

// 14. Derived class's own field still present
static_assert(
    contains(derived_opaque_layout, "i32[s:4,a:4]"),
    "F4 fix: derived class direct field should still appear"
);

// =========================================================================
// Part 5: Finding F5 -- Empty struct visibility
// =========================================================================
// An empty struct field contributes zero leaf nodes to the flat field list.
// The outer record header (s:SIZE,a:ALIGN) still captures the total size,
// so two records that differ only by an empty field will have different
// size headers -- soundness is preserved.

namespace f5_test {
    struct Empty {};

    struct WithEmpty {
        int32_t x;
        Empty e;          // occupies 1 byte (sizeof(Empty)==1), at some offset
        int32_t y;
    };

    struct WithoutEmpty {
        int32_t x;
        int32_t y;
    };
}

constexpr auto with_empty_layout =
    TypeSignature<f5_test::WithEmpty>::calculate();
constexpr auto without_empty_layout =
    TypeSignature<f5_test::WithoutEmpty>::calculate();

// 15. Empty field is invisible in the flat field list (conservative)
// The flat fields inside {} should only contain i32 entries, no entry for Empty.
// But the size headers differ: WithEmpty has sizeof >= 9 (padding may differ),
// while WithoutEmpty has sizeof == 8.
static_assert(
    !(with_empty_layout == without_empty_layout),
    "F5: records differing only by empty field must have different Layout signatures (size header differs)"
);

// =========================================================================
// Part 6: Finding F6 -- [[no_unique_address]] behavior
// =========================================================================
// With [[no_unique_address]], an empty member may share its offset with
// an adjacent member. The Layout engine reports what reflection sees.

namespace f6_test {
    struct Tag {};

    struct WithNUA {
        int32_t x;
        [[no_unique_address]] Tag t;
        int32_t y;
    };

    struct PlainTwoInt {
        int32_t x;
        int32_t y;
    };
}

constexpr auto nua_layout =
    TypeSignature<f6_test::WithNUA>::calculate();
constexpr auto plain_two_int_layout =
    TypeSignature<f6_test::PlainTwoInt>::calculate();

// 16. With [[no_unique_address]], if the empty member is optimized away
// (offset overlaps with next field), the record size may equal PlainTwoInt.
// Whether signatures match depends on compiler EBO decisions.
// We just verify both compile and produce valid signatures.
static_assert(
    nua_layout.length() > 0,
    "F6: [[no_unique_address]] struct should produce a valid Layout signature"
);
static_assert(
    plain_two_int_layout.length() > 0,
    "F6: plain two-int struct should produce a valid Layout signature"
);

// Task 2.3 / Bug 1 fix: After fixing empty member evaporation, WithNUA and
// PlainTwoInt MUST produce different signatures even when sizeof matches,
// because WithNUA contains an empty member that is now correctly emitted
// as a leaf node in the signature.  The two types have different member
// structures and therefore different binary layout identities.
static_assert(
    nua_layout != plain_two_int_layout,
    "F6: WithNUA must differ from PlainTwoInt -- empty member is part of layout identity"
);

// =========================================================================
// Part 7: Legacy opaque pointer_free conservatism
// =========================================================================
// Legacy opaque macros (TYPELAYOUT_OPAQUE_TYPE, _CONTAINER, _MAP) now
// default to pointer_free = false, meaning has_pointer == true.
// This is the conservative choice: opaque internals are unknown, so we
// assume they may contain pointers unless the user explicitly asserts
// otherwise via TYPELAYOUT_REGISTER_OPAQUE.
//
// This ensures classify() produces SafetyLevel::Opaque (or PointerRisk)
// rather than falsely reporting TrivialSafe for opaque types.

// 18. Legacy OPAQUE_TYPE: has_pointer defaults to true (conservative)
static_assert(
    layout_traits<opaque_test::XString>::has_pointer,
    "Legacy OPAQUE_TYPE: has_pointer should default to true (pointer_free = false)"
);

// 19. Legacy OPAQUE_CONTAINER: has_pointer defaults to true
static_assert(
    layout_traits<opaque_test::XVector<int32_t>>::has_pointer,
    "Legacy OPAQUE_CONTAINER: has_pointer should default to true (pointer_free = false)"
);

// 20. Legacy OPAQUE_MAP: has_pointer defaults to true
static_assert(
    layout_traits<opaque_test::XMap<int32_t, double>>::has_pointer,
    "Legacy OPAQUE_MAP: has_pointer should default to true (pointer_free = false)"
);

// 21. Opaque types are still detected as opaque
static_assert(
    layout_traits<opaque_test::XString>::has_opaque,
    "Legacy OPAQUE_TYPE: has_opaque should be true"
);

// =========================================================================
// Part 8: Finding F8 -- long vs int64_t platform erasure
// =========================================================================
// On this platform, long and int64_t (or int32_t) should produce the
// same Layout signature, since TypeLayout maps them to the same canonical
// name based on sizeof.

namespace f8_test {
    struct WithLong {
        long x;
    };

    // Pick the matching fixed-width type based on sizeof(long)
    struct WithFixedWidth {
        // sizeof(long)==8 -> int64_t, sizeof(long)==4 -> int32_t
        std::conditional_t<sizeof(long) == 8, int64_t, int32_t> x;
    };
}

constexpr auto long_layout =
    TypeSignature<f8_test::WithLong>::calculate();
constexpr auto fixed_layout =
    TypeSignature<f8_test::WithFixedWidth>::calculate();

// 17. long and its corresponding fixed-width type produce identical Layout signatures
static_assert(
    long_layout == fixed_layout,
    "F8: long and matching fixed-width int should produce identical Layout signatures on same platform"
);

// =========================================================================
// Part 9: CV-qualified opaque member handling
// =========================================================================
// A 'const OpaqueType' member must be treated as an opaque leaf node, not
// flattened into its internal bytes.  This exercises the fix to
// has_opaque_signature<T> which now strips cv-qualifiers before the check.

namespace cv_opaque_test {
    struct WithConstOpaque {
        int32_t x;
        const opaque_test::XString name;
        double value;
    };
}

constexpr auto cv_block_layout =
    TypeSignature<cv_opaque_test::WithConstOpaque>::calculate();

// 22. const-qualified opaque member appears as opaque leaf, not expanded
static_assert(
    contains(cv_block_layout, "O!xstring[s:32,a:1]"),
    "CV-qualified opaque member must be emitted as opaque leaf, not flattened"
);

// 23. Other fields still present
static_assert(
    contains(cv_block_layout, "i32[s:4,a:4]"),
    "CV-qualified opaque test: i32 field should still be present"
);
static_assert(
    contains(cv_block_layout, "f64[s:8,a:8]"),
    "CV-qualified opaque test: f64 field should still be present"
);

// =========================================================================
// Main -- runtime confirmation
// =========================================================================

int main() {
    std::cout << "=== Opaque & is_fixed_enum Tests ===\n\n";

    std::cout << "XString Layout:      " << TypeSignature<opaque_test::XString>::calculate().value << "\n";
    std::cout << "XVector<i32> Layout: " << xvec_i32_layout.value << "\n";
    std::cout << "XMap<i32,f64> Layout: " << xmap_sig.value << "\n";
    std::cout << "SharedBlock Layout:  " << block_layout.value << "\n";

    std::cout << "\n--- Finding Verification ---\n";
    std::cout << "WithEmpty Layout:    " << with_empty_layout.value << "\n";
    std::cout << "WithoutEmpty Layout: " << without_empty_layout.value << "\n";
    std::cout << "NUA Layout:          " << nua_layout.value << "\n";
    std::cout << "PlainTwoInt Layout:  " << plain_two_int_layout.value << "\n";
    std::cout << "Long Layout:         " << long_layout.value << "\n";
    std::cout << "FixedWidth Layout:   " << fixed_layout.value << "\n";

    std::cout << "\n--- Legacy Opaque Pointer Safety ---\n";
    std::cout << "XString has_pointer:     " << layout_traits<opaque_test::XString>::has_pointer << "\n";
    std::cout << "XVector<i32> has_pointer:" << layout_traits<opaque_test::XVector<int32_t>>::has_pointer << "\n";
    std::cout << "XMap<i32,f64> has_pointer:" << layout_traits<opaque_test::XMap<int32_t, double>>::has_pointer << "\n";
    std::cout << "XString has_opaque:      " << layout_traits<opaque_test::XString>::has_opaque << "\n";

    std::cout << "\nAll " << 21 << " static_assert tests passed at compile time.\n";
    return 0;
}
