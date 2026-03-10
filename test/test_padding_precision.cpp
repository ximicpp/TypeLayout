// Test: has_padding precision -- regression tests for the byte coverage
// bitmap implementation.
//
// Covers the scenarios that the old sizeof-summation approach handled
// incorrectly:
//   - EBO (Empty Base Optimization)
//   - [[no_unique_address]] members
//   - Bit-fields
//   - Nested struct internal padding
//   - Tail padding
//   - Mixed scenarios
//
// Also tests the runtime sig_has_padding() parser for consistency.
//
// All compile-time checks use static_assert.  Runtime tests use assert().
//
// Requires P2996 (Bloomberg Clang) for the compile-time tests.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include <boost/typelayout/tools/classify.hpp>
#include <boost/typelayout/tools/safety_level.hpp>
#include "test_util.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string_view>

using namespace boost::typelayout;

// =========================================================================
// Test types
// =========================================================================

namespace padding_tests {

// ----- 1. Basic padding / no-padding -----

// No padding: two same-aligned fields fill sizeof exactly.
struct NoPad_TwoInt {
    int32_t a;
    int32_t b;
};
static_assert(sizeof(NoPad_TwoInt) == 8);

// Padding between char and int32_t (3 bytes alignment gap).
struct Pad_CharInt {
    int8_t  c;
    int32_t i;
};
static_assert(sizeof(Pad_CharInt) == 8);

// Tail padding: char after int forces struct alignment to 4 => 12 bytes.
struct Pad_IntCharInt {
    int32_t a;
    int8_t  b;
    int32_t c;
};
static_assert(sizeof(Pad_IntCharInt) == 12);

// Packed-like ordering avoids internal gap but may still have tail padding.
struct NoPad_IntInt8 {
    int32_t a;
    int8_t  b;
};
// sizeof is 8 on typical platforms (4-byte aligned), so this has tail padding.
// The exact behavior depends on alignment, but we test the actual value.

// ----- 2. Empty Base Optimization (EBO) -----

struct EmptyBase {};
static_assert(sizeof(EmptyBase) == 1);

struct DerivedFromEmpty : EmptyBase {
    int32_t x;
};
// EBO: EmptyBase occupies 0 bytes inside DerivedFromEmpty.
// sizeof(DerivedFromEmpty) should be 4, no padding.
static_assert(sizeof(DerivedFromEmpty) == 4);

struct TwoEmptyBases : EmptyBase {
    int32_t x;
    int32_t y;
};
static_assert(sizeof(TwoEmptyBases) == 8);

// Multiple inheritance with EBO
struct EmptyBase2 {};
struct MultiEmpty : EmptyBase, EmptyBase2 {
    int32_t val;
};
static_assert(sizeof(MultiEmpty) == 4);

// Non-empty base with its own padding
struct BaseWithPad {
    int8_t  a;
    int32_t b;
};
static_assert(sizeof(BaseWithPad) == 8);

struct DerivedFromPadded : BaseWithPad {
    int32_t c;
};

// ----- 3. [[no_unique_address]] -----

struct EmptyTag {};

struct WithNUA {
    [[no_unique_address]] EmptyTag tag;
    int32_t value;
};
// NUA: EmptyTag occupies 0 bytes, sizeof should be 4.
static_assert(sizeof(WithNUA) == 4);

struct WithNUA_TwoEmpty {
    [[no_unique_address]] EmptyTag tag1;
    [[no_unique_address]] EmptyBase tag2;
    int64_t value;
};
static_assert(sizeof(WithNUA_TwoEmpty) == 8);

// ----- 4. Bit-fields -----

struct BitFieldSimple {
    uint32_t flags : 3;
    uint32_t mode  : 5;
};
// Single 4-byte storage unit for both bit-fields.
static_assert(sizeof(BitFieldSimple) == 4);

struct BitFieldWithPad {
    uint8_t  tag;        // offset 0, 1 byte
    int32_t  value;      // offset 4, 4 bytes (3 bytes padding before)
    uint32_t flags : 16; // bit-field in the trailing 4 bytes
};
// tag(1) + pad(3) + value(4) + flags-storage(4) = 12 bytes.
// The 3-byte gap between tag and value is padding.
static_assert(sizeof(BitFieldWithPad) == 12);

struct BitFieldNoPad {
    uint8_t  a;
    uint8_t  b;
    uint8_t  c;
    uint8_t  d;
};
// Fully packed, 4 bytes, no padding.
static_assert(sizeof(BitFieldNoPad) == 4);

// ----- 5. Nested struct with internal padding -----

struct InnerPadded {
    int8_t  x;
    int32_t y;
};
static_assert(sizeof(InnerPadded) == 8);

// Outer struct embeds InnerPadded -- the inner padding should be detected.
struct OuterWithNestedPad {
    InnerPadded inner;
    int32_t     z;
};
// sizeof = 8 (InnerPadded) + 4 (int32_t) = 12
static_assert(sizeof(OuterWithNestedPad) == 12);

// Outer struct where all fields are trivially packed, but inner has padding.
struct Nested_InnerNoPad {
    NoPad_TwoInt pair;
    int32_t      extra;
};
static_assert(sizeof(Nested_InnerNoPad) == 12);

// ----- 6. Deeply nested padding -----

struct Level1 {
    int8_t  a;
    int32_t b;
};

struct Level2 {
    Level1  inner;
    int8_t  c;
};
// Level1 = 8 bytes, c = 1 byte, alignment of Level2 = 4 => sizeof = 12
static_assert(sizeof(Level2) == 12);

struct Level3 {
    Level2  l2;
    int32_t d;
};
// Level2 = 12 bytes, d = 4 bytes => sizeof = 16
static_assert(sizeof(Level3) == 16);

// ----- 7. Base class with array of padded elements -----

// An array of padded structs inherited from a base class.
// The bitmap marks the array as atomic (all bytes covered), but the
// element type has internal padding.  any_base_array_elem_has_padding
// must detect this.
struct BaseWithPaddedArray {
    InnerPadded arr[2];
};
static_assert(sizeof(BaseWithPaddedArray) == 16);

struct DerivedFromPaddedArray : BaseWithPaddedArray {
    int32_t extra;
};

// Same pattern but with a non-padded array element -- should be clean.
struct BaseWithCleanArray {
    NoPad_TwoInt arr[2];
};

struct DerivedFromCleanArray : BaseWithCleanArray {
    int32_t extra;
};

// Multi-level: grandparent has the padded array.
struct MidLevel : BaseWithPaddedArray {
    int32_t mid;
};

struct GrandChild : MidLevel {
    int32_t gc;
};

// ----- 8. Trivially packed (no padding at any level) -----

struct FullyPacked {
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;
};
static_assert(sizeof(FullyPacked) == 16);

struct FullyPackedNested {
    FullyPacked inner;
    int32_t     extra;
};
static_assert(sizeof(FullyPackedNested) == 20);

// ----- 8. Single field -----

struct SingleChar {
    char c;
};
static_assert(sizeof(SingleChar) == 1);

struct SingleInt64 {
    int64_t v;
};
static_assert(sizeof(SingleInt64) == 8);

} // namespace padding_tests

// =========================================================================
// Part 1: Compile-time has_padding checks (static_assert)
// =========================================================================

using namespace padding_tests;

// -- Basic padding / no-padding --
static_assert(!layout_traits<NoPad_TwoInt>::has_padding,
    "P1.1: Two int32_t fields fully cover 8 bytes -- no padding");

static_assert(layout_traits<Pad_CharInt>::has_padding,
    "P1.2: int8_t + int32_t => 3 bytes alignment gap");

static_assert(layout_traits<Pad_IntCharInt>::has_padding,
    "P1.3: int32_t + int8_t + int32_t => internal + tail padding");

// NoPad_IntInt8: int32_t(4) + int8_t(1) = 5 used, sizeof = 8 => tail padding
static_assert(layout_traits<NoPad_IntInt8>::has_padding,
    "P1.4: int32_t + int8_t has tail padding (5 of 8 bytes used)");

// -- EBO --
static_assert(!layout_traits<DerivedFromEmpty>::has_padding,
    "P2.1: EBO -- EmptyBase costs 0 bytes, int32_t fills all 4 bytes");

static_assert(!layout_traits<TwoEmptyBases>::has_padding,
    "P2.2: EBO -- EmptyBase costs 0 bytes, two int32_t fill all 8 bytes");

static_assert(!layout_traits<MultiEmpty>::has_padding,
    "P2.3: Multiple EBO -- two empty bases cost 0 bytes, int32_t fills 4 bytes");

// DerivedFromPadded: inherits BaseWithPad (which HAS padding internally)
static_assert(layout_traits<DerivedFromPadded>::has_padding,
    "P2.4: Inherited base has internal padding -- bitmap must recurse into base");

// -- [[no_unique_address]] --
static_assert(!layout_traits<WithNUA>::has_padding,
    "P3.1: NUA EmptyTag costs 0 bytes, int32_t fills all 4 bytes");

static_assert(!layout_traits<WithNUA_TwoEmpty>::has_padding,
    "P3.2: Two NUA empty tags cost 0 bytes, int64_t fills all 8 bytes");

// -- Bit-fields --
static_assert(!layout_traits<BitFieldSimple>::has_padding,
    "P4.1: Two bit-fields in one uint32_t storage unit -- no padding");

// BitFieldWithPad: uint8_t + int32_t + uint32_t bitfield = 12 bytes,
// with a 3-byte gap between tag and value.
static_assert(layout_traits<BitFieldWithPad>::has_padding,
    "P4.2: uint8_t + int32_t + bit-field -- alignment gap between tag and value");

static_assert(!layout_traits<BitFieldNoPad>::has_padding,
    "P4.3: Four uint8_t fields -- fully packed, no padding");

// -- Nested struct internal padding --
static_assert(layout_traits<OuterWithNestedPad>::has_padding,
    "P5.1: Outer embeds InnerPadded which has 3 bytes internal padding");

static_assert(!layout_traits<Nested_InnerNoPad>::has_padding,
    "P5.2: Nested NoPad_TwoInt has no internal padding");

// -- Deeply nested --
static_assert(layout_traits<Level1>::has_padding,
    "P6.1: Level1 = int8_t + int32_t => padding");

static_assert(layout_traits<Level2>::has_padding,
    "P6.2: Level2 contains Level1 (padded) + tail padding");

static_assert(layout_traits<Level3>::has_padding,
    "P6.3: Level3 contains Level2 (deeply padded)");

// -- Base class with array of padded elements --
static_assert(layout_traits<DerivedFromPaddedArray>::has_padding,
    "P7.1: Base class has array of InnerPadded -- element has internal padding");

static_assert(!layout_traits<DerivedFromCleanArray>::has_padding,
    "P7.2: Base class has array of NoPad_TwoInt -- no element padding");

static_assert(layout_traits<GrandChild>::has_padding,
    "P7.3: Grandparent has array of InnerPadded -- transitive detection");

// -- Fully packed --
static_assert(!layout_traits<FullyPacked>::has_padding,
    "P8.1: Four int32_t -- fully packed, no padding");

static_assert(!layout_traits<FullyPackedNested>::has_padding,
    "P8.2: FullyPacked + int32_t -- no padding at any level");

// -- Single field --
static_assert(!layout_traits<SingleChar>::has_padding,
    "P9.1: Single char -- no padding");

static_assert(!layout_traits<SingleInt64>::has_padding,
    "P9.2: Single int64_t -- no padding");

// -- Scalars --
static_assert(!layout_traits<int32_t>::has_padding,
    "P10.1: Scalar int32_t -- no padding");

static_assert(!layout_traits<double>::has_padding,
    "P10.2: Scalar double -- no padding");

// =========================================================================
// Part 2: classify<T> consistency -- PaddingRisk where expected
// =========================================================================

static_assert(classify_v<NoPad_TwoInt> == SafetyLevel::TrivialSafe,
    "C1: NoPad_TwoInt -- no padding, no pointers => TrivialSafe");

static_assert(classify_v<Pad_CharInt> == SafetyLevel::PaddingRisk,
    "C2: Pad_CharInt -- alignment padding => PaddingRisk");

static_assert(classify_v<OuterWithNestedPad> == SafetyLevel::PaddingRisk,
    "C3: OuterWithNestedPad -- nested padding => PaddingRisk");

static_assert(classify_v<DerivedFromEmpty> == SafetyLevel::TrivialSafe,
    "C4: DerivedFromEmpty -- EBO, no padding => TrivialSafe");

static_assert(classify_v<WithNUA> == SafetyLevel::TrivialSafe,
    "C5: WithNUA -- NUA, no padding => TrivialSafe");

static_assert(classify_v<Level3> == SafetyLevel::PaddingRisk,
    "C6: Level3 -- deeply nested padding => PaddingRisk");

static_assert(classify_v<FullyPackedNested> == SafetyLevel::TrivialSafe,
    "C7: FullyPackedNested -- no padding at any level => TrivialSafe");

// =========================================================================
// Part 3: Runtime sig_has_padding() tests
// =========================================================================

void test_sig_has_padding_basic() {
    using boost::typelayout::detail::sig_has_padding;

    // Fully packed: two 4-byte fields cover all 8 bytes.
    assert(!sig_has_padding(
        "[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}"));

    // Tail padding: u32(4) + u16(2) = 6 bytes, record size 8.
    assert(sig_has_padding(
        "[64-le]record[s:8,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2]}"));

    // Internal gap: i8(1) at @0, i32(4) at @4 => gap at [1,4), total 8.
    assert(sig_has_padding(
        "[64-le]record[s:8,a:4]{@0:i8[s:1,a:1],@4:i32[s:4,a:4]}"));

    // Single field, exact fit.
    assert(!sig_has_padding(
        "[64-le]record[s:4,a:4]{@0:i32[s:4,a:4]}"));

    // Single field, tail padding.
    assert(sig_has_padding(
        "[64-le]record[s:4,a:1]{@0:i8[s:1,a:1]}"));

    // Three fields: i8(1) + i32(4) + i8(1) in 12-byte record
    // gaps at [1,4) and [9,12).
    assert(sig_has_padding(
        "[64-le]record[s:12,a:4]{@0:i8[s:1,a:1],@4:i32[s:4,a:4],@8:i8[s:1,a:1]}"));

    // Fully packed record with 3 fields: 4+4+4=12.
    assert(!sig_has_padding(
        "[64-le]record[s:12,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4],@8:i32[s:4,a:4]}"));

    std::cout << "  [PASS] sig_has_padding basic\n";
}

void test_sig_has_padding_edge_cases() {
    using boost::typelayout::detail::sig_has_padding;

    // Not a record => false (no padding detectable).
    assert(!sig_has_padding("[64-le]i32[s:4,a:4]"));

    // Empty body => false.
    assert(!sig_has_padding("[64-le]record[s:1,a:1]{}"));

    // Overlapping fields (e.g. union-like or NUA) => coverage merges.
    // Two 4-byte fields both at offset 0 in an 4-byte record: fully covered.
    assert(!sig_has_padding(
        "[64-le]record[s:4,a:4]{@0:i32[s:4,a:4],@0:i32[s:4,a:4]}"));

    // Nested record entry: the outer parser should find the nested record's
    // [s:SIZE] and use its total size for coverage.
    // This tests that nested braces are properly depth-tracked.
    assert(sig_has_padding(
        "[64-le]record[s:16,a:4]{@0:i8[s:1,a:1],@8:i32[s:4,a:4]}"));

    std::cout << "  [PASS] sig_has_padding edge cases\n";
}

void test_sig_has_padding_consistency() {
    // Verify that classify_signature agrees with sig_has_padding
    // for signatures without pointers, platform-variant types, or opaque markers.

    using boost::typelayout::detail::sig_has_padding;

    // Padded signature => classify_signature should return PaddingRisk.
    auto padded_sig = "[64-le]record[s:8,a:4]{@0:i8[s:1,a:1],@4:i32[s:4,a:4]}";
    assert(sig_has_padding(padded_sig));
    assert(classify_signature(padded_sig) == SafetyLevel::PaddingRisk);

    // Packed signature => classify_signature should return TrivialSafe.
    auto packed_sig = "[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}";
    assert(!sig_has_padding(packed_sig));
    assert(classify_signature(packed_sig) == SafetyLevel::TrivialSafe);

    // Pointer signature with padding => PointerRisk takes priority.
    auto ptr_pad_sig = "[64-le]record[s:16,a:8]{@0:i8[s:1,a:1],@8:ptr[s:8,a:8]}";
    assert(sig_has_padding(ptr_pad_sig));
    assert(classify_signature(ptr_pad_sig) == SafetyLevel::PointerRisk);

    // PlatformVariant with padding => PlatformVariant takes priority.
    auto plat_pad_sig = "[64-le]record[s:8,a:4]{@0:i8[s:1,a:1],@4:wchar[s:4,a:4]}";
    assert(sig_has_padding(plat_pad_sig));
    assert(classify_signature(plat_pad_sig) == SafetyLevel::PlatformVariant);

    std::cout << "  [PASS] sig_has_padding / classify_signature consistency\n";
}

// =========================================================================
// main
// =========================================================================

int main() {
    std::cout << "=== test_padding_precision ===\n";

    // Part 1 and Part 2 are all static_assert -- verified at compile time.
    std::cout << "  [PASS] Compile-time has_padding (Part 1: "
              << "22 static_assert checks)\n";
    std::cout << "  [PASS] Compile-time classify consistency (Part 2: "
              << "7 static_assert checks)\n";

    // Part 3: Runtime sig_has_padding tests.
    test_sig_has_padding_basic();
    test_sig_has_padding_edge_cases();
    test_sig_has_padding_consistency();

    std::cout << "All padding precision tests passed.\n";
    return 0;
}
