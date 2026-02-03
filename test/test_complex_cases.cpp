// Boost.TypeLayout
//
// Complex Cases Test Suite
// Tests for deep nesting, large structures, complex templates, and inheritance
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>
#include <tuple>
#include <variant>
#include <array>
#include <string_view>

using namespace boost::typelayout;

// =========================================================================
// Test Helper Macros
// =========================================================================

#define TEST_SIGNATURE(Type, desc) \
    do { \
        constexpr auto sig = get_layout_signature<Type>(); \
        std::cout << "[PASS] " << desc << std::endl; \
        std::cout << "       sizeof=" << sizeof(Type) \
                  << " alignof=" << alignof(Type) << std::endl; \
        std::cout << "       sig=" << sig.c_str() << std::endl; \
    } while(0)

#define TEST_SECTION(name) \
    std::cout << "\n========== " << name << " ==========\n" << std::endl

#define TEST_RESULT(condition, desc) \
    do { \
        if (condition) { \
            std::cout << "[PASS] " << desc << std::endl; \
        } else { \
            std::cout << "[FAIL] " << desc << std::endl; \
            failed_tests++; \
        } \
    } while(0)

static int failed_tests = 0;

// =========================================================================
// CATEGORY 1: Deep Nesting Test Types
// =========================================================================

// Level 1
struct Level1 {
    int32_t value;
};

// Level 2
struct Level2 {
    Level1 inner;
    int32_t value;
};

// Level 3
struct Level3 {
    Level2 inner;
    int32_t value;
};

// Level 4
struct Level4 {
    Level3 inner;
    int32_t value;
};

// Level 5
struct Level5 {
    Level4 inner;
    int32_t value;
};

// Level 6
struct Level6 {
    Level5 inner;
    int32_t value;
};

// Level 7
struct Level7 {
    Level6 inner;
    int32_t value;
};

// Level 8
struct Level8 {
    Level7 inner;
    int32_t value;
};

// Level 9
struct Level9 {
    Level8 inner;
    int32_t value;
};

// Level 10
struct Level10 {
    Level9 inner;
    int32_t value;
};

// Level 15 (for boundary testing)
struct Level11 { Level10 inner; int32_t value; };
struct Level12 { Level11 inner; int32_t value; };
struct Level13 { Level12 inner; int32_t value; };
struct Level14 { Level13 inner; int32_t value; };
struct Level15 { Level14 inner; int32_t value; };

// =========================================================================
// CATEGORY 2: Large Structure Test Types
// =========================================================================

// 50 members structure
struct Large50 {
    int32_t m01, m02, m03, m04, m05, m06, m07, m08, m09, m10;
    int32_t m11, m12, m13, m14, m15, m16, m17, m18, m19, m20;
    int32_t m21, m22, m23, m24, m25, m26, m27, m28, m29, m30;
    int32_t m31, m32, m33, m34, m35, m36, m37, m38, m39, m40;
    int32_t m41, m42, m43, m44, m45, m46, m47, m48, m49, m50;
};

// 60 members structure (near the limit)
// NOTE: 100 members exceeds constexpr step limit in current Clang P2996
struct Large60 {
    int32_t a01, a02, a03, a04, a05, a06, a07, a08, a09, a10;
    int32_t a11, a12, a13, a14, a15, a16, a17, a18, a19, a20;
    int32_t a21, a22, a23, a24, a25, a26, a27, a28, a29, a30;
    int32_t a31, a32, a33, a34, a35, a36, a37, a38, a39, a40;
    int32_t a41, a42, a43, a44, a45, a46, a47, a48, a49, a50;
    int32_t a51, a52, a53, a54, a55, a56, a57, a58, a59, a60;
};

// Mixed types large structure
struct LargeMixed {
    int8_t   i8_1, i8_2, i8_3, i8_4;
    int16_t  i16_1, i16_2, i16_3, i16_4;
    int32_t  i32_1, i32_2, i32_3, i32_4;
    int64_t  i64_1, i64_2, i64_3, i64_4;
    float    f1, f2, f3, f4;
    double   d1, d2, d3, d4;
    char     c1, c2, c3, c4;
    bool     b1, b2, b3, b4;
    void*    p1;
    void*    p2;
    std::array<int32_t, 10> arr;
};

// =========================================================================
// CATEGORY 3: Complex Template Test Types
// =========================================================================

// Nested tuple types
using NestedTuple2 = std::tuple<int, std::tuple<float, double>>;
using NestedTuple3 = std::tuple<int, std::tuple<float, std::tuple<char, bool>>>;
using NestedTuple4 = std::tuple<int, std::tuple<float, std::tuple<char, std::tuple<short, long>>>>;

// Wide tuple
using WideTuple = std::tuple<int, float, double, char, short, long, bool, int8_t, int16_t, int32_t>;

// CRTP pattern
template<typename Derived>
struct CRTPBase {
    int32_t base_value;
    void interface_method() {
        static_cast<Derived*>(this)->implementation();
    }
};

struct CRTPDerived : CRTPBase<CRTPDerived> {
    int32_t derived_value;
    void implementation() {}
};

// Recursive CRTP
template<typename Derived>
struct CRTPLayer1 {
    int32_t layer1_value;
};

template<typename Derived>
struct CRTPLayer2 : CRTPLayer1<Derived> {
    int32_t layer2_value;
};

struct CRTPMultiLevel : CRTPLayer2<CRTPMultiLevel> {
    int32_t final_value;
};

// Variadic template structure
template<typename... Ts>
struct VariadicHolder {
    std::tuple<Ts...> data;
};

using Variadic5 = VariadicHolder<int, float, double, char, bool>;
using Variadic10 = VariadicHolder<int8_t, int16_t, int32_t, int64_t, float, double, char, bool, short, long>;

// std::variant with many alternatives
using Variant5 = std::variant<int, float, double, char, bool>;
// NOTE: Variant with 10 alternatives exceeds constexpr step limit
using Variant6 = std::variant<int, float, double, char, bool, short>;

// =========================================================================
// CATEGORY 4: Inheritance Complexity Test Types
// =========================================================================

// Diamond inheritance (non-virtual)
struct DiamondTop {
    int32_t top_value;
};

struct DiamondLeft : DiamondTop {
    int32_t left_value;
};

struct DiamondRight : DiamondTop {
    int32_t right_value;
};

struct DiamondBottom : DiamondLeft, DiamondRight {
    int32_t bottom_value;
};

// Diamond inheritance (virtual)
struct VirtualDiamondTop {
    int32_t top_value;
};

struct VirtualDiamondLeft : virtual VirtualDiamondTop {
    int32_t left_value;
};

struct VirtualDiamondRight : virtual VirtualDiamondTop {
    int32_t right_value;
};

struct VirtualDiamondBottom : VirtualDiamondLeft, VirtualDiamondRight {
    int32_t bottom_value;
};

// Deep virtual inheritance chain
struct VChain1 { int32_t v1; };
struct VChain2 : virtual VChain1 { int32_t v2; };
struct VChain3 : virtual VChain2 { int32_t v3; };
struct VChain4 : virtual VChain3 { int32_t v4; };
struct VChain5 : virtual VChain4 { int32_t v5; };

// Multiple virtual bases
struct MVBase1 { int32_t mb1; };
struct MVBase2 { int32_t mb2; };
struct MVBase3 { int32_t mb3; };

struct MultiVirtual : virtual MVBase1, virtual MVBase2, virtual MVBase3 {
    int32_t final_value;
};

// =========================================================================
// Test Functions
// =========================================================================

void test_deep_nesting() {
    TEST_SECTION("Category 1: Deep Nesting Tests");
    
    std::cout << "--- Level 5 Nesting ---" << std::endl;
    TEST_SIGNATURE(Level5, "Level5 (5 nested structs)");
    
    // Verify size accumulation
    constexpr size_t expected_size_5 = 5 * sizeof(int32_t);
    TEST_RESULT(sizeof(Level5) == expected_size_5, 
        "Level5 size matches expected (5 * sizeof(int32_t))");
    
    std::cout << "\n--- Level 10 Nesting ---" << std::endl;
    TEST_SIGNATURE(Level10, "Level10 (10 nested structs)");
    
    constexpr size_t expected_size_10 = 10 * sizeof(int32_t);
    TEST_RESULT(sizeof(Level10) == expected_size_10,
        "Level10 size matches expected (10 * sizeof(int32_t))");
    
    std::cout << "\n--- Level 15 Nesting (Boundary Test) ---" << std::endl;
    TEST_SIGNATURE(Level15, "Level15 (15 nested structs)");
    
    constexpr size_t expected_size_15 = 15 * sizeof(int32_t);
    TEST_RESULT(sizeof(Level15) == expected_size_15,
        "Level15 size matches expected (15 * sizeof(int32_t))");
    
    // Verify signatures are different at each level
    constexpr auto sig5 = get_layout_signature<Level5>();
    constexpr auto sig10 = get_layout_signature<Level10>();
    constexpr auto sig15 = get_layout_signature<Level15>();
    
    TEST_RESULT(sig5 != sig10, "Level5 and Level10 have different signatures");
    TEST_RESULT(sig10 != sig15, "Level10 and Level15 have different signatures");
    
    std::cout << "\n[INFO] Deep nesting tests completed successfully" << std::endl;
}

void test_large_structures() {
    TEST_SECTION("Category 2: Large Structure Tests");
    
    std::cout << "--- 50 Members Structure ---" << std::endl;
    TEST_SIGNATURE(Large50, "Large50 (50 int32_t members)");
    
    constexpr size_t expected_size_50 = 50 * sizeof(int32_t);
    TEST_RESULT(sizeof(Large50) == expected_size_50,
        "Large50 size matches expected (50 * sizeof(int32_t))");
    
    std::cout << "\n--- 60 Members Structure ---" << std::endl;
    TEST_SIGNATURE(Large60, "Large60 (60 int32_t members)");
    
    constexpr size_t expected_size_60 = 60 * sizeof(int32_t);
    TEST_RESULT(sizeof(Large60) == expected_size_60,
        "Large60 size matches expected (60 * sizeof(int32_t))");
    
    std::cout << "\n--- Mixed Types Large Structure ---" << std::endl;
    TEST_SIGNATURE(LargeMixed, "LargeMixed (various types with padding)");
    
    // Verify signatures capture all members
    constexpr auto sig50 = get_layout_signature<Large50>();
    constexpr auto sig60 = get_layout_signature<Large60>();
    
    TEST_RESULT(sig50 != sig60, "Large50 and Large60 have different signatures");
    
    // Document the limitation
    std::cout << "\n[LIMITATION] Structures with 100+ members may exceed constexpr step limit" << std::endl;
    std::cout << "[INFO] Large structure tests completed successfully" << std::endl;
}

void test_complex_templates() {
    TEST_SECTION("Category 3: Complex Template Tests");
    
    std::cout << "--- Nested std::tuple ---" << std::endl;
    TEST_SIGNATURE(NestedTuple2, "NestedTuple2 (2-level tuple nesting)");
    TEST_SIGNATURE(NestedTuple3, "NestedTuple3 (3-level tuple nesting)");
    TEST_SIGNATURE(NestedTuple4, "NestedTuple4 (4-level tuple nesting)");
    
    std::cout << "\n--- Wide std::tuple ---" << std::endl;
    TEST_SIGNATURE(WideTuple, "WideTuple (10 different types)");
    
    std::cout << "\n--- CRTP Pattern ---" << std::endl;
    TEST_SIGNATURE(CRTPDerived, "CRTPDerived (CRTP single level)");
    TEST_SIGNATURE(CRTPMultiLevel, "CRTPMultiLevel (CRTP multi-level)");
    
    std::cout << "\n--- Variadic Templates ---" << std::endl;
    TEST_SIGNATURE(Variadic5, "Variadic5 (5 template parameters)");
    TEST_SIGNATURE(Variadic10, "Variadic10 (10 template parameters)");
    
    std::cout << "\n--- std::variant ---" << std::endl;
    TEST_SIGNATURE(Variant5, "Variant5 (5 alternatives)");
    TEST_SIGNATURE(Variant6, "Variant6 (6 alternatives)");
    // NOTE: Variant with 10+ alternatives exceeds constexpr step limit
    std::cout << "[LIMITATION] std::variant with 10+ alternatives may exceed constexpr step limit" << std::endl;
    
    // Verify nested tuples have different signatures
    constexpr auto sigT2 = get_layout_signature<NestedTuple2>();
    constexpr auto sigT3 = get_layout_signature<NestedTuple3>();
    constexpr auto sigT4 = get_layout_signature<NestedTuple4>();
    
    TEST_RESULT(sigT2 != sigT3, "NestedTuple2 and NestedTuple3 have different signatures");
    TEST_RESULT(sigT3 != sigT4, "NestedTuple3 and NestedTuple4 have different signatures");
    
    std::cout << "\n[INFO] Complex template tests completed successfully" << std::endl;
}

void test_inheritance_complexity() {
    TEST_SECTION("Category 4: Inheritance Complexity Tests");
    
    std::cout << "--- Diamond Inheritance (Non-Virtual) ---" << std::endl;
    TEST_SIGNATURE(DiamondTop, "DiamondTop");
    TEST_SIGNATURE(DiamondLeft, "DiamondLeft");
    TEST_SIGNATURE(DiamondRight, "DiamondRight");
    TEST_SIGNATURE(DiamondBottom, "DiamondBottom (contains 2 copies of DiamondTop)");
    
    // Non-virtual diamond has 2 copies of base
    std::cout << "[INFO] DiamondBottom contains 2 DiamondTop subobjects" << std::endl;
    
    std::cout << "\n--- Diamond Inheritance (Virtual) ---" << std::endl;
    TEST_SIGNATURE(VirtualDiamondTop, "VirtualDiamondTop");
    TEST_SIGNATURE(VirtualDiamondLeft, "VirtualDiamondLeft");
    TEST_SIGNATURE(VirtualDiamondRight, "VirtualDiamondRight");
    TEST_SIGNATURE(VirtualDiamondBottom, "VirtualDiamondBottom (single virtual base)");
    
    // Virtual diamond has 1 copy of base
    std::cout << "[INFO] VirtualDiamondBottom contains 1 VirtualDiamondTop subobject" << std::endl;
    
    std::cout << "\n--- Deep Virtual Inheritance Chain ---" << std::endl;
    TEST_SIGNATURE(VChain5, "VChain5 (5-level virtual inheritance)");
    
    std::cout << "\n--- Multiple Virtual Bases ---" << std::endl;
    TEST_SIGNATURE(MultiVirtual, "MultiVirtual (3 virtual bases)");
    
    // Verify diamond signatures are different
    constexpr auto sigNonVirtual = get_layout_signature<DiamondBottom>();
    constexpr auto sigVirtual = get_layout_signature<VirtualDiamondBottom>();
    
    TEST_RESULT(sigNonVirtual != sigVirtual, 
        "Non-virtual and virtual diamond have different signatures");
    
    std::cout << "\n[INFO] Inheritance complexity tests completed successfully" << std::endl;
}

void test_signature_consistency() {
    TEST_SECTION("Category 5: Signature Consistency Verification");
    
    // Test that same type always produces same signature
    constexpr auto sig1 = get_layout_signature<Level10>();
    constexpr auto sig2 = get_layout_signature<Level10>();
    
    TEST_RESULT(sig1 == sig2, "Same type produces consistent signature");
    
    // Test hash consistency (get_layout_hash returns uint64_t)
    constexpr uint64_t hash1 = get_layout_hash<Level10>();
    constexpr uint64_t hash2 = get_layout_hash<Level10>();
    
    TEST_RESULT(hash1 == hash2, "Same type produces consistent hash");
    std::cout << "[INFO] Level10 hash: 0x" << std::hex << hash1 << std::dec << std::endl;
    
    // Test cross-category comparison
    constexpr auto sigLevel10 = get_layout_signature<Level10>();
    constexpr auto sigLarge50 = get_layout_signature<Large50>();
    constexpr auto sigCRTP = get_layout_signature<CRTPDerived>();
    constexpr auto sigDiamond = get_layout_signature<DiamondBottom>();
    
    TEST_RESULT(sigLevel10 != sigLarge50, "Level10 and Large50 have different signatures");
    TEST_RESULT(sigLarge50 != sigCRTP, "Large50 and CRTPDerived have different signatures");
    TEST_RESULT(sigCRTP != sigDiamond, "CRTPDerived and DiamondBottom have different signatures");
    
    // Test hash uniqueness
    constexpr uint64_t hashLarge50 = get_layout_hash<Large50>();
    constexpr uint64_t hashCRTP = get_layout_hash<CRTPDerived>();
    
    TEST_RESULT(hash1 != hashLarge50, "Level10 and Large50 have different hashes");
    TEST_RESULT(hashLarge50 != hashCRTP, "Large50 and CRTPDerived have different hashes");
    
    std::cout << "\n[INFO] Signature consistency tests completed" << std::endl;
}

// =========================================================================
// Main
// =========================================================================

int main() {
    std::cout << "TypeLayout Complex Cases Test Suite\n" << std::endl;
    std::cout << "Platform: ";
    #if defined(_WIN32)
    std::cout << "Windows ";
    #elif defined(__linux__)
    std::cout << "Linux ";
    #elif defined(__APPLE__)
    std::cout << "macOS ";
    #endif
    std::cout << (sizeof(void*) * 8) << "-bit\n" << std::endl;
    
    // Run all test categories
    test_deep_nesting();
    test_large_structures();
    test_complex_templates();
    test_inheritance_complexity();
    test_signature_consistency();
    
    // Summary
    std::cout << "\n========== TEST SUMMARY ==========\n" << std::endl;
    if (failed_tests == 0) {
        std::cout << "[SUCCESS] All complex case tests passed!" << std::endl;
    } else {
        std::cout << "[FAILURE] " << failed_tests << " test(s) failed." << std::endl;
    }
    
    return failed_tests;
}
