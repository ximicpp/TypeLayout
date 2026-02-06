// Physical Mode Tests
// Test cases for the Physical layer (inheritance flattening)

#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstring>

using namespace boost::typelayout;

// ============================================
// Test 1: Inheritance flattening — Physical matches, Structural doesn't
// ============================================
namespace test_inheritance_flattening {
    struct Base { int x; };
    struct Derived : Base { double y; };
    struct Flat { int x; double y; };

    // Physical layer: layout identical → match
    static_assert(physical_signatures_match<Derived, Flat>(),
        "Physical: Derived and Flat should match");

    // Structural layer: different structure → no match
    static_assert(!signatures_match<Derived, Flat>(),
        "Structural: Derived and Flat should NOT match");

    // Verify sizes are identical
    static_assert(sizeof(Derived) == sizeof(Flat),
        "Derived and Flat should have same size");
}

// ============================================
// Test 2: Multi-level inheritance flattening
// ============================================
namespace test_multilevel {
    struct A { int x; };
    struct B : A { int y; };
    struct C : B { int z; };
    struct Flat { int x; int y; int z; };

    static_assert(physical_signatures_match<C, Flat>(),
        "Physical: C and Flat should match");
    static_assert(!signatures_match<C, Flat>(),
        "Structural: C and Flat should NOT match");
}

// ============================================
// Test 3: Multiple inheritance flattening
// ============================================
namespace test_multiple_inheritance {
    struct MA { int x; };
    struct MB { double y; };
    struct MC : MA, MB { char z; };
    struct MFlat { int x; double y; char z; };

    static_assert(physical_signatures_match<MC, MFlat>(),
        "Physical: MC and MFlat should match");
}

// ============================================
// Test 4: Plain struct — both layers match
// ============================================
namespace test_plain_struct {
    struct Plain1 { int a; double b; };
    struct Plain2 { int a; double b; };

    static_assert(physical_signatures_match<Plain1, Plain2>(),
        "Physical: Plain1 and Plain2 should match");
    static_assert(signatures_match<Plain1, Plain2>(),
        "Structural: Plain1 and Plain2 should match");
}

// ============================================
// Test 5: Polymorphic type — Physical has no polymorphic marker
// ============================================
namespace test_polymorphic {
    struct Poly { virtual void f() {} int x; };
    struct NonPoly { void* vptr; int x; };  // Simulate vtable manually

    // Different layouts (vtable pointer position may differ)
    // Just verify that Physical signature doesn't contain "polymorphic"
    constexpr auto poly_phys = get_physical_signature<Poly>();
    constexpr auto poly_struct = get_structural_signature<Poly>();

    // Physical uses "record" prefix, Structural uses "class" with "polymorphic"
    static_assert(poly_phys.c_str()[8] == 'r', "Physical should use 'record' prefix");
}

// ============================================
// Test 6: Physical signature format verification
// ============================================
namespace test_format {
    struct Simple { int x; double y; };

    constexpr auto phys = get_physical_signature<Simple>();
    constexpr auto struc = get_structural_signature<Simple>();

    // Physical uses "record", Structural uses "struct"
    // They should NOT be equal
    static_assert(!(phys == struc), "Physical and Structural signatures should differ");
}

// ============================================
// Test 7: Backward compatibility — default is Structural
// ============================================
namespace test_backward_compat {
    struct TestType { int x; double y; };

    static_assert(get_layout_signature<TestType>() == get_structural_signature<TestType>(),
        "Default mode should be Structural");
}

// ============================================
// Test 8: Physical hash consistency
// ============================================
namespace test_hash {
    struct Base { int x; };
    struct Derived : Base { double y; };
    struct Flat { int x; double y; };

    static_assert(physical_hashes_match<Derived, Flat>(),
        "Physical hashes should match");
    static_assert(!hashes_match<Derived, Flat>(),
        "Structural hashes should NOT match");
}

// ============================================
// Test 9: Physical concepts
// ============================================
namespace test_concepts {
    struct Base { int x; };
    struct Derived : Base { double y; };
    struct Flat { int x; double y; };

    static_assert(PhysicalLayoutCompatible<Derived, Flat>,
        "PhysicalLayoutCompatible should be satisfied");
    static_assert(!LayoutCompatible<Derived, Flat>,
        "LayoutCompatible should NOT be satisfied");
}

// ============================================
// Test 10: Physical verification
// ============================================
namespace test_verification {
    struct Base { int x; };
    struct Derived : Base { double y; };
    struct Flat { int x; double y; };

    static_assert(physical_verifications_match<Derived, Flat>(),
        "Physical verifications should match");
    static_assert(!verifications_match<Derived, Flat>(),
        "Structural verifications should NOT match");
}

// ============================================
// Test 11: Empty base class (EBO)
// ============================================
namespace test_ebo {
    struct Empty {};
    struct WithEmpty : Empty { int x; };
    struct Plain { int x; };

    // EBO means Empty doesn't occupy space
    static_assert(sizeof(WithEmpty) == sizeof(Plain),
        "Empty base should not add size");
    
    // Physical signatures should match
    static_assert(physical_signatures_match<WithEmpty, Plain>(),
        "Physical: WithEmpty and Plain should match with EBO");
}

// ============================================
// Test 12: Nested struct (not flattened)
// ============================================
namespace test_nested {
    struct Inner { int x; };
    struct Outer1 { Inner inner; double y; };
    struct Outer2 { Inner inner; double y; };

    // Same structure → match in both modes
    static_assert(physical_signatures_match<Outer1, Outer2>(),
        "Physical: identical nested structs should match");
    static_assert(signatures_match<Outer1, Outer2>(),
        "Structural: identical nested structs should match");
}

int main() {
    std::cout << "=== Physical Mode Test Results ===\n\n";

    // Print some signature examples
    std::cout << "Test 1: Inheritance Flattening\n";
    std::cout << "  Derived Physical:   " << get_physical_signature<test_inheritance_flattening::Derived>().c_str() << "\n";
    std::cout << "  Flat Physical:      " << get_physical_signature<test_inheritance_flattening::Flat>().c_str() << "\n";
    std::cout << "  Derived Structural: " << get_structural_signature<test_inheritance_flattening::Derived>().c_str() << "\n";
    std::cout << "  Flat Structural:    " << get_structural_signature<test_inheritance_flattening::Flat>().c_str() << "\n";
    std::cout << "\n";

    std::cout << "Test 2: Multi-level Inheritance\n";
    std::cout << "  C Physical:    " << get_physical_signature<test_multilevel::C>().c_str() << "\n";
    std::cout << "  Flat Physical: " << get_physical_signature<test_multilevel::Flat>().c_str() << "\n";
    std::cout << "\n";

    std::cout << "Test 5: Polymorphic Type\n";
    std::cout << "  Poly Physical:    " << get_physical_signature<test_polymorphic::Poly>().c_str() << "\n";
    std::cout << "  Poly Structural:  " << get_structural_signature<test_polymorphic::Poly>().c_str() << "\n";
    std::cout << "\n";

    std::cout << "All static assertions passed!\n";
    return 0;
}
