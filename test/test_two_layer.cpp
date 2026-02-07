// Two-layer signature system tests.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;

namespace test_basic {
    struct Simple { int32_t x; double y; };
    struct Simple2 { int32_t a; double b; };  // Same layout, different names
}

namespace test_inheritance {
    struct Base { int32_t id; };
    struct Derived : Base { double value; };
    struct Flat { int32_t id; double value; };
}

namespace test_multilevel {
    struct A { int32_t x; };
    struct B : A { int32_t y; };
    struct C : B { int32_t z; };
    struct Flat { int32_t x; int32_t y; int32_t z; };
}

namespace test_multiple_inheritance {
    struct Base1 { int32_t a; };
    struct Base2 { int32_t b; };
    struct Multi : Base1, Base2 { int32_t c; };
    struct Flat { int32_t a; int32_t b; int32_t c; };
}

namespace test_polymorphic {
    struct Poly { virtual void foo(); int32_t x; };
    struct NonPoly { int32_t x; };
}

namespace test_empty_base {
    struct Empty {};
    struct WithEmpty : Empty { int32_t x; double y; };
    struct Plain { int32_t x; double y; };
}

namespace test_nested {
    struct Inner { int32_t a; int32_t b; };
    struct Outer1 { Inner inner; double d; };
    struct Outer2 { Inner inner; double d; };
}

// --- Composition flattening (Fix #4) ---

namespace test_composition_flatten {
    struct Inner { int32_t a; int32_t b; };
    struct Composed { Inner x; };
    struct Flat { int32_t a; int32_t b; };

    struct Deep { int32_t p; int32_t q; };
    struct Mid { Deep d; int32_t r; };
    struct Outer { Mid m; };
    struct DeepFlat { int32_t p; int32_t q; int32_t r; };
}

// --- Enum identity (Fix #2) ---

namespace test_enum_identity {
    enum class Color : uint8_t { Red, Green, Blue };
    enum class Shape : uint8_t { Circle, Square, Triangle };
}

// --- Base class namespace collision (Fix #1) ---

namespace test_base_ns1 { struct Tag { int32_t id; }; }
namespace test_base_ns2 { struct Tag { int32_t id; }; }

namespace test_base_collision {
    struct A : test_base_ns1::Tag { double v; };
    struct B : test_base_ns2::Tag { double v; };
}

// --- Virtual inheritance ---

namespace test_virtual_inherit {
    struct VBase { int32_t x; };
    struct Derived : virtual VBase { int32_t y; };
}

// --- Union non-flattening ---

namespace test_union {
    struct Inner { int32_t a; int32_t b; };
    union U1 { Inner x; double y; };
    union U2 { Inner x; double y; };
    // U1 and U2 should have identical Layout signatures
    // Inner member should appear as record{...}, not be flattened
}

// --- alignas ---

namespace test_alignas {
    struct alignas(16) Aligned { int32_t a; int32_t b; };
    struct alignas(16) Aligned2 { int32_t c; int32_t d; };
}

// --- Layout signature tests ---

static_assert([]() consteval {
    constexpr auto sig = get_layout_signature<test_basic::Simple>();
    constexpr auto expected = CompileString{"[64-le]record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}"};
    return sig == expected;
}(), "Simple struct Layout signature format");

static_assert(layout_signatures_match<test_inheritance::Derived, test_inheritance::Flat>(),
    "Derived and Flat should have identical Layout signatures");

static_assert(layout_signatures_match<test_multilevel::C, test_multilevel::Flat>(),
    "Multi-level inheritance should flatten to match flat struct");

static_assert(layout_signatures_match<test_multiple_inheritance::Multi, test_multiple_inheritance::Flat>(),
    "Multiple inheritance should flatten to match flat struct");

// Fix #3: Polymorphic types now have vptr marker in Layout
static_assert([]() consteval {
    constexpr auto sig = get_layout_signature<test_polymorphic::Poly>();
    // vptr should appear in Layout signature
    for (std::size_t i = 0; i + 3 < sig.length(); ++i) {
        if (sig.value[i] == 'v' && sig.value[i+1] == 'p' && sig.value[i+2] == 't' && sig.value[i+3] == 'r')
            return true;
    }
    return false;
}(), "Polymorphic type Layout SHOULD contain 'vptr'");

// Fix #3: Polymorphic vs non-polymorphic must NOT match in Layout
static_assert(!layout_signatures_match<test_polymorphic::Poly, test_polymorphic::NonPoly>(),
    "Polymorphic and non-polymorphic types must NOT match in Layout");

// Fix #4: Composition flattening
static_assert(layout_signatures_match<test_composition_flatten::Composed, test_composition_flatten::Flat>(),
    "Composed struct should flatten to match flat struct in Layout");

static_assert(layout_signatures_match<test_composition_flatten::Outer, test_composition_flatten::DeepFlat>(),
    "Deep composition should flatten to match flat struct in Layout");

struct ByteArrayChar { char buf[32]; };
struct ByteArrayU8 { uint8_t buf[32]; };
struct ByteArrayByte { std::byte buf[32]; };
static_assert(layout_signatures_match<ByteArrayChar, ByteArrayU8>(),
    "char[] and uint8_t[] should normalize to same Layout signature");
static_assert(layout_signatures_match<ByteArrayChar, ByteArrayByte>(),
    "char[] and std::byte[] should normalize to same Layout signature");

// --- Definition signature tests ---

static_assert([]() consteval {
    constexpr auto sig = get_definition_signature<test_basic::Simple>();
    constexpr auto expected = CompileString{"[64-le]record[s:16,a:8]{@0[x]:i32[s:4,a:4],@8[y]:f64[s:8,a:8]}"};
    return sig == expected;
}(), "Simple struct Definition signature format");

static_assert([]() consteval {
    constexpr auto sig = get_definition_signature<test_inheritance::Derived>();
    for (std::size_t i = 0; i + 6 < sig.length(); ++i) {
        if (sig.value[i] == '~' && sig.value[i+1] == 'b' && sig.value[i+2] == 'a' 
            && sig.value[i+3] == 's' && sig.value[i+4] == 'e' && sig.value[i+5] == '<') {
            return true;
        }
    }
    return false;
}(), "Definition should contain ~base<Name>");

static_assert([]() consteval {
    constexpr auto sig = get_definition_signature<test_polymorphic::Poly>();
    for (std::size_t i = 0; i + 3 < sig.length(); ++i) {
        if (sig.value[i] == 'p' && sig.value[i+1] == 'o' && sig.value[i+2] == 'l') return true;
    }
    return false;
}(), "Polymorphic type Definition should contain 'polymorphic'");

// Fix #2: Enum identity in Definition mode
static_assert(!definition_signatures_match<test_enum_identity::Color, test_enum_identity::Shape>(),
    "Different enums with same underlying type must NOT match in Definition");

static_assert(layout_signatures_match<test_enum_identity::Color, test_enum_identity::Shape>(),
    "Different enums with same underlying type SHOULD match in Layout");

// Fix #1: Base class qualified names prevent namespace collision
static_assert(!definition_signatures_match<test_base_collision::A, test_base_collision::B>(),
    "Structs inheriting from different-namespace same-name bases must NOT match in Definition");

static_assert(layout_signatures_match<test_base_collision::A, test_base_collision::B>(),
    "Structs inheriting from different-namespace same-name bases SHOULD match in Layout");

// --- Projection relationship tests ---

static_assert([]() consteval {
    constexpr bool def_match = definition_signatures_match<test_nested::Outer1, test_nested::Outer2>();
    constexpr bool lay_match = layout_signatures_match<test_nested::Outer1, test_nested::Outer2>();
    return !def_match || lay_match;  // def_match => lay_match
}(), "Definition match implies Layout match");

static_assert(layout_signatures_match<test_inheritance::Derived, test_inheritance::Flat>(),
    "Layout should match for Derived vs Flat");
static_assert(!definition_signatures_match<test_inheritance::Derived, test_inheritance::Flat>(),
    "Definition should NOT match for Derived vs Flat");

static_assert(layout_signatures_match<test_basic::Simple, test_basic::Simple2>(),
    "Layout should match for types with different field names");
static_assert(!definition_signatures_match<test_basic::Simple, test_basic::Simple2>(),
    "Definition should NOT match for types with different field names");

static_assert(get_layout_signature<int32_t>() == get_definition_signature<int32_t>(),
    "i32 Layout == Definition");
static_assert(get_layout_signature<double>() == get_definition_signature<double>(),
    "f64 Layout == Definition");

enum class Color : uint8_t { Red, Green, Blue };
static_assert([]() consteval {
    constexpr auto sig = get_layout_signature<Color>();
    constexpr auto expected = CompileString{"[64-le]enum[s:1,a:1]<u8[s:1,a:1]>"};
    return sig == expected;
}(), "Enum Layout signature correct");

static_assert(layout_signatures_match<test_empty_base::WithEmpty, test_empty_base::Plain>(),
    "Empty base class should not affect Layout signature");

// Virtual inheritance: VBase fields should appear in Layout signature
static_assert([]() consteval {
    constexpr auto sig = get_layout_signature<test_virtual_inherit::Derived>();
    // Must contain i32 field from VBase (x) in addition to direct field (y)
    int count = 0;
    for (std::size_t i = 0; i + 2 < sig.length(); ++i) {
        if (sig.value[i] == 'i' && sig.value[i+1] == '3' && sig.value[i+2] == '2')
            ++count;
    }
    return count >= 2;  // both x and y
}(), "Virtual base fields must appear in Layout signature");

// Union non-flattening: two identical unions should match
static_assert(layout_signatures_match<test_union::U1, test_union::U2>(),
    "Identical unions should have identical Layout signatures");

// Union Layout signature should contain 'record' (Inner is kept as atomic record, not flattened)
static_assert([]() consteval {
    constexpr auto sig = get_layout_signature<test_union::U1>();
    // Should contain "record" inside the union (the Inner member kept as record)
    for (std::size_t i = 0; i + 5 < sig.length(); ++i) {
        if (sig.value[i] == 'r' && sig.value[i+1] == 'e' && sig.value[i+2] == 'c'
            && sig.value[i+3] == 'o' && sig.value[i+4] == 'r' && sig.value[i+5] == 'd')
            return true;
    }
    return false;
}(), "Union Layout should keep struct member as 'record' (not flatten)");

// alignas: captured via alignof(T)
static_assert(layout_signatures_match<test_alignas::Aligned, test_alignas::Aligned2>(),
    "Two alignas(16) structs with same field layout should match");

static_assert(!layout_signatures_match<test_basic::Simple, test_alignas::Aligned>(),
    "Different alignment should cause Layout mismatch");

int main() {
    std::cout << "=== Two-Layer Signature Tests ===\n\n";

    auto print_sig = [](const char* label, const auto& sig) {
        std::cout << "  " << label << sig << "\n";
    };

    std::cout << "--- Layout Signatures ---\n";
    print_sig("Simple:     ", get_layout_signature<test_basic::Simple>());
    print_sig("Derived:    ", get_layout_signature<test_inheritance::Derived>());
    print_sig("Flat:       ", get_layout_signature<test_inheritance::Flat>());
    print_sig("Poly:       ", get_layout_signature<test_polymorphic::Poly>());
    print_sig("NonPoly:    ", get_layout_signature<test_polymorphic::NonPoly>());
    print_sig("Composed:   ", get_layout_signature<test_composition_flatten::Composed>());
    print_sig("CompFlat:   ", get_layout_signature<test_composition_flatten::Flat>());
    print_sig("DeepOuter:  ", get_layout_signature<test_composition_flatten::Outer>());
    print_sig("DeepFlat:   ", get_layout_signature<test_composition_flatten::DeepFlat>());

    std::cout << "\n--- Definition Signatures ---\n";
    print_sig("Simple:     ", get_definition_signature<test_basic::Simple>());
    print_sig("Derived:    ", get_definition_signature<test_inheritance::Derived>());
    print_sig("Flat:       ", get_definition_signature<test_inheritance::Flat>());
    print_sig("Color:      ", get_definition_signature<test_enum_identity::Color>());
    print_sig("Shape:      ", get_definition_signature<test_enum_identity::Shape>());
    print_sig("BaseNs1:    ", get_definition_signature<test_base_collision::A>());
    print_sig("BaseNs2:    ", get_definition_signature<test_base_collision::B>());

    std::cout << "\n--- Projection ---\n";
    std::cout << "  Derived == Flat (Layout)?      " << (layout_signatures_match<test_inheritance::Derived, test_inheritance::Flat>() ? "YES" : "NO") << "\n";
    std::cout << "  Derived == Flat (Definition)?   " << (definition_signatures_match<test_inheritance::Derived, test_inheritance::Flat>() ? "YES" : "NO") << "\n";
    std::cout << "  Poly == NonPoly (Layout)?       " << (layout_signatures_match<test_polymorphic::Poly, test_polymorphic::NonPoly>() ? "YES" : "NO") << "\n";
    std::cout << "  Color == Shape (Layout)?        " << (layout_signatures_match<test_enum_identity::Color, test_enum_identity::Shape>() ? "YES" : "NO") << "\n";
    std::cout << "  Color == Shape (Definition)?    " << (definition_signatures_match<test_enum_identity::Color, test_enum_identity::Shape>() ? "YES" : "NO") << "\n";
    std::cout << "  Composed == Flat (Layout)?      " << (layout_signatures_match<test_composition_flatten::Composed, test_composition_flatten::Flat>() ? "YES" : "NO") << "\n";

    std::cout << "\nAll tests passed!\n";
    return 0;
}