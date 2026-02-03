// Boost.TypeLayout - Signature Analysis Tool
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// Analyze signature format and identify optimization opportunities
#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;

// Simple 2-field struct
struct Point { int32_t x, y; };

// 10-field struct  
struct Small10 {
    int32_t a1, a2, a3, a4, a5, a6, a7, a8, a9, a10;
};

// 20-field struct
struct Medium20 {
    int32_t a01, a02, a03, a04, a05, a06, a07, a08, a09, a10;
    int32_t a11, a12, a13, a14, a15, a16, a17, a18, a19, a20;
};

// Nested struct
struct Nested {
    Point p1;
    Point p2;
};

// With different types
struct MixedTypes {
    int8_t a;
    int16_t b;
    int32_t c;
    int64_t d;
    float e;
    double f;
};

int main() {
    std::cout << "=== Signature Format Analysis ===" << std::endl;
    
    // 1. Basic int32_t
    std::cout << "\n--- int32_t ---" << std::endl;
    constexpr auto sig_i32 = get_layout_signature<int32_t>();
    std::cout << "Length: " << sig_i32.size << std::endl;
    std::cout << sig_i32.value << std::endl;
    
    // 2. Point (2 fields)
    std::cout << "\n--- Point (2 int32_t) ---" << std::endl;
    constexpr auto sig_point = get_layout_signature<Point>();
    std::cout << "Length: " << sig_point.size << std::endl;
    std::cout << sig_point.value << std::endl;
    
    // 3. Small10 (10 fields)
    std::cout << "\n--- Small10 (10 int32_t) ---" << std::endl;
    constexpr auto sig_small = get_layout_signature<Small10>();
    std::cout << "Length: " << sig_small.size << std::endl;
    std::cout << sig_small.value << std::endl;
    
    // 4. Medium20 (20 fields)
    std::cout << "\n--- Medium20 (20 int32_t) ---" << std::endl;
    constexpr auto sig_medium = get_layout_signature<Medium20>();
    std::cout << "Length: " << sig_medium.size << std::endl;
    std::cout << sig_medium.value << std::endl;
    
    // 5. Nested struct
    std::cout << "\n--- Nested (2 Point) ---" << std::endl;
    constexpr auto sig_nested = get_layout_signature<Nested>();
    std::cout << "Length: " << sig_nested.size << std::endl;
    std::cout << sig_nested.value << std::endl;
    
    // 6. Mixed types
    std::cout << "\n--- MixedTypes (6 different types) ---" << std::endl;
    constexpr auto sig_mixed = get_layout_signature<MixedTypes>();
    std::cout << "Length: " << sig_mixed.size << std::endl;
    std::cout << sig_mixed.value << std::endl;
    
    // Detailed analysis
    std::cout << "\n========================================" << std::endl;
    std::cout << "=== REDUNDANCY ANALYSIS ===" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\n[CURRENT FORMAT] @OFFSET[name]:TYPE[s:SIZE,a:ALIGN]" << std::endl;
    std::cout << "Example: @36[a10]:i32[s:4,a:4]" << std::endl;
    std::cout << "  - '@36': 3 chars (offset)" << std::endl;
    std::cout << "  - '[a10]': 5 chars (field name)" << std::endl;
    std::cout << "  - ':': 1 char" << std::endl;
    std::cout << "  - 'i32[s:4,a:4]': 12 chars (TYPE WITH SIZE/ALIGN)" << std::endl;
    std::cout << "  Total: 21 chars per field" << std::endl;
    
    std::cout << "\n[REDUNDANCY #1] Size/Align info repeated for every field" << std::endl;
    std::cout << "  - 'i32[s:4,a:4]' appears 10 times in Small10" << std::endl;
    std::cout << "  - Each adds 12 chars, total = 120 chars" << std::endl;
    std::cout << "  - BUT: size/align is DERIVABLE from type 'i32'!" << std::endl;
    
    std::cout << "\n[REDUNDANCY #2] Nested struct repeats entire signature" << std::endl;
    std::cout << "  - In Nested, Point signature appears TWICE" << std::endl;
    std::cout << "  - struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}" << std::endl;
    std::cout << "  - 56 chars × 2 = 112 chars" << std::endl;
    
    std::cout << "\n[REDUNDANCY #3] Architecture prefix only needed at top level" << std::endl;
    std::cout << "  - '[64-le]' (7 chars) only at root is correct" << std::endl;
    std::cout << "  - Already optimized!" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "=== OPTIMIZATION PROPOSALS ===" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\n[OPT-1] Remove [s:SIZE,a:ALIGN] from primitive types" << std::endl;
    std::cout << "  BEFORE: i32[s:4,a:4]  (12 chars)" << std::endl;
    std::cout << "  AFTER:  i32           (3 chars)" << std::endl;
    std::cout << "  Savings: 9 chars × N fields" << std::endl;
    std::cout << "  For 100 int32_t: saves 900 chars (~17%)" << std::endl;
    
    std::cout << "\n[OPT-2] Use type references for repeated struct types" << std::endl;
    std::cout << "  BEFORE: @0[p1]:struct{...full...},@8[p2]:struct{...full...}" << std::endl;
    std::cout << "  AFTER:  @0[p1]:$0{...},@8[p2]:$0" << std::endl;
    std::cout << "  Savings: (N-1) × full_signature_length" << std::endl;
    
    std::cout << "\n[OPT-3] Compact offset notation for sequential fields" << std::endl;
    std::cout << "  BEFORE: @0[a1]:i32,@4[a2]:i32,@8[a3]:i32..." << std::endl;
    std::cout << "  AFTER:  @+4[a1,a2,a3...]:i32" << std::endl;
    std::cout << "  (For homogeneous arrays of same-type fields)" << std::endl;
    
    std::cout << "\n[OPT-4] Keep [s:,a:] only at struct level" << std::endl;
    std::cout << "  BEFORE: struct[s:40,a:4]{@0[a1]:i32[s:4,a:4],...}" << std::endl;
    std::cout << "  AFTER:  struct[s:40,a:4]{@0[a1]:i32,...}" << std::endl;
    std::cout << "  Struct size/align already captures the aggregate info" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "=== ESTIMATED SAVINGS ===" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Calculate potential savings
    size_t current_100 = 5183;  // From earlier test
    size_t savings_opt1 = 900;  // Remove [s:4,a:4] from 100 int32
    size_t optimized_100 = current_100 - savings_opt1;
    
    std::cout << "\nFor 100 int32_t fields struct:" << std::endl;
    std::cout << "  Current:    " << current_100 << " chars" << std::endl;
    std::cout << "  After OPT-1: ~" << optimized_100 << " chars (-" << savings_opt1 << ")" << std::endl;
    std::cout << "  Reduction:  ~" << (savings_opt1 * 100 / current_100) << "%" << std::endl;
    
    return 0;
}