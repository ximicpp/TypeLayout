// Boost.TypeLayout - Signature Mode Tests
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
//
// These tests validate the core guarantee:
//   "Identical Structural Signature ⟺ Identical Memory Layout"
//
// Specifically tests that member names DO NOT affect Structural signatures.

#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;

// ============================================================================
// Test Types: Same layout, different member names
// ============================================================================

// Pair A: Simple POD structs
struct PointA {
    float x;
    float y;
};

struct PointB {
    float horizontal;  // Different name, same type & offset
    float vertical;    // Different name, same type & offset
};

// Pair B: Mixed types with padding
struct RecordA {
    uint32_t id;
    uint64_t timestamp;
    uint16_t flags;
};

struct RecordB {
    uint32_t key;      // Different name
    uint64_t value;    // Different name
    uint16_t status;   // Different name
};

// Pair C: Nested structs
struct InnerA {
    int32_t a;
    int32_t b;
};

struct InnerB {
    int32_t first;   // Different name
    int32_t second;  // Different name
};

struct OuterA {
    InnerA inner;
    float extra;
};

struct OuterB {
    InnerB nested;   // Different field name, structurally identical inner type
    float bonus;     // Different name
};

// Pair D: Arrays
struct ArrayContainerA {
    int32_t data[4];
    uint8_t tag;
};

struct ArrayContainerB {
    int32_t values[4];   // Different name
    uint8_t marker;      // Different name
};

// ============================================================================
// Test execution
// ============================================================================

int main() {
    std::cout << "=== TypeLayout Signature Mode Tests ===\n\n";

    bool all_passed = true;

    // -------------------------------------------------------------------------
    // Test 1: Simple POD - Name Independence
    // -------------------------------------------------------------------------
    std::cout << "Test 1: Simple POD Name Independence\n";
    
    constexpr auto sig_pointA = get_structural_signature<PointA>();
    constexpr auto sig_pointB = get_structural_signature<PointB>();
    
    std::cout << "  PointA (structural): " << sig_pointA.c_str() << "\n";
    std::cout << "  PointB (structural): " << sig_pointB.c_str() << "\n";
    
    static_assert(signatures_match<PointA, PointB>(),
        "FAIL: PointA and PointB should have identical structural signatures");
    
    if (sig_pointA == sig_pointB) {
        std::cout << "  ✓ PASS: Structural signatures match\n";
    } else {
        std::cout << "  ✗ FAIL: Structural signatures differ!\n";
        all_passed = false;
    }

    // Annotated signatures should differ (include names)
    constexpr auto ann_pointA = get_annotated_signature<PointA>();
    constexpr auto ann_pointB = get_annotated_signature<PointB>();
    
    std::cout << "  PointA (annotated): " << ann_pointA.c_str() << "\n";
    std::cout << "  PointB (annotated): " << ann_pointB.c_str() << "\n";
    
    if (ann_pointA != ann_pointB) {
        std::cout << "  ✓ PASS: Annotated signatures differ (expected)\n";
    } else {
        std::cout << "  ✗ FAIL: Annotated signatures should include names!\n";
        all_passed = false;
    }

    std::cout << "\n";

    // -------------------------------------------------------------------------
    // Test 2: Mixed Types with Padding
    // -------------------------------------------------------------------------
    std::cout << "Test 2: Mixed Types with Padding\n";
    
    constexpr auto sig_recordA = get_structural_signature<RecordA>();
    constexpr auto sig_recordB = get_structural_signature<RecordB>();
    
    std::cout << "  RecordA: " << sig_recordA.c_str() << "\n";
    std::cout << "  RecordB: " << sig_recordB.c_str() << "\n";
    
    static_assert(signatures_match<RecordA, RecordB>(),
        "FAIL: RecordA and RecordB should have identical structural signatures");
    
    static_assert(hashes_match<RecordA, RecordB>(),
        "FAIL: RecordA and RecordB should have identical hashes");

    if (sig_recordA == sig_recordB) {
        std::cout << "  ✓ PASS: Structural signatures match\n";
    } else {
        std::cout << "  ✗ FAIL!\n";
        all_passed = false;
    }

    std::cout << "\n";

    // -------------------------------------------------------------------------
    // Test 3: Nested Structs
    // -------------------------------------------------------------------------
    std::cout << "Test 3: Nested Structs\n";
    
    constexpr auto sig_outerA = get_structural_signature<OuterA>();
    constexpr auto sig_outerB = get_structural_signature<OuterB>();
    
    std::cout << "  OuterA: " << sig_outerA.c_str() << "\n";
    std::cout << "  OuterB: " << sig_outerB.c_str() << "\n";
    
    static_assert(signatures_match<OuterA, OuterB>(),
        "FAIL: OuterA and OuterB should have identical structural signatures");

    if (sig_outerA == sig_outerB) {
        std::cout << "  ✓ PASS: Nested struct signatures match\n";
    } else {
        std::cout << "  ✗ FAIL!\n";
        all_passed = false;
    }

    std::cout << "\n";

    // -------------------------------------------------------------------------
    // Test 4: Array Containers
    // -------------------------------------------------------------------------
    std::cout << "Test 4: Array Containers\n";
    
    constexpr auto sig_arrayA = get_structural_signature<ArrayContainerA>();
    constexpr auto sig_arrayB = get_structural_signature<ArrayContainerB>();
    
    std::cout << "  ArrayContainerA: " << sig_arrayA.c_str() << "\n";
    std::cout << "  ArrayContainerB: " << sig_arrayB.c_str() << "\n";
    
    static_assert(signatures_match<ArrayContainerA, ArrayContainerB>(),
        "FAIL: ArrayContainerA and ArrayContainerB should match");

    if (sig_arrayA == sig_arrayB) {
        std::cout << "  ✓ PASS: Array container signatures match\n";
    } else {
        std::cout << "  ✗ FAIL!\n";
        all_passed = false;
    }

    std::cout << "\n";

    // -------------------------------------------------------------------------
    // Test 5: Hash Consistency
    // -------------------------------------------------------------------------
    std::cout << "Test 5: Hash Consistency\n";
    
    constexpr uint64_t hash_pointA = get_layout_hash<PointA>();
    constexpr uint64_t hash_pointB = get_layout_hash<PointB>();
    constexpr uint64_t hash_recordA = get_layout_hash<RecordA>();
    constexpr uint64_t hash_recordB = get_layout_hash<RecordB>();
    
    std::cout << "  PointA hash:  0x" << std::hex << hash_pointA << "\n";
    std::cout << "  PointB hash:  0x" << hash_pointB << "\n";
    std::cout << "  RecordA hash: 0x" << hash_recordA << "\n";
    std::cout << "  RecordB hash: 0x" << hash_recordB << std::dec << "\n";
    
    static_assert(hash_pointA == hash_pointB, "Hash mismatch for Point types");
    static_assert(hash_recordA == hash_recordB, "Hash mismatch for Record types");
    
    if (hash_pointA == hash_pointB && hash_recordA == hash_recordB) {
        std::cout << "  ✓ PASS: Hashes are name-independent\n";
    } else {
        std::cout << "  ✗ FAIL!\n";
        all_passed = false;
    }

    std::cout << "\n";

    // -------------------------------------------------------------------------
    // Test 6: Concept Usage
    // -------------------------------------------------------------------------
    std::cout << "Test 6: Concept Usage\n";
    
    static_assert(LayoutCompatible<PointA, PointB>, 
        "LayoutCompatible concept should pass for same-layout types");
    static_assert(LayoutHashCompatible<RecordA, RecordB>,
        "LayoutHashCompatible concept should pass");
    
    std::cout << "  ✓ PASS: Concepts work with name-independent signatures\n";

    std::cout << "\n";

    // -------------------------------------------------------------------------
    // Final Summary
    // -------------------------------------------------------------------------
    std::cout << "========================================\n";
    if (all_passed) {
        std::cout << "ALL TESTS PASSED ✓\n";
        std::cout << "Core guarantee validated: Structural signatures are name-independent.\n";
        return 0;
    } else {
        std::cout << "SOME TESTS FAILED ✗\n";
        return 1;
    }
}
