// Opaque type registration tests.
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

TYPELAYOUT_REGISTER_OPAQUE(opaque_test::XString, "xstring", false)

// For template types, register specific instantiations
TYPELAYOUT_REGISTER_OPAQUE(opaque_test::XVector<int32_t>, "xvector_i32", false)
TYPELAYOUT_REGISTER_OPAQUE(opaque_test::XVector<double>, "xvector_f64", false)

}} // namespace boost::typelayout

// Register enum-element vector outside the namespace block above
// (Color must be defined first)
namespace opaque_test {
    enum class Color : uint8_t { Red, Green, Blue };
}

namespace boost { namespace typelayout {
TYPELAYOUT_REGISTER_OPAQUE(opaque_test::XVector<opaque_test::Color>, "xvector_color", false)
// Typedefs needed because macros can't handle template commas
using XMap_i32_f64 = opaque_test::XMap<int32_t, double>;
using XMap_i32_i32 = opaque_test::XMap<int32_t, int32_t>;
TYPELAYOUT_REGISTER_OPAQUE(XMap_i32_f64, "xmap_i32_f64", false)
TYPELAYOUT_REGISTER_OPAQUE(XMap_i32_i32, "xmap_i32_i32", false)
}} // namespace boost::typelayout

// --- REGISTER_OPAQUE tests ---

// 1. Basic signature format
static_assert(
    TypeSignature<opaque_test::XString>::calculate()
        == "O(xstring|32|1)",
    "REGISTER_OPAQUE signature should be O(xstring|32|1)"
);

// 2. XVector<int32_t> registered with explicit tag
static_assert(
    TypeSignature<opaque_test::XVector<int32_t>>::calculate()
        == "O(xvector_i32|24|1)",
    "REGISTER_OPAQUE: XVector<int32_t> should be O(xvector_i32|24|1)"
);

// 3. XVector<Color> registered separately
static_assert(
    TypeSignature<opaque_test::XVector<opaque_test::Color>>::calculate()
        == "O(xvector_color|24|1)",
    "REGISTER_OPAQUE: XVector<Color> should be O(xvector_color|24|1)"
);

// 4. Different element types produce different signatures (different tags)
static_assert(
    !(TypeSignature<opaque_test::XVector<int32_t>>::calculate()
        == TypeSignature<opaque_test::XVector<double>>::calculate()),
    "REGISTER_OPAQUE: different instantiations should produce different signatures"
);

// 5. Map with int32_t key and double value
static_assert(
    TypeSignature<opaque_test::XMap<int32_t, double>>::calculate()
        == "O(xmap_i32_f64|48|1)",
    "REGISTER_OPAQUE: XMap<int32_t, double> should be O(xmap_i32_f64|48|1)"
);

// 6. Map with same K,V types
static_assert(
    TypeSignature<opaque_test::XMap<int32_t, int32_t>>::calculate()
        == "O(xmap_i32_i32|48|1)",
    "REGISTER_OPAQUE: XMap<int32_t, int32_t> should be O(xmap_i32_i32|48|1)"
);

// =========================================================================
// Part 2: Integration -- Opaque as field in a normal struct
// =========================================================================

namespace integration_test {
    struct SharedBlock {
        int32_t id;
        opaque_test::XString name;
        double value;
    };
}

constexpr auto block_layout =
    TypeSignature<integration_test::SharedBlock>::calculate();

// 7. Opaque field emitted as leaf in containing struct's Layout signature
static_assert(
    contains(block_layout, "O(xstring|32|1)"),
    "Integration: Layout should contain opaque O(xstring|32|1) as leaf"
);

// 8. Primitive fields around the opaque are still flattened normally
static_assert(
    contains(block_layout, "i32[s:4,a:4]"),
    "Integration: Layout should contain i32 field"
);
static_assert(
    contains(block_layout, "f64[s:8,a:8]"),
    "Integration: Layout should contain f64 field"
);

// =========================================================================
// Part 3: Opaque base class handling
// =========================================================================

namespace opaque_base_test {
    struct DerivedFromOpaque : opaque_test::XString {
        int32_t extra;
    };
}

constexpr auto derived_opaque_layout =
    TypeSignature<opaque_base_test::DerivedFromOpaque>::calculate();

// 9. Opaque base class emitted as leaf in Layout signature
static_assert(
    contains(derived_opaque_layout, "O(xstring|32|1)"),
    "Opaque base class should appear as O(xstring|32|1) leaf"
);

// 10. Derived class's own field still present
static_assert(
    contains(derived_opaque_layout, "i32[s:4,a:4]"),
    "Derived class direct field should still appear"
);

// =========================================================================
// Part 4: Empty struct visibility
// =========================================================================

namespace f5_test {
    struct Empty {};

    struct WithEmpty {
        int32_t x;
        Empty e;
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

// 11. Records differing only by empty field have different signatures
static_assert(
    !(with_empty_layout == without_empty_layout),
    "Records differing only by empty field must have different Layout signatures (size header differs)"
);

// =========================================================================
// Part 5: [[no_unique_address]] behavior
// =========================================================================

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

static_assert(
    nua_layout.length() > 0,
    "[[no_unique_address]] struct should produce a valid Layout signature"
);
static_assert(
    plain_two_int_layout.length() > 0,
    "plain two-int struct should produce a valid Layout signature"
);

static_assert(
    nua_layout != plain_two_int_layout,
    "WithNUA must differ from PlainTwoInt -- empty member is part of layout identity"
);

// =========================================================================
// Part 6: Opaque pointer_free control
// =========================================================================

// 12. REGISTER_OPAQUE with HasPointer=false: pointer_free=true, has_pointer=false
static_assert(
    !layout_traits<opaque_test::XString>::has_pointer,
    "REGISTER_OPAQUE(false): has_pointer should be false"
);

// 13. Opaque types are detected as opaque
static_assert(
    layout_traits<opaque_test::XString>::has_opaque,
    "REGISTER_OPAQUE: has_opaque should be true"
);

// 14. Opaque type with HasPointer=true
namespace opaque_ptr_test {
    struct WithPtrs { void* p; int x; };
}
namespace boost { namespace typelayout {
TYPELAYOUT_REGISTER_OPAQUE(opaque_ptr_test::WithPtrs, "with_ptrs", true)
}} // namespace boost::typelayout

static_assert(
    layout_traits<opaque_ptr_test::WithPtrs>::has_pointer,
    "REGISTER_OPAQUE(true): has_pointer should be true"
);

// =========================================================================
// Part 7: long vs int64_t platform erasure
// =========================================================================

namespace f8_test {
    struct WithLong {
        long x;
    };

    struct WithFixedWidth {
        std::conditional_t<sizeof(long) == 8, int64_t, int32_t> x;
    };
}

constexpr auto long_layout =
    TypeSignature<f8_test::WithLong>::calculate();
constexpr auto fixed_layout =
    TypeSignature<f8_test::WithFixedWidth>::calculate();

// 15. long and its corresponding fixed-width type produce identical signatures
static_assert(
    long_layout == fixed_layout,
    "long and matching fixed-width int should produce identical Layout signatures on same platform"
);

// =========================================================================
// Part 8: CV-qualified opaque member handling
// =========================================================================

namespace cv_opaque_test {
    struct WithConstOpaque {
        int32_t x;
        const opaque_test::XString name;
        double value;
    };
}

constexpr auto cv_block_layout =
    TypeSignature<cv_opaque_test::WithConstOpaque>::calculate();

// 16. const-qualified opaque member appears as opaque leaf, not expanded
static_assert(
    contains(cv_block_layout, "O(xstring|32|1)"),
    "CV-qualified opaque member must be emitted as opaque leaf, not flattened"
);

// 17. Other fields still present
static_assert(
    contains(cv_block_layout, "i32[s:4,a:4]"),
    "CV-qualified opaque test: i32 field should still be present"
);
static_assert(
    contains(cv_block_layout, "f64[s:8,a:8]"),
    "CV-qualified opaque test: f64 field should still be present"
);

// =========================================================================
// Part 9: Relocatable container pointer_free detection
// =========================================================================

namespace relocatable_test {
    // Simulate a relocatable container (not trivially_copyable)
    template <typename T>
    struct RVec {
        char storage_[32];
        RVec() {}  // non-trivial to prevent trivially_copyable
    };

    template <typename K, typename V>
    struct RMap {
        char storage_[48];
        RMap() {}
    };
} // namespace relocatable_test

namespace boost { namespace typelayout {

TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(relocatable_test::RVec, "rvec")
TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(relocatable_test::RMap, "rmap")

}} // namespace boost::typelayout

// 18. Container with plain scalar: pointer_free = true
static_assert(
    TypeSignature<relocatable_test::RVec<int32_t>>::pointer_free,
    "CONTAINER_RELOCATABLE<i32>: pointer_free should be true"
);

// 19. Container with pointer element: pointer_free = false (ptr[ token)
static_assert(
    !TypeSignature<relocatable_test::RVec<void*>>::pointer_free,
    "CONTAINER_RELOCATABLE<void*>: pointer_free should be false"
);

// 20. Container with function pointer element: pointer_free = false (fnptr[ token)
using FnPtrType = void(*)();
static_assert(
    !TypeSignature<relocatable_test::RVec<FnPtrType>>::pointer_free,
    "CONTAINER_RELOCATABLE<fnptr>: pointer_free should be false"
);

// 21. Map with plain scalars: pointer_free = true
static_assert(
    TypeSignature<relocatable_test::RMap<int32_t, double>>::pointer_free,
    "MAP_RELOCATABLE<i32,f64>: pointer_free should be true"
);

// 22. Map with pointer value: pointer_free = false
static_assert(
    !TypeSignature<relocatable_test::RMap<int32_t, void*>>::pointer_free,
    "MAP_RELOCATABLE<i32,void*>: pointer_free should be false"
);

// 23. Map with function pointer key: pointer_free = false
static_assert(
    !TypeSignature<relocatable_test::RMap<FnPtrType, int32_t>>::pointer_free,
    "MAP_RELOCATABLE<fnptr,i32>: pointer_free should be false"
);

// =========================================================================
// Main -- runtime confirmation
// =========================================================================

int main() {
    std::cout << "=== Opaque Tests ===\n\n";

    std::cout << "XString:             " << TypeSignature<opaque_test::XString>::calculate().value << "\n";
    std::cout << "XVector<i32>:        " << TypeSignature<opaque_test::XVector<int32_t>>::calculate().value << "\n";
    std::cout << "XMap<i32,f64>:       " << TypeSignature<opaque_test::XMap<int32_t, double>>::calculate().value << "\n";
    std::cout << "SharedBlock:         " << block_layout.value << "\n";

    std::cout << "\n--- Finding Verification ---\n";
    std::cout << "WithEmpty Layout:    " << with_empty_layout.value << "\n";
    std::cout << "WithoutEmpty Layout: " << without_empty_layout.value << "\n";
    std::cout << "NUA Layout:          " << nua_layout.value << "\n";
    std::cout << "PlainTwoInt Layout:  " << plain_two_int_layout.value << "\n";
    std::cout << "Long Layout:         " << long_layout.value << "\n";
    std::cout << "FixedWidth Layout:   " << fixed_layout.value << "\n";

    std::cout << "\n--- Pointer Safety ---\n";
    std::cout << "XString has_pointer:      " << layout_traits<opaque_test::XString>::has_pointer << "\n";
    std::cout << "WithPtrs has_pointer:     " << layout_traits<opaque_ptr_test::WithPtrs>::has_pointer << "\n";
    std::cout << "XString has_opaque:       " << layout_traits<opaque_test::XString>::has_opaque << "\n";

    std::cout << "\n--- Relocatable Container ---\n";
    std::cout << "RVec<i32> pointer_free:   " << TypeSignature<relocatable_test::RVec<int32_t>>::pointer_free << "\n";
    std::cout << "RVec<void*> pointer_free: " << TypeSignature<relocatable_test::RVec<void*>>::pointer_free << "\n";
    std::cout << "RVec<fnptr> pointer_free: " << TypeSignature<relocatable_test::RVec<FnPtrType>>::pointer_free << "\n";
    std::cout << "RMap<i32,f64> ptr_free:   " << TypeSignature<relocatable_test::RMap<int32_t, double>>::pointer_free << "\n";
    std::cout << "RMap<i32,void*> ptr_free: " << TypeSignature<relocatable_test::RMap<int32_t, void*>>::pointer_free << "\n";
    std::cout << "RMap<fnptr,i32> ptr_free: " << TypeSignature<relocatable_test::RMap<FnPtrType, int32_t>>::pointer_free << "\n";

    std::cout << "\nAll 23 static_assert tests passed at compile time.\n";
    return 0;
}
