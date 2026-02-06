// Boost.TypeLayout
//
// Stress Test Suite - Large Struct Support
// Tests the current limits of the P2996 toolchain for large structures.
//
// KNOWN LIMITATION: Current P2996 toolchain (Bloomberg experimental Clang)
// has constexpr step limits that restrict the maximum number of members
// that can be processed at compile-time. This limit is approximately 40-50
// members for signature generation and hashing.
//
// This test file documents the current working limits and serves as a
// regression test for when the toolchain improves.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/typelayout.hpp>
#include <cstdint>
#include <iostream>

using namespace boost::typelayout;

// ============================================================================
// Test 1: 20 Member Struct (baseline - should always work)
// ============================================================================

struct Stress20 {
    int32_t m00, m01, m02, m03, m04, m05, m06, m07, m08, m09;
    int32_t m10, m11, m12, m13, m14, m15, m16, m17, m18, m19;
};

static_assert(sizeof(Stress20) == 20 * sizeof(int32_t), "Stress20 should have 20 members");

// Test 1.1: Hash computation must succeed
static_assert(get_layout_hash<Stress20>() != 0, "Stress20 hash must compile");

// Test 1.2: Member count verification
static_assert(get_member_count<Stress20>() == 20, "Stress20 should have 20 members");

// ============================================================================
// Test 2: 30 Member Struct (near current limit)
// ============================================================================

struct Stress30 {
    int32_t m00, m01, m02, m03, m04, m05, m06, m07, m08, m09;
    int32_t m10, m11, m12, m13, m14, m15, m16, m17, m18, m19;
    int32_t m20, m21, m22, m23, m24, m25, m26, m27, m28, m29;
};

static_assert(sizeof(Stress30) == 30 * sizeof(int32_t), "Stress30 should have 30 members");

// Test 2.1: Hash computation must succeed
static_assert(get_layout_hash<Stress30>() != 0, "Stress30 hash must compile");

// Test 2.2: Member count verification
static_assert(get_member_count<Stress30>() == 30, "Stress30 should have 30 members");

// ============================================================================
// Test 3: 40 Member Struct (at current limit)
// ============================================================================

struct Stress40 {
    int32_t m00, m01, m02, m03, m04, m05, m06, m07, m08, m09;
    int32_t m10, m11, m12, m13, m14, m15, m16, m17, m18, m19;
    int32_t m20, m21, m22, m23, m24, m25, m26, m27, m28, m29;
    int32_t m30, m31, m32, m33, m34, m35, m36, m37, m38, m39;
};

static_assert(sizeof(Stress40) == 40 * sizeof(int32_t), "Stress40 should have 40 members");

// Test 3.1: Hash computation must succeed
static_assert(get_layout_hash<Stress40>() != 0, "Stress40 hash must compile");

// Test 3.2: Member count verification
static_assert(get_member_count<Stress40>() == 40, "Stress40 should have 40 members");

// ============================================================================
// Test 4: Equivalence Tests (different names, same layout)
// ============================================================================

struct Stress20Alt {
    int32_t n00, n01, n02, n03, n04, n05, n06, n07, n08, n09;
    int32_t n10, n11, n12, n13, n14, n15, n16, n17, n18, n19;
};

// Same layout, different member names - must have same hash
static_assert(hashes_match<Stress20, Stress20Alt>(), 
              "Stress20 and Stress20Alt must have same layout hash");

// ============================================================================
// Test 5: Mixed Type Stress Test (20 members with various types)
// ============================================================================

struct MixedStress20 {
    int8_t   m00; int16_t  m01; int32_t  m02; int64_t  m03; float    m04;
    double   m05; int8_t   m06; int16_t  m07; int32_t  m08; int64_t  m09;
    float    m10; double   m11; int8_t   m12; int16_t  m13; int32_t  m14;
    int64_t  m15; float    m16; double   m17; int8_t   m18; int16_t  m19;
};

// Test mixed types compile
static_assert(get_layout_hash<MixedStress20>() != 0, "MixedStress20 hash must compile");
static_assert(get_member_count<MixedStress20>() == 20, "MixedStress20 should have 20 members");

// ============================================================================
// Main - Runtime Output
// ============================================================================

int main() {
    std::cout << "=== TypeLayout Stress Test Suite ===\n\n";
    
    std::cout << "--- Stress20 (20 members) ---\n";
    std::cout << "Size: " << sizeof(Stress20) << " bytes\n";
    std::cout << "Hash: 0x" << std::hex << get_layout_hash<Stress20>() << std::dec << "\n\n";
    
    std::cout << "--- Stress30 (30 members) ---\n";
    std::cout << "Size: " << sizeof(Stress30) << " bytes\n";
    std::cout << "Hash: 0x" << std::hex << get_layout_hash<Stress30>() << std::dec << "\n\n";
    
    std::cout << "--- Stress40 (40 members) ---\n";
    std::cout << "Size: " << sizeof(Stress40) << " bytes\n";
    std::cout << "Hash: 0x" << std::hex << get_layout_hash<Stress40>() << std::dec << "\n\n";
    
    std::cout << "--- MixedStress20 (20 mixed-type members) ---\n";
    std::cout << "Size: " << sizeof(MixedStress20) << " bytes\n";
    std::cout << "Hash: 0x" << std::hex << get_layout_hash<MixedStress20>() << std::dec << "\n\n";
    
    std::cout << "=== All stress tests passed! ===\n";
    std::cout << "\nNote: Current P2996 toolchain has constexpr step limits.\n";
    std::cout << "Structures with >50 members may require toolchain updates.\n";
    
    return 0;
}