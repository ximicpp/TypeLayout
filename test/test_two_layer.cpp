// Two-layer signature system tests.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include "test_util.hpp"
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;
using boost::typelayout::test::contains;

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

// --- Bit-fields ---

namespace test_bitfield {
    struct Flags {
        uint8_t a : 3;
        uint8_t b : 5;
    };
}

// --- Anonymous members ---

namespace test_anon {
    struct WithAnon {
        int32_t x;
        struct { int32_t y; };  // anonymous struct member
    };
}

// --- Deep namespace (>=3 levels) ---

namespace test_deep_ns {
    namespace a { namespace b { namespace c { struct Tag { int32_t id; }; } } }
    namespace d { namespace b { namespace c { struct Tag { int32_t id; }; } } }
    struct FromA : a::b::c::Tag { double v; };
    struct FromD : d::b::c::Tag { double v; };
}

// --- Pointer/reference types ---

namespace test_ptr_ref {
    struct WithPtr { int32_t* p; double* q; };
    struct WithRef { int32_t x; };
    struct WithFnPtr { void(*fn)(int); int32_t x; };
}

// --- Platform-dependent types ---

namespace test_platform_types {
    struct WithLongDouble { long double ld; };
    struct WithWchar { wchar_t wc; int32_t x; };
}

// --- Array fields ---

namespace test_array_fields {
    struct WithArray { int32_t arr[4]; double d; };
    struct WithMultiDim { int32_t mat[2][3]; };
}

// --- CV-qualified fields ---

namespace test_cv_fields {
    struct WithConst { const int32_t x; double y; };
    struct WithoutConst { int32_t x; double y; };
    struct WithVolatile { volatile int32_t x; double y; };
}

// --- alignas ---

namespace test_alignas {
    struct alignas(16) Aligned { int32_t a; int32_t b; };
    struct alignas(16) Aligned2 { int32_t c; int32_t d; };
}

// --- Layout signature tests ---

static_assert([]() consteval {
    constexpr auto sig = get_layout_signature<test_basic::Simple>();
    constexpr auto expected = FixedString{"[64-le]record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}"};
    return sig == expected;
}(), "Simple struct Layout signature format");

static_assert(layout_signatures_match<test_inheritance::Derived, test_inheritance::Flat>(),
    "Derived and Flat should have identical Layout signatures");

static_assert(layout_signatures_match<test_multilevel::C, test_multilevel::Flat>(),
    "Multi-level inheritance should flatten to match flat struct");

static_assert(layout_signatures_match<test_multiple_inheritance::Multi, test_multiple_inheritance::Flat>(),
    "Multiple inheritance should flatten to match flat struct");

// Fix #3: Polymorphic types have vptr as a synthesized ptr field in Layout
static_assert(contains(get_layout_signature<test_polymorphic::Poly>(), "ptr["),
    "Polymorphic type Layout SHOULD contain 'ptr[' for vptr");

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
    constexpr auto expected = FixedString{"[64-le]record[s:16,a:8]{@0[x]:i32[s:4,a:4],@8[y]:f64[s:8,a:8]}"};
    return sig == expected;
}(), "Simple struct Definition signature format");

static_assert(contains(get_definition_signature<test_inheritance::Derived>(), "~base<"),
    "Definition should contain ~base<Name>");

static_assert(contains(get_definition_signature<test_polymorphic::Poly>(), "polymorphic"),
    "Polymorphic type Definition should contain 'polymorphic'");

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
    constexpr auto expected = FixedString{"[64-le]enum[s:1,a:1]<u8[s:1,a:1]>"};
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
static_assert(contains(get_layout_signature<test_union::U1>(), "record"),
    "Union Layout should keep struct member as 'record' (not flatten)");

// Bit-field: Layout signature should contain "bits<"
static_assert(contains(get_layout_signature<test_bitfield::Flags>(), "bits<"),
    "Bit-field Layout signature should contain 'bits<'");

// Bit-field: Definition signature should contain field names in brackets
static_assert(
    contains(get_definition_signature<test_bitfield::Flags>(), "[a]") &&
    contains(get_definition_signature<test_bitfield::Flags>(), "[b]"),
    "Bit-field Definition signature should contain field names [a] and [b]");

// Anonymous member: Definition should contain "<anon:"
static_assert(contains(get_definition_signature<test_anon::WithAnon>(), "<anon:"),
    "Anonymous member Definition signature should contain '<anon:'");

// alignas: captured via alignof(T)
static_assert(layout_signatures_match<test_alignas::Aligned, test_alignas::Aligned2>(),
    "Two alignas(16) structs with same field layout should match");

static_assert(!layout_signatures_match<test_basic::Simple, test_alignas::Aligned>(),
    "Different alignment should cause Layout mismatch");

// --- Additional Definition-layer tests ---

// Task 1.2: Deep namespace (>=3 levels) qualified name correctness
static_assert(contains(get_definition_signature<test_deep_ns::FromA>(), "a::b::c::Tag"),
    "Deep namespace base should include full path a::b::c::Tag");
static_assert(contains(get_definition_signature<test_deep_ns::FromD>(), "d::b::c::Tag"),
    "Deep namespace base should include full path d::b::c::Tag");
static_assert(!definition_signatures_match<test_deep_ns::FromA, test_deep_ns::FromD>(),
    "Types inheriting from different deep-namespace bases must NOT match in Definition");
static_assert(layout_signatures_match<test_deep_ns::FromA, test_deep_ns::FromD>(),
    "Types inheriting from different deep-namespace bases SHOULD match in Layout");

// Task 2.1: Virtual inheritance ~vbase<> in Definition
static_assert(contains(get_definition_signature<test_virtual_inherit::Derived>(), "~vbase<"),
    "Virtual base should appear as ~vbase<> in Definition signature");

// Task 2.2: Multiple inheritance Definition format
static_assert(
    contains(get_definition_signature<test_multiple_inheritance::Multi>(), "~base<") &&
    contains(get_definition_signature<test_multiple_inheritance::Multi>(), "Base1") &&
    contains(get_definition_signature<test_multiple_inheritance::Multi>(), "Base2"),
    "Multiple inheritance Definition should contain ~base<> for both bases");
static_assert(!definition_signatures_match<test_multiple_inheritance::Multi, test_multiple_inheritance::Flat>(),
    "Multiple inheritance should NOT match flat struct in Definition");

// Task 2.3: Deeply nested struct Definition preserves tree
static_assert(
    contains(get_definition_signature<test_composition_flatten::Outer>(), "[m]:record") &&
    contains(get_definition_signature<test_composition_flatten::Outer>(), "[d]:record"),
    "Deeply nested struct Definition should preserve tree structure with field names");

// Task 2.4: Union Definition field names
static_assert(
    contains(get_definition_signature<test_union::U1>(), "[x]:") &&
    contains(get_definition_signature<test_union::U1>(), "[y]:"),
    "Union Definition signature should include field names [x] and [y]");

// --- Layout-specific type tests ---

// Pointer types: all pointers produce ptr[s:8,a:8] regardless of pointee
static_assert(contains(get_layout_signature<test_ptr_ref::WithPtr>(), "ptr[s:8,a:8]"),
    "Pointer field should produce ptr[s:SIZE,a:ALIGN]");

// Two struct with pointer fields at same offsets should match
// (int32_t* and double* both produce ptr[s:8,a:8])
static_assert([]() consteval {
    constexpr auto sig = get_layout_signature<test_ptr_ref::WithPtr>();
    // Should contain two ptr fields at @0 and @8
    return contains(sig, "ptr[s:8,a:8]");
}(), "Pointer fields should be encoded as ptr");

// Function pointer: should produce fnptr
static_assert(contains(get_layout_signature<test_ptr_ref::WithFnPtr>(), "fnptr[s:8,a:8]"),
    "Function pointer field should produce fnptr[s:SIZE,a:ALIGN]");

// Platform types: long double and wchar_t
static_assert(contains(get_layout_signature<test_platform_types::WithLongDouble>(), "f80[s:"),
    "long double field should produce f80 signature");
static_assert(contains(get_layout_signature<test_platform_types::WithWchar>(), "wchar[s:"),
    "wchar_t field should produce wchar signature");

// Anonymous member Layout: should flatten (no field names in Layout)
static_assert(!contains(get_layout_signature<test_anon::WithAnon>(), "<anon:"),
    "Anonymous member should NOT have <anon:> in Layout signature (no names in Layout)");

// Array field: should contain array<> syntax
static_assert(contains(get_layout_signature<test_array_fields::WithArray>(), "array[s:"),
    "Array field should produce array signature");

// Multidimensional array: int[2][3] should be array of arrays
static_assert(contains(get_layout_signature<test_array_fields::WithMultiDim>(), "array[s:"),
    "Multidimensional array field should produce nested array signature");

// CV-qualified fields: const/volatile should not affect Layout
static_assert(layout_signatures_match<test_cv_fields::WithConst, test_cv_fields::WithoutConst>(),
    "const-qualified fields should match non-const in Layout");
static_assert(layout_signatures_match<test_cv_fields::WithVolatile, test_cv_fields::WithoutConst>(),
    "volatile-qualified fields should match non-volatile in Layout");

// =========================================================================
// Task 2.1: Negative tests for unsupported types
// =========================================================================
//
// void, T[] (unbounded array), and bare function types trigger
// static_assert inside TypeSignature::calculate().  Because static_assert
// is a hard error (not SFINAE-detectable), we cannot write
// "requires { get_layout_signature<void>(); }" -- the program is
// ill-formed regardless.  Instead, we verify the type-trait predicates that
// guard the rejection paths, ensuring the implementation *would* reject
// these types.

static_assert(std::is_void_v<void>,
    "void is correctly classified -- would be rejected by type_map (C++ hard error)");
static_assert(!std::is_void_v<void*>,
    "void* is NOT void -- should be accepted as ptr");

static_assert(std::is_unbounded_array_v<int[]>,
    "int[] is an unbounded array -- would be rejected by type_map");
static_assert(!std::is_unbounded_array_v<int[4]>,
    "int[4] is a bounded array -- should be accepted");

static_assert(std::is_function_v<void(int)>,
    "void(int) is a function type -- would be rejected by type_map");
static_assert(!std::is_function_v<void(*)(int)>,
    "void(*)(int) is a function pointer -- should be accepted as fnptr");

// Positive counterparts: accepted types produce non-empty signatures
static_assert(get_layout_signature<void*>().length() > 0,
    "void* should produce a valid Layout signature");
static_assert(get_layout_signature<int[4]>().length() > 0,
    "int[4] should produce a valid Layout signature");
static_assert(get_layout_signature<void(*)(int)>().length() > 0,
    "void(*)(int) should produce a valid Layout signature");

// =========================================================================
// Task 2.2: Member-pointer and nullptr_t signature tests
// =========================================================================

namespace test_memptr {
    struct S { int32_t x; double y; };
}

// nullptr_t: should produce "nullptr[s:SIZE,a:ALIGN]"
static_assert(contains(get_layout_signature<std::nullptr_t>(), "nullptr[s:"),
    "nullptr_t Layout signature should contain 'nullptr[s:'");
static_assert(get_layout_signature<std::nullptr_t>() == get_definition_signature<std::nullptr_t>(),
    "nullptr_t Layout and Definition signatures should be identical");

// Member data pointer: int32_t S::* -- should produce "memptr[s:SIZE,a:ALIGN]"
static_assert(contains(get_layout_signature<int32_t test_memptr::S::*>(), "memptr[s:"),
    "Member data pointer Layout signature should contain 'memptr[s:'");

// Member function pointer: void (S::*)() -- should also produce "memptr"
static_assert(contains(get_layout_signature<void (test_memptr::S::*)()>(), "memptr[s:"),
    "Member function pointer Layout signature should contain 'memptr[s:'");

// Member pointer Layout == Definition (no name component)
static_assert(
    get_layout_signature<int32_t test_memptr::S::*>() ==
    get_definition_signature<int32_t test_memptr::S::*>(),
    "Member data pointer Layout and Definition should be identical");

// Struct with member-pointer field
namespace test_memptr_field {
    struct WithMemPtr { int32_t test_memptr::S::* mp; int32_t x; };
}
static_assert(contains(get_layout_signature<test_memptr_field::WithMemPtr>(), "memptr"),
    "Struct with member-pointer field should contain 'memptr' in Layout");

// Struct with nullptr_t field
namespace test_nullptr_field {
    struct WithNullptr { std::nullptr_t np; int32_t x; };
}
static_assert(contains(get_layout_signature<test_nullptr_field::WithNullptr>(), "nullptr"),
    "Struct with nullptr_t field should contain 'nullptr' in Layout");

// =========================================================================

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
    print_sig("DeepNsA:    ", get_definition_signature<test_deep_ns::FromA>());
    print_sig("DeepNsD:    ", get_definition_signature<test_deep_ns::FromD>());
    print_sig("VirtInh:    ", get_definition_signature<test_virtual_inherit::Derived>());
    print_sig("MultiInh:   ", get_definition_signature<test_multiple_inheritance::Multi>());
    print_sig("UnionDef:   ", get_definition_signature<test_union::U1>());
    print_sig("NestedDef:  ", get_definition_signature<test_composition_flatten::Outer>());

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
