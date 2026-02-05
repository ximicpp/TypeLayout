// Boost.TypeLayout - Compile-Time Benchmark Suite
//
// This file measures the compile-time overhead of TypeLayout signature generation.
// It generates types of varying complexity and measures instantiation time.
//
// Usage:
//   # Measure compile time (wall clock)
//   time clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
//       -DBENCH_SIMPLE bench_compile_time.cpp -o /dev/null
//
//   time clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
//       -DBENCH_COMPLEX bench_compile_time.cpp -o /dev/null
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout.hpp>
#include <cstdint>

using namespace boost::typelayout;

// =============================================================================
// Simple Types (5 members) - Baseline
// =============================================================================

#ifdef BENCH_SIMPLE

struct Simple1 { int a; float b; double c; char d; short e; };
struct Simple2 { uint32_t x; uint64_t y; float z; int w; char v; };
struct Simple3 { double d1; double d2; int i1; int i2; char c1; };
struct Simple4 { int arr[4]; float f; };
struct Simple5 { char name[16]; int id; float score; };

// Generate signatures for all simple types
constexpr auto sig1 = get_layout_signature<Simple1>();
constexpr auto sig2 = get_layout_signature<Simple2>();
constexpr auto sig3 = get_layout_signature<Simple3>();
constexpr auto sig4 = get_layout_signature<Simple4>();
constexpr auto sig5 = get_layout_signature<Simple5>();

// Generate hashes
constexpr auto hash1 = get_layout_hash<Simple1>();
constexpr auto hash2 = get_layout_hash<Simple2>();
constexpr auto hash3 = get_layout_hash<Simple3>();
constexpr auto hash4 = get_layout_hash<Simple4>();
constexpr auto hash5 = get_layout_hash<Simple5>();

// Verify no collisions
static_assert(no_hash_collision<Simple1, Simple2, Simple3, Simple4, Simple5>());

#endif // BENCH_SIMPLE

// =============================================================================
// Medium Types (20 members)
// =============================================================================

#ifdef BENCH_MEDIUM

struct Medium1 {
    int a1, a2, a3, a4, a5;
    float b1, b2, b3, b4, b5;
    double c1, c2, c3, c4, c5;
    char d1, d2, d3, d4, d5;
};

struct Medium2 {
    uint32_t x1, x2, x3, x4, x5;
    uint64_t y1, y2, y3, y4, y5;
    int16_t z1, z2, z3, z4, z5;
    int8_t w1, w2, w3, w4, w5;
};

struct Medium3 {
    double arr1[5];
    float arr2[5];
    int arr3[5];
    char arr4[5];
};

constexpr auto sig_m1 = get_layout_signature<Medium1>();
constexpr auto sig_m2 = get_layout_signature<Medium2>();
constexpr auto sig_m3 = get_layout_signature<Medium3>();

constexpr auto hash_m1 = get_layout_hash<Medium1>();
constexpr auto hash_m2 = get_layout_hash<Medium2>();
constexpr auto hash_m3 = get_layout_hash<Medium3>();

static_assert(no_hash_collision<Medium1, Medium2, Medium3>());

#endif // BENCH_MEDIUM

// =============================================================================
// Complex Types (30-40 members - below constexpr step limit)
// =============================================================================

#ifdef BENCH_COMPLEX

struct Complex1 {
    // 30 int fields
    int i01, i02, i03, i04, i05, i06, i07, i08, i09, i10;
    int i11, i12, i13, i14, i15, i16, i17, i18, i19, i20;
    int i21, i22, i23, i24, i25, i26, i27, i28, i29, i30;
};

struct Complex2 {
    // Mixed types, 35 members
    double d01, d02, d03, d04, d05, d06, d07;
    float f01, f02, f03, f04, f05, f06, f07;
    int i01, i02, i03, i04, i05, i06, i07;
    uint64_t u01, u02, u03, u04, u05, u06, u07;
    char c01, c02, c03, c04, c05, c06, c07;
};

struct Complex3 {
    // Arrays
    int arr1[8];
    float arr2[8];
    double arr3[8];
    char arr4[8];
};

// Inheritance
struct Base1 { int x; float y; };
struct Base2 { double z; char w; };
struct Complex4 : Base1, Base2 {
    int a1, a2, a3, a4, a5;
    float b1, b2, b3, b4, b5;
    double c1, c2, c3, c4, c5;
};

constexpr auto sig_c1 = get_layout_signature<Complex1>();
constexpr auto sig_c2 = get_layout_signature<Complex2>();
constexpr auto sig_c3 = get_layout_signature<Complex3>();
constexpr auto sig_c4 = get_layout_signature<Complex4>();

constexpr auto hash_c1 = get_layout_hash<Complex1>();
constexpr auto hash_c2 = get_layout_hash<Complex2>();
constexpr auto hash_c3 = get_layout_hash<Complex3>();
constexpr auto hash_c4 = get_layout_hash<Complex4>();

// Note: Skip no_hash_collision for complex types to avoid constexpr step limit
// Individual hash generation is sufficient for benchmarking

#endif // BENCH_COMPLEX

// =============================================================================
// Very Large Types (40 members - maximum before hitting constexpr limit)
// =============================================================================

#ifdef BENCH_VERY_LARGE

struct VeryLarge1 {
    // 40 int fields - practical maximum
    int i01, i02, i03, i04, i05, i06, i07, i08, i09, i10;
    int i11, i12, i13, i14, i15, i16, i17, i18, i19, i20;
    int i21, i22, i23, i24, i25, i26, i27, i28, i29, i30;
    int i31, i32, i33, i34, i35, i36, i37, i38, i39, i40;
};

struct VeryLarge2 {
    // Mixed large struct
    double d01, d02, d03, d04, d05, d06, d07, d08, d09, d10;
    float f01, f02, f03, f04, f05, f06, f07, f08, f09, f10;
    int i01, i02, i03, i04, i05, i06, i07, i08, i09, i10;
    uint64_t u01, u02, u03, u04, u05, u06, u07, u08, u09, u10;
};

constexpr auto sig_vl1 = get_layout_signature<VeryLarge1>();
constexpr auto sig_vl2 = get_layout_signature<VeryLarge2>();

constexpr auto hash_vl1 = get_layout_hash<VeryLarge1>();
constexpr auto hash_vl2 = get_layout_hash<VeryLarge2>();

#endif // BENCH_VERY_LARGE

// =============================================================================
// All Benchmarks Combined (for total time measurement)
// =============================================================================

#ifdef BENCH_ALL

// Include all types from above...
struct AllSimple1 { int a; float b; double c; char d; short e; };
struct AllMedium1 {
    int a1, a2, a3, a4, a5;
    float b1, b2, b3, b4, b5;
    double c1, c2, c3, c4, c5;
    char d1, d2, d3, d4, d5;
};
struct AllComplex1 {
    int i01, i02, i03, i04, i05, i06, i07, i08, i09, i10;
    int i11, i12, i13, i14, i15, i16, i17, i18, i19, i20;
    int i21, i22, i23, i24, i25, i26, i27, i28, i29, i30;
    int i31, i32, i33, i34, i35, i36, i37, i38, i39, i40;
    int i41, i42, i43, i44, i45, i46, i47, i48, i49, i50;
};

constexpr auto all_sig1 = get_layout_signature<AllSimple1>();
constexpr auto all_sig2 = get_layout_signature<AllMedium1>();
constexpr auto all_sig3 = get_layout_signature<AllComplex1>();

constexpr auto all_hash1 = get_layout_hash<AllSimple1>();
constexpr auto all_hash2 = get_layout_hash<AllMedium1>();
constexpr auto all_hash3 = get_layout_hash<AllComplex1>();

static_assert(no_hash_collision<AllSimple1, AllMedium1, AllComplex1>());

#endif // BENCH_ALL

// =============================================================================
// Main (minimal - just ensures compilation succeeds)
// =============================================================================

int main() {
    return 0;
}
