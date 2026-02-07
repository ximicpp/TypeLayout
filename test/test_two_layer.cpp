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

static_assert([]() consteval {
    constexpr auto sig = get_layout_signature<test_polymorphic::Poly>();
    for (std::size_t i = 0; i < sig.length(); ++i) {
        if (sig.value[i] == 'p' && sig.value[i+1] == 'o' && sig.value[i+2] == 'l') return false;
    }
    return true;
}(), "Polymorphic type Layout should NOT contain 'polymorphic'");

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

// --- Projection relationship tests ---

static_assert([]() consteval {
    constexpr bool def_match = definition_signatures_match<test_nested::Outer1, test_nested::Outer2>();
    constexpr bool lay_match = layout_signatures_match<test_nested::Outer1, test_nested::Outer2>();
    return !def_match || lay_match;  // def_match ⟹ lay_match
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
}(), "Enum signature correct");

static_assert(layout_signatures_match<test_empty_base::WithEmpty, test_empty_base::Plain>(),
    "Empty base class should not affect Layout signature");

int main() {
    std::cout << "=== Two-Layer Signature Tests ===\n\n";

    auto print_sig = [](const char* label, const auto& sig) {
        std::cout << "  " << label << sig << "\n";
    };

    std::cout << "--- Layout Signatures ---\n";
    print_sig("Simple:  ", get_layout_signature<test_basic::Simple>());
    print_sig("Derived: ", get_layout_signature<test_inheritance::Derived>());
    print_sig("Flat:    ", get_layout_signature<test_inheritance::Flat>());

    std::cout << "\n--- Definition Signatures ---\n";
    print_sig("Simple:  ", get_definition_signature<test_basic::Simple>());
    print_sig("Derived: ", get_definition_signature<test_inheritance::Derived>());
    print_sig("Flat:    ", get_definition_signature<test_inheritance::Flat>());

    std::cout << "\n--- Projection ---\n";
    std::cout << "  Derived == Flat (Layout)?      " << (layout_signatures_match<test_inheritance::Derived, test_inheritance::Flat>() ? "YES" : "NO") << "\n";
    std::cout << "  Derived == Flat (Definition)?   " << (definition_signatures_match<test_inheritance::Derived, test_inheritance::Flat>() ? "YES" : "NO") << "\n";

    std::cout << "\nAll tests passed!\n";
    return 0;
}