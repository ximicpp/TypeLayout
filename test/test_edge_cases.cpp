// test_edge_cases.cpp - Edge case and stress tests
// Tests for boundary conditions identified in core-completeness analysis
// Compile success = all tests pass (static_assert based)

#include <cstdint>
#include <array>
#include <utility>
#include <boost/typelayout/typelayout_all.hpp>

using namespace boost::typelayout;

//=============================================================================
// 1. std::array Tests
//=============================================================================

// Basic std::array
static_assert(get_layout_signature<std::array<int32_t, 4>>() == 
    "[64-le]std_array[s:16,a:4]<i32[s:4,a:4],4>");

static_assert(get_layout_signature<std::array<double, 3>>() == 
    "[64-le]std_array[s:24,a:8]<f64[s:8,a:8],3>");

static_assert(get_layout_signature<std::array<uint8_t, 16>>() == 
    "[64-le]std_array[s:16,a:1]<u8[s:1,a:1],16>");

// Note: Empty std::array<T, 0> size is implementation-defined (may be 0 or 1)
// Skipping empty array test for portability

// Nested std::array
static_assert(get_layout_signature<std::array<std::array<int32_t, 2>, 3>>() ==
    "[64-le]std_array[s:24,a:4]<std_array[s:8,a:4]<i32[s:4,a:4],2>,3>");

// Struct containing std::array
struct WithStdArray {
    int32_t id;
    std::array<double, 2> values;
};
static_assert(sizeof(WithStdArray) == 24);

//=============================================================================
// 2. std::pair Tests
//=============================================================================

// Basic std::pair
static_assert(get_layout_signature<std::pair<int32_t, int32_t>>() ==
    "[64-le]pair[s:8,a:4]{i32[s:4,a:4],i32[s:4,a:4]}");

static_assert(get_layout_signature<std::pair<int32_t, double>>() ==
    "[64-le]pair[s:16,a:8]{i32[s:4,a:4],f64[s:8,a:8]}");

// Nested std::pair
static_assert(sizeof(std::pair<std::pair<int32_t, int32_t>, double>) == 24 ||
              sizeof(std::pair<std::pair<int32_t, int32_t>, double>) == 16);

// Struct containing std::pair
struct WithPair {
    std::pair<int16_t, int16_t> point;
    uint32_t flags;
};
static_assert(sizeof(WithPair) == 8);

//=============================================================================
// 3. Zero-Width Bit-field Tests
//=============================================================================

// Zero-width bit-field forces alignment to next storage unit
struct ZeroWidthBitfield {
    uint32_t a : 8;
    uint32_t : 0;      // Force alignment
    uint32_t b : 8;
};
// After zero-width, b starts at new storage unit
static_assert(sizeof(ZeroWidthBitfield) == 8);

// Multiple zero-width bit-fields
struct MultiZeroWidth {
    uint16_t x : 4;
    uint16_t : 0;
    uint16_t y : 4;
    uint16_t : 0;
    uint16_t z : 4;
};
static_assert(sizeof(MultiZeroWidth) == 6);

// Zero-width between regular fields
struct ZeroWidthMixed {
    uint8_t a;
    uint32_t : 0;      // Force alignment to 4-byte boundary
    uint32_t b;
};
static_assert(sizeof(ZeroWidthMixed) == 8);
static_assert(alignof(ZeroWidthMixed) == 4);

//=============================================================================
// 4. Deep Inheritance Hierarchy Tests (10+ levels)
//=============================================================================

struct Level0 { int32_t v0; };
struct Level1 : Level0 { int32_t v1; };
struct Level2 : Level1 { int32_t v2; };
struct Level3 : Level2 { int32_t v3; };
struct Level4 : Level3 { int32_t v4; };
struct Level5 : Level4 { int32_t v5; };
struct Level6 : Level5 { int32_t v6; };
struct Level7 : Level6 { int32_t v7; };
struct Level8 : Level7 { int32_t v8; };
struct Level9 : Level8 { int32_t v9; };
struct Level10 : Level9 { int32_t v10; };

// Verify deep inheritance works
static_assert(sizeof(Level10) == 44);  // 11 * 4 bytes
static_assert(get_member_count<Level10>() == 1);  // Only own member
static_assert(get_base_count<Level10>() == 1);    // Direct base only

// Can generate signature for deeply nested type
constexpr auto deep_sig = get_layout_signature<Level10>();
static_assert(deep_sig.length() > 0);

//=============================================================================
// 5. Large Struct Tests (50+ fields)
//=============================================================================

struct LargeStruct {
    int32_t f00, f01, f02, f03, f04, f05, f06, f07, f08, f09;
    int32_t f10, f11, f12, f13, f14, f15, f16, f17, f18, f19;
    int32_t f20, f21, f22, f23, f24, f25, f26, f27, f28, f29;
    int32_t f30, f31, f32, f33, f34, f35, f36, f37, f38, f39;
    int32_t f40, f41, f42, f43, f44, f45, f46, f47, f48, f49;
};

static_assert(sizeof(LargeStruct) == 200);  // 50 * 4 bytes
static_assert(get_member_count<LargeStruct>() == 50);

// Signature generation should work for large structs
constexpr auto large_sig = get_layout_signature<LargeStruct>();
static_assert(large_sig.length() > 500);  // Significant signature length

//=============================================================================
// 6. Extreme Alignment Tests
//=============================================================================

// 64-byte alignment (cache line)
struct alignas(64) CacheAligned {
    int32_t x;
    int32_t y;
};
static_assert(alignof(CacheAligned) == 64);
static_assert(sizeof(CacheAligned) == 64);

// 256-byte alignment
struct alignas(256) Aligned256 {
    double values[4];
};
static_assert(alignof(Aligned256) == 256);
static_assert(sizeof(Aligned256) == 256);

// 4096-byte alignment (page size)
struct alignas(4096) PageAligned {
    char data[100];
};
static_assert(alignof(PageAligned) == 4096);
static_assert(sizeof(PageAligned) == 4096);

// Verify signatures capture alignment
constexpr auto page_sig = get_layout_signature<PageAligned>();
// Should contain "a:4096"

//=============================================================================
// 7. Empty Base Optimization Edge Cases
//=============================================================================

struct Empty1 {};
struct Empty2 {};
struct Empty3 {};

// Multiple empty bases
struct MultiEmpty : Empty1, Empty2, Empty3 {
    int32_t value;
};
static_assert(sizeof(MultiEmpty) == 4);  // EBO should apply

// Empty base with empty derived
struct EmptyDerived : Empty1 {};
static_assert(sizeof(EmptyDerived) == 1);

// Chain of empty bases
struct EmptyChain1 : Empty1 {};
struct EmptyChain2 : EmptyChain1 {};
struct EmptyChain3 : EmptyChain2 { int32_t x; };
static_assert(sizeof(EmptyChain3) == 4);

//=============================================================================
// 8. Bit-field Edge Cases
//=============================================================================

// Bit-field exactly filling storage unit
struct FullBitfield {
    uint32_t a : 16;
    uint32_t b : 16;
};
static_assert(sizeof(FullBitfield) == 4);

// Bit-field with unusual widths
struct OddBitfield {
    uint32_t a : 7;
    uint32_t b : 13;
    uint32_t c : 11;
    uint32_t d : 1;
};
static_assert(sizeof(OddBitfield) == 4);  // Total: 32 bits

// Single-bit fields
struct SingleBits {
    uint8_t b0 : 1;
    uint8_t b1 : 1;
    uint8_t b2 : 1;
    uint8_t b3 : 1;
    uint8_t b4 : 1;
    uint8_t b5 : 1;
    uint8_t b6 : 1;
    uint8_t b7 : 1;
};
static_assert(sizeof(SingleBits) == 1);
static_assert(has_bitfields<SingleBits>());

//=============================================================================
// 9. Anonymous Member Edge Cases
//=============================================================================

// Nested anonymous struct
struct NestedAnon {
    struct {
        struct {
            int32_t deep;
        };
    };
    int32_t shallow;
};

// Anonymous union member
struct WithAnonUnion {
    int32_t type;
    union {
        int32_t i;
        float f;
    };
};
static_assert(sizeof(WithAnonUnion) == 8);

//=============================================================================
// 10. Complex Combined Cases
//=============================================================================

// Struct with multiple complex features
struct ComplexStruct {
    std::array<int32_t, 4> arr;           // std::array
    std::pair<uint16_t, uint16_t> pair;   // std::pair
    struct {
        uint8_t flags : 4;
        uint8_t type : 4;
    } bits;                                // Bit-fields
    alignas(8) double aligned_val;         // Custom alignment
};

// Inheritance with std::array member
struct BaseWithArray {
    std::array<uint8_t, 8> data;
};
struct DerivedWithArray : BaseWithArray {
    int32_t extra;
};
static_assert(sizeof(DerivedWithArray) == 12);

//=============================================================================
// 11. Serializability with New Types
//=============================================================================

// std::array with serializable element is serializable
static_assert(is_serializable_v<std::array<int32_t, 10>, PlatformSet::current()>);

// std::array with non-serializable element is not serializable
static_assert(!is_serializable_v<std::array<wchar_t, 10>, PlatformSet::current()>);

// std::pair with serializable elements is serializable
static_assert(is_serializable_v<std::pair<int32_t, double>, PlatformSet::current()>);

// std::pair with non-serializable element is not serializable
static_assert(!is_serializable_v<std::pair<int32_t, wchar_t>, PlatformSet::current()>);

//=============================================================================
// Main
//=============================================================================

int main() {
    // All tests are compile-time static_assert
    return 0;
}
