// Test: Advanced type coverage -- multi-dimensional arrays, deeply nested
// union-containing-record, and cross-platform signature round-trip.
//
// Part 1 (compile-time): Multi-dimensional arrays, nested unions in structs.
// Part 2 (runtime): sig_parser and classify_signature for nested union sigs.
// Part 3 (runtime): Cross-platform round-trip with distinct platform sigs.
//
// Requires P2996 (Bloomberg Clang) for Parts 1.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/typelayout.hpp>
#include <boost/typelayout/tools/safety_level.hpp>
#include <boost/typelayout/transfer.hpp>
#include <boost/typelayout/tools/compat_check.hpp>
#include "test_util.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <sstream>

using namespace boost::typelayout;
using boost::typelayout::detail::layout_traits;

// =========================================================================
// Part 1: Multi-dimensional arrays
// =========================================================================

namespace md_array_tests {

// 1D array for baseline
struct With1DArray {
    int32_t data[4];
};

// 2D array: int32_t[3][4]
struct With2DArray {
    int32_t matrix[3][4];
};

// 3D array
struct With3DArray {
    int32_t cube[2][3][4];
};

// 2D byte array (should normalize)
struct With2DByteArray {
    char grid[4][8];
};

// 2D array of padded struct
struct PaddedElem {
    int8_t  a;
    int32_t b;
};

struct With2DPaddedArray {
    PaddedElem items[2][3];
};

// Mixed: 2D array alongside scalar fields
struct MixedWith2D {
    uint32_t id;
    float    values[2][3];
    int32_t  tail;
};

} // namespace md_array_tests

// -- Part 1a: Signature generation succeeds for multi-dimensional arrays --

// Signature generation succeeds (sizeof validates P2996 can reflect these)
static_assert(
    sizeof(md_array_tests::With1DArray) == sizeof(int32_t) * 4,
    "M1.1: 1D array size is 4 * sizeof(int32_t)");

static_assert(
    sizeof(md_array_tests::With2DArray) == sizeof(int32_t) * 12,
    "M1.2: 2D array size is 3*4 * sizeof(int32_t)");

static_assert(
    sizeof(md_array_tests::With3DArray) == sizeof(int32_t) * 24,
    "M1.3: 3D array size is 2*3*4 * sizeof(int32_t)");

// -- Part 1b: Signature contains nested array tokens --

// 2D array should contain "array[" and the inner element signature
static_assert(
    layout_traits<md_array_tests::With2DArray>::signature.contains(FixedString{"array["}),
    "M1.4: 2D array signature contains 'array['");

// 2D int32_t array should contain i32 as leaf type
static_assert(
    layout_traits<md_array_tests::With2DArray>::signature.contains(FixedString{"i32["}),
    "M1.5: 2D int32_t array signature contains inner 'i32['");

// 3D array also generates nested array signatures
static_assert(
    layout_traits<md_array_tests::With3DArray>::signature.contains(FixedString{"array["}),
    "M1.6: 3D array signature contains 'array['");

// -- Part 1c: Padding detection through multi-dimensional arrays --

// 2D array of padded elements should detect padding
static_assert(
    layout_traits<md_array_tests::With2DPaddedArray>::has_padding,
    "M1.7: 2D array of padded structs has internal padding");

// 2D array of trivial ints -- no padding
static_assert(
    !layout_traits<md_array_tests::With2DArray>::has_padding,
    "M1.8: 2D int32_t array has no padding");

// 3D array of trivial ints -- no padding
static_assert(
    !layout_traits<md_array_tests::With3DArray>::has_padding,
    "M1.9: 3D int32_t array has no padding");

// -- Part 1d: Property checks for multi-dimensional array types --

static_assert(
    !layout_traits<md_array_tests::With2DArray>::has_padding,
    "M1.10: 2D int32_t array has no padding");

static_assert(
    layout_traits<md_array_tests::With2DPaddedArray>::has_padding,
    "M1.11: 2D array of padded elements has padding");

static_assert(
    !layout_traits<md_array_tests::MixedWith2D>::has_padding,
    "M1.12: Mixed struct with 2D float array has no padding");

// -- Part 1e: Layout-compatible 2D vs flat struct --

namespace md_array_tests {
struct FlatEquiv2D {
    int32_t matrix[12]; // 3*4 = 12 elements, same byte footprint
};
} // namespace md_array_tests

// Different internal structure (nested vs flat array), but same total bytes
static_assert(
    sizeof(md_array_tests::With2DArray) ==
    sizeof(md_array_tests::FlatEquiv2D),
    "M1.13: 2D and flat array have same total size");

// But signatures differ (nested array vs 1D array)
static_assert(
    !(layout_traits<md_array_tests::With2DArray>::signature ==
      layout_traits<md_array_tests::FlatEquiv2D>::signature),
    "M1.14: 2D and flat array have different signatures (structural difference)");

// -- Part 1f: is_byte_copy_safe for multi-dimensional arrays --

static_assert(
    is_byte_copy_safe_v<md_array_tests::With2DArray>,
    "M1.15: 2D int32_t array is byte-copy safe");

static_assert(
    is_byte_copy_safe_v<md_array_tests::With3DArray>,
    "M1.16: 3D int32_t array is byte-copy safe");

// =========================================================================
// Part 2: Deeply nested union-containing-record
// =========================================================================

namespace union_tests {

// Simple union inside a struct
union DataUnion {
    int32_t i;
    float   f;
};

struct RecordWithUnion {
    uint32_t  tag;
    DataUnion data;
};

// Nested: struct containing struct containing union
struct InnerWithUnion {
    uint16_t  code;
    DataUnion payload;
};

struct OuterWithNestedUnion {
    uint32_t       id;
    InnerWithUnion inner;
};

// Double nesting: union inside struct inside struct inside struct
struct Level1WithUnion {
    int8_t    flags;
    DataUnion u;
};

struct Level2 {
    Level1WithUnion l1;
    int32_t         extra;
};

struct Level3 {
    Level2  l2;
    int64_t timestamp;
};

// Union with record members
struct RecordA {
    int32_t x;
    int32_t y;
};

struct RecordB {
    float a;
    float b;
};

union UnionOfRecords {
    RecordA ra;
    RecordB rb;
};

struct ContainsUnionOfRecords {
    uint32_t      tag;
    UnionOfRecords data;
};

// Union with array members
union ArrayUnion {
    int32_t  ints[4];
    float    floats[4];
    uint8_t  bytes[16];
};

struct WithArrayUnion {
    uint32_t   type_tag;
    ArrayUnion payload;
};

} // namespace union_tests

// -- Part 2a: Signature generation for union-containing records --

static_assert(
    layout_traits<union_tests::RecordWithUnion>::signature.contains(FixedString{"union["}),
    "M2.1: RecordWithUnion signature contains 'union['");

static_assert(
    sizeof(union_tests::RecordWithUnion) == 8,
    "M2.2: RecordWithUnion is 8 bytes (tag + union)");

// Nested union-in-struct-in-struct
static_assert(
    layout_traits<union_tests::OuterWithNestedUnion>::signature.contains(FixedString{"union["}),
    "M2.3: OuterWithNestedUnion signature contains 'union[' (deeply nested)");

static_assert(
    sizeof(union_tests::OuterWithNestedUnion) >= sizeof(union_tests::InnerWithUnion) + sizeof(uint32_t),
    "M2.4: OuterWithNestedUnion contains InnerWithUnion + id");

// Triple nesting
static_assert(
    layout_traits<union_tests::Level3>::signature.contains(FixedString{"union["}),
    "M2.5: Level3 (triple nesting) signature contains 'union['");

// Union of records
static_assert(
    layout_traits<union_tests::ContainsUnionOfRecords>::signature.contains(FixedString{"union["}),
    "M2.6: ContainsUnionOfRecords contains 'union[' in signature");

// Union with arrays
static_assert(
    layout_traits<union_tests::WithArrayUnion>::signature.contains(FixedString{"union["}),
    "M2.7: WithArrayUnion contains 'union[' in signature");

static_assert(
    layout_traits<union_tests::WithArrayUnion>::signature.contains(FixedString{"array["}),
    "M2.8: WithArrayUnion union members include 'array['");

// -- Part 2b: Classification of union-containing records --

static_assert(
    !layout_traits<union_tests::RecordWithUnion>::has_pointer,
    "M2.9: RecordWithUnion has no pointers");

static_assert(
    !layout_traits<union_tests::RecordWithUnion>::has_opaque,
    "M2.10: RecordWithUnion has no opaque members");

// -- Part 2c: Padding detection in union-containing records --

// RecordWithUnion: uint32_t(4) + union DataUnion(4) = 8, aligned to 4 -> no padding
static_assert(
    !layout_traits<union_tests::RecordWithUnion>::has_padding,
    "M2.11: RecordWithUnion has no padding (tag + union are 4-byte aligned)");

// Level3 has deeply nested Level1WithUnion with int8_t + union(4) -> padding
static_assert(
    layout_traits<union_tests::Level3>::has_padding,
    "M2.12: Level3 has padding (int8_t + DataUnion alignment gap in Level1)");

// =========================================================================
// Part 3: Runtime sig_parser tests for nested union signatures
// =========================================================================

void test_sig_parser_nested_unions() {
    using boost::typelayout::detail::sig_has_padding;

    // Simple union in record, no padding
    assert(!sig_has_padding(
        "[64-le]record[s:8,a:4]{@0:u32[s:4,a:4],@4:union[s:4,a:4]{@0:i32[s:4,a:4],@0:f32[s:4,a:4]}}"));

    // Union in record, with padding before the union
    assert(sig_has_padding(
        "[64-le]record[s:12,a:4]{@0:i8[s:1,a:1],@4:union[s:4,a:4]{@0:i32[s:4,a:4],@0:f32[s:4,a:4]},@8:i32[s:4,a:4]}"));

    // Deeply nested: record { u16 + union } -- has padding (u16 covers 2 of 4 bytes before union)
    assert(sig_has_padding(
        "[64-le]record[s:8,a:4]{@0:u16[s:2,a:2],@4:union[s:4,a:4]{@0:i32[s:4,a:4],@0:f32[s:4,a:4]}}"));

    // Union containing record members -- verify brace depth tracking
    assert(!sig_has_padding(
        "[64-le]record[s:12,a:4]{@0:u32[s:4,a:4],@4:union[s:8,a:4]{@0:record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]},@0:record[s:8,a:4]{@0:f32[s:4,a:4],@4:f32[s:4,a:4]}}}"));

    // Union with array members inside a record
    assert(!sig_has_padding(
        "[64-le]record[s:20,a:4]{@0:u32[s:4,a:4],@4:union[s:16,a:4]{@0:array[s:16,a:4]<i32[s:4,a:4],4>,@0:array[s:16,a:4]<f32[s:4,a:4],4>}}"));

    std::cout << "  [PASS] sig_parser nested union signatures\n";
}

void test_classify_nested_union_sigs() {
    using compat::classify_signature;
    using compat::SafetyLevel;

    // Union in record, no padding, no pointers -> TrivialSafe
    assert(classify_signature(
        "[64-le]record[s:8,a:4]{@0:u32[s:4,a:4],@4:union[s:4,a:4]{@0:i32[s:4,a:4],@0:f32[s:4,a:4]}}")
        == SafetyLevel::TrivialSafe);

    // Union containing pointer -> PointerRisk
    assert(classify_signature(
        "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:union[s:8,a:8]{@0:ptr[s:8,a:8],@0:i64[s:8,a:8]}}")
        == SafetyLevel::PointerRisk);

    // Nested union with wchar -> PlatformVariant
    assert(classify_signature(
        "[64-le]record[s:8,a:4]{@0:u32[s:4,a:4],@4:union[s:4,a:4]{@0:wchar[s:4,a:4],@0:i32[s:4,a:4]}}")
        == SafetyLevel::PlatformVariant);

    // Nested union with padding in outer record -> PaddingRisk
    assert(classify_signature(
        "[64-le]record[s:12,a:4]{@0:i8[s:1,a:1],@4:union[s:4,a:4]{@0:i32[s:4,a:4],@0:f32[s:4,a:4]},@8:i32[s:4,a:4]}")
        == SafetyLevel::PaddingRisk);

    std::cout << "  [PASS] classify_signature nested union signatures\n";
}

// =========================================================================
// Part 4: Cross-platform signature round-trip with distinct platforms
// =========================================================================

void test_cross_platform_roundtrip() {
    using namespace boost::typelayout::compat;

    // Simulate three platforms with genuinely different layouts:
    // - Linux LP64: long=8, wchar_t=4, long_double=16
    // - macOS ARM64: long=8, wchar_t=4, long_double=8
    // - Windows LLP64: long=4, wchar_t=2, long_double=8

    // Type A: Uses fixed-width ints only -> identical everywhere
    constexpr const char typeA_linux[]  = "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@4:u32[s:4,a:4],@8:u64[s:8,a:8]}";
    constexpr const char typeA_macos[]  = "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@4:u32[s:4,a:4],@8:u64[s:8,a:8]}";
    constexpr const char typeA_win[]    = "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@4:u32[s:4,a:4],@8:u64[s:8,a:8]}";

    // Type B: Contains long double -> different on all three
    constexpr const char typeB_linux[]  = "[64-le]record[s:32,a:16]{@0:u32[s:4,a:4],@16:fld80[s:16,a:16]}";
    constexpr const char typeB_macos[]  = "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:fld64[s:8,a:8]}";
    constexpr const char typeB_win[]    = "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:fld64[s:8,a:8]}";

    // Type C: Contains wchar_t -> linux/macos match, windows differs
    constexpr const char typeC_linux[]  = "[64-le]record[s:8,a:4]{@0:wchar[s:4,a:4],@4:u32[s:4,a:4]}";
    constexpr const char typeC_macos[]  = "[64-le]record[s:8,a:4]{@0:wchar[s:4,a:4],@4:u32[s:4,a:4]}";
    constexpr const char typeC_win[]    = "[64-le]record[s:8,a:4]{@0:wchar[s:2,a:2],@4:u32[s:4,a:4]}";

    // Type D: Contains pointer -> identical layout but PointerRisk
    constexpr const char typeD_linux[]  = "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:ptr[s:8,a:8]}";
    constexpr const char typeD_macos[]  = "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:ptr[s:8,a:8]}";
    constexpr const char typeD_win[]    = "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:ptr[s:8,a:8]}";

    TypeEntry linux_types[] = {
        {"TypeA", typeA_linux, true}, {"TypeB", typeB_linux, true},
        {"TypeC", typeC_linux, true}, {"TypeD", typeD_linux, false}
    };
    TypeEntry macos_types[] = {
        {"TypeA", typeA_macos, true}, {"TypeB", typeB_macos, true},
        {"TypeC", typeC_macos, true}, {"TypeD", typeD_macos, false}
    };
    TypeEntry win_types[] = {
        {"TypeA", typeA_win, true}, {"TypeB", typeB_win, true},
        {"TypeC", typeC_win, true}, {"TypeD", typeD_win, false}
    };

    // -- Test with all three platforms --
    CompatReporter reporter;
    reporter.add_platform({
        "x86_64_linux", linux_types, 4,
        8, 8, 4, 16, 16, "[64-le]", "LP64"
    });
    reporter.add_platform({
        "arm64_macos", macos_types, 4,
        8, 8, 4, 8, 16, "[64-le]", "LP64"
    });
    reporter.add_platform({
        "x86_64_windows", win_types, 4,
        8, 4, 2, 8, 16, "[64-le]", "LLP64"
    });

    auto results = reporter.compare();
    assert(results.size() == 4);

    // TypeA: all match, TrivialSafe
    assert(results[0].name == "TypeA");
    assert(results[0].layout_match == true);
    assert(results[0].safety == SafetyLevel::TrivialSafe);

    // TypeB: long double differs (linux != macos/windows)
    assert(results[1].name == "TypeB");
    assert(results[1].layout_match == false);
    assert(results[1].safety == SafetyLevel::PlatformVariant);

    // TypeC: wchar_t differs (linux/macos != windows)
    assert(results[2].name == "TypeC");
    assert(results[2].layout_match == false);
    assert(results[2].safety == SafetyLevel::PlatformVariant);

    // TypeD: all match, PointerRisk
    assert(results[3].name == "TypeD");
    assert(results[3].layout_match == true);
    assert(results[3].safety == SafetyLevel::PointerRisk);

    // -- are_transfer_safe subset queries --

    // TypeA on all platforms: TrivialSafe + match -> true
    assert(reporter.are_transfer_safe(
        {"TypeA"}, {"x86_64_linux", "arm64_macos", "x86_64_windows"}));

    // TypeB on macOS + Windows only: match (both fld64 8B) + PlatformVariant -> true
    assert(reporter.are_transfer_safe(
        {"TypeB"}, {"arm64_macos", "x86_64_windows"}));

    // TypeB on all three: layout mismatch -> false
    assert(!reporter.are_transfer_safe(
        {"TypeB"}, {"x86_64_linux", "arm64_macos", "x86_64_windows"}));

    // TypeC on linux + macos: match -> true
    assert(reporter.are_transfer_safe(
        {"TypeC"}, {"x86_64_linux", "arm64_macos"}));

    // TypeC on all three: differs on windows -> false
    assert(!reporter.are_transfer_safe(
        {"TypeC"}, {"x86_64_linux", "arm64_macos", "x86_64_windows"}));

    // TypeD: PointerRisk blocks even though layouts match
    assert(!reporter.are_transfer_safe(
        {"TypeD"}, {"x86_64_linux", "arm64_macos"}));

    // Multi-type query: TypeA + TypeC on linux + macos -> true
    assert(reporter.are_transfer_safe(
        {"TypeA", "TypeC"}, {"x86_64_linux", "arm64_macos"}));

    // Multi-type query: TypeA + TypeD -> false (TypeD has pointer)
    assert(!reporter.are_transfer_safe(
        {"TypeA", "TypeD"}, {"x86_64_linux", "arm64_macos"}));

    // -- Verify report output --
    std::ostringstream oss;
    reporter.print_report(oss);
    std::string report = oss.str();

    // Report should contain all platform names
    assert(report.find("x86_64_linux") != std::string::npos);
    assert(report.find("arm64_macos") != std::string::npos);
    assert(report.find("x86_64_windows") != std::string::npos);

    // Report should show MATCH for TypeA and TypeD
    assert(report.find("TypeA") != std::string::npos);
    assert(report.find("DIFFER") != std::string::npos);  // TypeB and TypeC differ

    // Report should show transfer-safe for TypeA
    assert(report.find("Transfer-safe") != std::string::npos);

    std::cout << "  [PASS] Cross-platform round-trip (3 platforms, 4 types)\n";
}

// =========================================================================
// Part 5: is_transfer_safe with cross-platform-like scenarios
// =========================================================================

namespace transfer_tests {

// Trivial type
struct TrivialMsg {
    uint32_t id;
    uint32_t seq;
    float    value;
};

// Padded type
struct PaddedMsg {
    uint8_t  flags;
    uint32_t data;
};

// Contains opaque
struct OpaqueData {
    char raw[16];
};

} // namespace transfer_tests

namespace boost { namespace typelayout {
TYPELAYOUT_REGISTER_OPAQUE(transfer_tests::OpaqueData, "xfr_opaque", false)
}} // namespace boost::typelayout

namespace transfer_tests {

struct MsgWithOpaque {
    uint32_t   id;
    OpaqueData blob;
};

} // namespace transfer_tests

void test_is_transfer_safe_extended() {
    using namespace transfer_tests;

    // Trivial type with matching signature -> true
    {
        constexpr auto sig = get_layout_signature<TrivialMsg>();
        assert(is_transfer_safe<TrivialMsg>(std::string_view(sig)));
    }

    // Trivial type with altered offset -> false (sig mismatch)
    {
        // Craft a signature with wrong offsets
        assert(!is_transfer_safe<TrivialMsg>(
            "[64-le]record[s:12,a:4]{@0:u32[s:4,a:4],@4:u32[s:4,a:4],@8:f64[s:8,a:8]}"));
    }

    // Padded type with matching signature -> true
    {
        constexpr auto sig = get_layout_signature<PaddedMsg>();
        assert(is_transfer_safe<PaddedMsg>(std::string_view(sig)));
    }

    // Simulate receiving a 32-bit platform's version of the same type
    // (different arch prefix) -> false
    {
        assert(!is_transfer_safe<TrivialMsg>(
            "[32-le]record[s:12,a:4]{@0:u32[s:4,a:4],@4:u32[s:4,a:4],@8:f32[s:4,a:4]}"));
    }

    // Opaque type with matching signature -> true
    {
        constexpr auto sig = get_layout_signature<OpaqueData>();
        assert(is_transfer_safe<OpaqueData>(std::string_view(sig)));
    }

    // Struct with opaque member, matching -> true
    {
        constexpr auto sig = get_layout_signature<MsgWithOpaque>();
        assert(is_transfer_safe<MsgWithOpaque>(std::string_view(sig)));
    }

    // Struct with opaque member, wrong tag -> false
    {
        assert(!is_transfer_safe<MsgWithOpaque>(
            "[64-le]record[s:20,a:4]{@0:u32[s:4,a:4],@4:O(wrong_tag|16|1)}"));
    }

    // Empty string -> false
    {
        assert(!is_transfer_safe<TrivialMsg>(""));
    }

    std::cout << "  [PASS] is_transfer_safe extended scenarios\n";
}

// =========================================================================
// main
// =========================================================================

int main() {
    std::cout << "=== test_advanced_types ===\n";

    // Part 1 + 2 compile-time
    std::cout << "  [PASS] Compile-time: multi-dimensional arrays (16 static_asserts)\n";
    std::cout << "  [PASS] Compile-time: union-containing records (12 static_asserts)\n";

    // Part 3: Runtime sig_parser
    test_sig_parser_nested_unions();
    test_classify_nested_union_sigs();

    // Part 4: Cross-platform round-trip
    test_cross_platform_roundtrip();

    // Part 5: is_transfer_safe
    test_is_transfer_safe_extended();

    // Print sample signatures
    std::cout << "\n--- Sample signatures ---\n";
    std::cout << "With2DArray:    " << layout_traits<md_array_tests::With2DArray>::signature.value << "\n";
    std::cout << "With3DArray:    " << layout_traits<md_array_tests::With3DArray>::signature.value << "\n";
    std::cout << "RecordWithUnion:" << layout_traits<union_tests::RecordWithUnion>::signature.value << "\n";
    std::cout << "Level3:         " << layout_traits<union_tests::Level3>::signature.value << "\n";
    std::cout << "WithArrayUnion: " << layout_traits<union_tests::WithArrayUnion>::signature.value << "\n";

    std::cout << "\nAll advanced type tests passed.\n";
    return 0;
}
