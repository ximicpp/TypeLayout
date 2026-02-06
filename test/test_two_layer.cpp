// Boost.TypeLayout - Two-Layer Signature System Tests (v2.0)
// Tests Layout (byte-level) and Definition (type definition) signatures
//
// Compile: clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ -I./include test/test_two_layer.cpp

#include <boost/typelayout/core/signature.hpp>
#include <boost/typelayout/core/verification.hpp>
#include <boost/typelayout/core/concepts.hpp>
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;

// =============================================================================
// Test types
// =============================================================================

namespace test_basic {
    struct Simple { int32_t x; double y; };
    struct Simple2 { int32_t a; double b; };  // Same layout, different names
}

namespace test_inheritance {
    struct Base { int32_t id; };
    struct Derived : Base { double value; };
    struct Flat { int32_t id; double value; };
    struct FlatRenamed { int32_t x; double y; };  // Same layout as Flat, different names
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

namespace test_bitfield {
    struct WithBits { int32_t a : 4; int32_t b : 12; int32_t c; };
}

// =============================================================================
// 8. Layout Signature Tests
// =============================================================================

// 8.1 Simple struct Layout signature format — uses "record" prefix, no names
static_assert([]() consteval {
    constexpr auto sig = get_layout_signature<test_basic::Simple>();
    // Should start with arch prefix and "record"
    constexpr auto expected = CompileString{"[64-le]record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}"};
    return sig == expected;
}(), "8.1: Simple struct Layout signature format");

// 8.2 Inherited vs flat: Layout signatures MATCH (inheritance flattened)
static_assert(layout_signatures_match<test_inheritance::Derived, test_inheritance::Flat>(),
    "8.2: Derived and Flat should have identical Layout signatures");

// 8.3 Multi-level inheritance flattening
static_assert(layout_signatures_match<test_multilevel::C, test_multilevel::Flat>(),
    "8.3: Multi-level inheritance should flatten to match flat struct");

// 8.4 Multiple inheritance flattening
static_assert(layout_signatures_match<test_multiple_inheritance::Multi, test_multiple_inheritance::Flat>(),
    "8.4: Multiple inheritance should flatten to match flat struct");

// 8.5 Polymorphic type Layout has NO polymorphic marker
static_assert([]() consteval {
    constexpr auto sig = get_layout_signature<test_polymorphic::Poly>();
    // Layout sig should use "record" without "polymorphic"
    // Just verify it starts correctly and has no "polymorphic" substring
    constexpr auto str = sig.c_str();
    for (std::size_t i = 0; str[i] != '\0'; ++i) {
        if (str[i] == 'p' && str[i+1] == 'o' && str[i+2] == 'l') return false;
    }
    return true;
}(), "8.5: Polymorphic type Layout should NOT contain 'polymorphic'");

// 8.6 Byte array normalization
struct ByteArrayChar { char buf[32]; };
struct ByteArrayU8 { uint8_t buf[32]; };
struct ByteArrayByte { std::byte buf[32]; };
static_assert(layout_signatures_match<ByteArrayChar, ByteArrayU8>(),
    "8.6a: char[] and uint8_t[] should normalize to same Layout signature");
static_assert(layout_signatures_match<ByteArrayChar, ByteArrayByte>(),
    "8.6b: char[] and std::byte[] should normalize to same Layout signature");

// 8.8 Layout hash consistency
static_assert(layout_hashes_match<test_inheritance::Derived, test_inheritance::Flat>(),
    "8.8: Layout hashes should match for layout-identical types");

// =============================================================================
// 9. Definition Signature Tests
// =============================================================================

// 9.1 Simple struct Definition signature — uses "record" prefix, WITH names
static_assert([]() consteval {
    constexpr auto sig = get_definition_signature<test_basic::Simple>();
    constexpr auto expected = CompileString{"[64-le]record[s:16,a:8]{@0[x]:i32[s:4,a:4],@8[y]:f64[s:8,a:8]}"};
    return sig == expected;
}(), "9.1: Simple struct Definition signature format");

// 9.2 Base class subtree preserved: ~base<Name>:record[...]{...}
static_assert([]() consteval {
    constexpr auto sig = get_definition_signature<test_inheritance::Derived>();
    // Should contain "~base<Base>:record"
    constexpr auto str = sig.c_str();
    bool found = false;
    for (std::size_t i = 0; str[i] != '\0' && str[i+1] != '\0'; ++i) {
        if (str[i] == '~' && str[i+1] == 'b' && str[i+2] == 'a' && str[i+3] == 's' && str[i+4] == 'e' && str[i+5] == '<') {
            found = true;
            break;
        }
    }
    return found;
}(), "9.2: Definition should contain ~base<Name>");

// 9.3 Polymorphic type has 'polymorphic' marker in Definition
static_assert([]() consteval {
    constexpr auto sig = get_definition_signature<test_polymorphic::Poly>();
    constexpr auto str = sig.c_str();
    for (std::size_t i = 0; str[i] != '\0'; ++i) {
        if (str[i] == 'p' && str[i+1] == 'o' && str[i+2] == 'l') return true;
    }
    return false;
}(), "9.3: Polymorphic type Definition should contain 'polymorphic'");

// 9.4 Inherited type does NOT have 'inherited' marker
static_assert([]() consteval {
    constexpr auto sig = get_definition_signature<test_inheritance::Derived>();
    constexpr auto str = sig.c_str();
    for (std::size_t i = 0; str[i] != '\0'; ++i) {
        if (str[i] == 'i' && str[i+1] == 'n' && str[i+2] == 'h' && str[i+3] == 'e') return false;
    }
    return true;
}(), "9.4: Inherited type Definition should NOT contain 'inherited'");

// 9.7 Definition hash consistency — same definition → same hash
static_assert(definition_hashes_match<test_nested::Outer1, test_nested::Outer2>(),
    "9.7: Identical definitions should have matching hashes");

// =============================================================================
// 10. Projection Relationship Tests
// =============================================================================

// 10.1 Definition match ⟹ Layout match
static_assert([]() consteval {
    // Outer1 and Outer2 have identical definitions
    constexpr bool def_match = definition_signatures_match<test_nested::Outer1, test_nested::Outer2>();
    constexpr bool lay_match = layout_signatures_match<test_nested::Outer1, test_nested::Outer2>();
    return !def_match || lay_match;  // def_match ⟹ lay_match
}(), "10.1: Definition match implies Layout match");

// 10.2 Layout match but Definition differs (inheritance vs flat)
static_assert(layout_signatures_match<test_inheritance::Derived, test_inheritance::Flat>(),
    "10.2a: Layout should match for Derived vs Flat");
static_assert(!definition_signatures_match<test_inheritance::Derived, test_inheritance::Flat>(),
    "10.2b: Definition should NOT match for Derived vs Flat");

// 10.3 Layout match but Definition differs (field names)
static_assert(layout_signatures_match<test_basic::Simple, test_basic::Simple2>(),
    "10.3a: Layout should match for types with different field names");
static_assert(!definition_signatures_match<test_basic::Simple, test_basic::Simple2>(),
    "10.3b: Definition should NOT match for types with different field names");

// 10.4 Layout differs ⟹ Definition differs
static_assert(!layout_signatures_match<test_basic::Simple, int32_t>(),
    "10.4a: Different layouts should not match");

// =============================================================================
// 11. Regression: basic types, enums, unions, arrays
// =============================================================================

// 11.1 Basic types produce identical Layout and Definition signatures
static_assert(get_layout_signature<int32_t>() == get_definition_signature<int32_t>(),
    "11.1a: i32 Layout == Definition");
static_assert(get_layout_signature<double>() == get_definition_signature<double>(),
    "11.1b: f64 Layout == Definition");
static_assert(get_layout_signature<int32_t*>() == get_definition_signature<int32_t*>(),
    "11.1c: ptr Layout == Definition");

// 11.2 Enum
enum class Color : uint8_t { Red, Green, Blue };
static_assert([]() consteval {
    constexpr auto sig = get_layout_signature<Color>();
    constexpr auto expected = CompileString{"[64-le]enum[s:1,a:1]<u8[s:1,a:1]>"};
    return sig == expected;
}(), "11.2: Enum signature correct");

// 11.3 Nested struct recursion
static_assert([]() consteval {
    constexpr auto sig = get_layout_signature<test_nested::Outer1>();
    // Outer contains Inner (record) + double
    return sig.length() > 0;
}(), "11.3: Nested struct signature generates correctly");

// =============================================================================
// Concepts
// =============================================================================

static_assert(LayoutCompatible<test_inheritance::Derived, test_inheritance::Flat>,
    "LayoutCompatible: Derived and Flat should be layout-compatible");
static_assert(!DefinitionCompatible<test_inheritance::Derived, test_inheritance::Flat>,
    "DefinitionCompatible: Derived and Flat should NOT be definition-compatible");
static_assert(DefinitionCompatible<test_nested::Outer1, test_nested::Outer2>,
    "DefinitionCompatible: Identical definitions should be compatible");
static_assert(LayoutHashCompatible<test_basic::Simple, test_basic::Simple2>,
    "LayoutHashCompatible: Same layout, different names");
static_assert(!DefinitionHashCompatible<test_basic::Simple, test_basic::Simple2>,
    "DefinitionHashCompatible: Different names → different definition hashes");

// =============================================================================
// Macros
// =============================================================================

TYPELAYOUT_ASSERT_LAYOUT_COMPATIBLE(test_inheritance::Derived, test_inheritance::Flat);
TYPELAYOUT_ASSERT_DEFINITION_COMPATIBLE(test_nested::Outer1, test_nested::Outer2);

// =============================================================================
// Empty base optimization
// =============================================================================

static_assert(layout_signatures_match<test_empty_base::WithEmpty, test_empty_base::Plain>(),
    "EBO: Empty base class should not affect Layout signature");

// =============================================================================
// Main: runtime diagnostics
// =============================================================================

int main() {
    std::cout << "=== Two-Layer Signature System Tests ===\n\n";

    std::cout << "--- Layout Signatures (byte-level) ---\n";
    std::cout << "  Simple:    " << get_layout_signature_cstr<test_basic::Simple>() << "\n";
    std::cout << "  Derived:   " << get_layout_signature_cstr<test_inheritance::Derived>() << "\n";
    std::cout << "  Flat:      " << get_layout_signature_cstr<test_inheritance::Flat>() << "\n";
    std::cout << "  Poly:      " << get_layout_signature_cstr<test_polymorphic::Poly>() << "\n";

    std::cout << "\n--- Definition Signatures (type definition) ---\n";
    std::cout << "  Simple:    " << get_definition_signature_cstr<test_basic::Simple>() << "\n";
    std::cout << "  Derived:   " << get_definition_signature_cstr<test_inheritance::Derived>() << "\n";
    std::cout << "  Flat:      " << get_definition_signature_cstr<test_inheritance::Flat>() << "\n";
    std::cout << "  Poly:      " << get_definition_signature_cstr<test_polymorphic::Poly>() << "\n";

    std::cout << "\n--- Projection Relationship ---\n";
    std::cout << "  Derived == Flat (Layout)?      " << (layout_signatures_match<test_inheritance::Derived, test_inheritance::Flat>() ? "YES" : "NO") << "\n";
    std::cout << "  Derived == Flat (Definition)?   " << (definition_signatures_match<test_inheritance::Derived, test_inheritance::Flat>() ? "YES" : "NO") << "\n";
    std::cout << "  Simple == Simple2 (Layout)?     " << (layout_signatures_match<test_basic::Simple, test_basic::Simple2>() ? "YES" : "NO") << "\n";
    std::cout << "  Simple == Simple2 (Definition)?  " << (definition_signatures_match<test_basic::Simple, test_basic::Simple2>() ? "YES" : "NO") << "\n";

    std::cout << "\nAll tests passed!\n";
    return 0;
}
