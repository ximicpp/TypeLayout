// Test: Cross-Platform Compatibility Check Utilities
//
// Tests sig_match, layout_match (constexpr + runtime),
// and CompatReporter output.
//
// This test does NOT require P2996 — C++17 is sufficient.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/tools/compat_check.hpp>
#include <cassert>
#include <sstream>
#include <iostream>
#include <cstring>

using namespace boost::typelayout;
using namespace boost::typelayout::compat;

// =========================================================================
// 1. Compile-time tests (static_assert)
// =========================================================================

// Identical signatures match
static_assert(sig_match(
    "[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}",
    "[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}"),
    "identical signatures should match");

// Different signatures don't match
static_assert(!sig_match(
    "[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}",
    "[64-le]record[s:16,a:8]{@0:i64[s:8,a:8],@8:i64[s:8,a:8]}"),
    "different signatures should not match");

// layout_match is an alias for sig_match
static_assert(layout_match(
    "[64-le]record[s:16,a:4]{@0:u32[s:4,a:4]}",
    "[64-le]record[s:16,a:4]{@0:u32[s:4,a:4]}"),
    "layout_match should work like sig_match");

// Empty strings match
static_assert(sig_match("", ""), "empty strings should match");

// One empty, one not — should not match
static_assert(!sig_match("", "something"), "empty vs non-empty should not match");

// Substring should not match full string
static_assert(!sig_match("[64-le]", "[64-le]record"), "prefix should not match full");

// =========================================================================
// 2. Runtime tests
// =========================================================================

// Sample signature data for two platforms
namespace platform_a {
    inline constexpr const char PacketHeader_layout[] =
        "[64-le]record[s:16,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2]}";

    inline constexpr const char UnsafeType_layout[] =
        "[64-le]record[s:16,a:8]{@0:i64[s:8,a:8],@8:wchar[s:4,a:4]}";

    inline constexpr TypeEntry types[] = {
        {"PacketHeader", PacketHeader_layout},
        {"UnsafeType", UnsafeType_layout},
    };
    inline constexpr std::size_t type_count = 2;
}

namespace platform_b {
    // PacketHeader -- same
    inline constexpr const char PacketHeader_layout[] =
        "[64-le]record[s:16,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2]}";

    // UnsafeType -- different (wchar_t=2B on Windows)
    inline constexpr const char UnsafeType_layout[] =
        "[64-le]record[s:12,a:8]{@0:i32[s:4,a:4],@8:wchar[s:2,a:2]}";

    inline constexpr TypeEntry types[] = {
        {"PacketHeader", PacketHeader_layout},
        {"UnsafeType", UnsafeType_layout},
    };
    inline constexpr std::size_t type_count = 2;
}

void test_compat_reporter() {
    CompatReporter reporter;
    reporter.add_platform("platform_a", platform_a::types, platform_a::type_count);
    reporter.add_platform("platform_b", platform_b::types, platform_b::type_count);

    // Test compare()
    auto results = reporter.compare();
    assert(results.size() == 2);

    // PacketHeader should match; signature has tail padding (6 of 16 bytes covered)
    assert(results[0].name == "PacketHeader");
    assert(results[0].layout_match == true);
    assert(results[0].safety == SafetyLevel::PaddingRisk);

    // UnsafeType should differ and be Risk (contains wchar)
    assert(results[1].name == "UnsafeType");
    assert(results[1].layout_match == false);
    assert(results[1].safety == SafetyLevel::PlatformVariant);

    // Test print_report()
    std::ostringstream oss;
    reporter.print_report(oss);
    std::string report = oss.str();

    // Report should contain key elements
    assert(report.find("Compatibility Report") != std::string::npos);
    assert(report.find("platform_a") != std::string::npos);
    assert(report.find("platform_b") != std::string::npos);
    assert(report.find("PacketHeader") != std::string::npos);
    assert(report.find("UnsafeType") != std::string::npos);
    assert(report.find("MATCH") != std::string::npos);
    assert(report.find("DIFFER") != std::string::npos);
    assert(report.find("padding may leak") != std::string::npos);  // PacketHeader
    assert(report.find("Needs serialization") != std::string::npos);
    assert(report.find("0%") != std::string::npos);  // 0/2 serialization-free

    std::cout << "  [PASS] CompatReporter compare + report\n";
}

void test_single_platform() {
    // Single platform: all types should match (compared to themselves)
    CompatReporter reporter;
    reporter.add_platform("only_plat", platform_a::types, platform_a::type_count);

    auto results = reporter.compare();
    assert(results.size() == 2);
    assert(results[0].layout_match == true);
    assert(results[1].layout_match == true);

    std::cout << "  [PASS] Single platform (self-match)\n";
}

void test_empty_reporter() {
    CompatReporter reporter;
    auto results = reporter.compare();
    assert(results.empty());

    std::cout << "  [PASS] Empty reporter\n";
}

void test_safety_classification() {
    // PaddingRisk: u32 + u16 covers 6 bytes but record size is 8 (2 bytes tail padding)
    assert(classify_signature("[64-le]record[s:8,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2]}")
           == SafetyLevel::PaddingRisk);

    // TrivialSafe: only fixed-width integers (fully packed, no padding)
    assert(classify_signature("[64-le]record[s:8,a:4]{@0:u32[s:4,a:4],@4:u32[s:4,a:4]}")
           == SafetyLevel::TrivialSafe);

    // TrivialSafe: floats
    assert(classify_signature("[64-le]record[s:8,a:4]{@0:f32[s:4,a:4],@4:f32[s:4,a:4]}")
           == SafetyLevel::TrivialSafe);

    // TrivialSafe: enum
    assert(classify_signature("[64-le]record[s:4,a:4]{@0:enum[s:4,a:4]<i32[s:4,a:4]>}")
           == SafetyLevel::TrivialSafe);

    // TrivialSafe: bytes array
    assert(classify_signature("[64-le]record[s:16,a:1]{@0:bytes[s:16,a:1]}")
           == SafetyLevel::TrivialSafe);

    // PointerRisk: contains pointer
    assert(classify_signature("[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:ptr[s:8,a:8]}")
           == SafetyLevel::PointerRisk);

    // PointerRisk: contains function pointer
    assert(classify_signature("[64-le]record[s:8,a:8]{@0:fnptr[s:8,a:8]}")
           == SafetyLevel::PointerRisk);

    // PointerRisk: contains pointer field
    assert(classify_signature("[64-le]record[s:16,a:8]{@0:ptr[s:8,a:8],@8:i32[s:4,a:4]}")
           == SafetyLevel::PointerRisk);

    // PlatformVariant: contains wchar_t
    assert(classify_signature("[64-le]record[s:4,a:4]{@0:wchar[s:4,a:4]}")
           == SafetyLevel::PlatformVariant);

    // PlatformVariant: contains bit-field
    assert(classify_signature("[64-le]record[s:4,a:4]{@0.0:bits<3,u32[s:4,a:4]>}")
           == SafetyLevel::PlatformVariant);

    // PlatformVariant: contains long double (f80) -- platform-dependent size
    assert(classify_signature("[64-le]record[s:16,a:16]{@0:f80[s:16,a:16]}")
           == SafetyLevel::PlatformVariant);

    // PlatformVariant: struct containing long double alongside safe fields
    assert(classify_signature("[64-le]record[s:32,a:16]{@0:i32[s:4,a:4],@16:f80[s:16,a:16]}")
           == SafetyLevel::PlatformVariant);

    // PointerRisk takes priority over PlatformVariant
    // (dangling-pointer risk is more severe than cross-platform size differences)
    assert(classify_signature("[64-le]record[s:16,a:8]{@0:ptr[s:8,a:8],@8:wchar[s:4,a:4]}")
           == SafetyLevel::PointerRisk);

    // PaddingRisk: char + int with alignment gap
    assert(classify_signature("[64-le]record[s:8,a:4]{@0:i8[s:1,a:1],@4:i32[s:4,a:4]}")
           == SafetyLevel::PaddingRisk);

    // TrivialSafe: fully packed record
    assert(classify_signature("[64-le]record[s:5,a:1]{@0:i32[s:4,a:4],@4:i8[s:1,a:1]}")
           == SafetyLevel::TrivialSafe);

    // PaddingRisk: tail padding (struct alignment forces extra bytes)
    assert(classify_signature("[64-le]record[s:12,a:4]{@0:i8[s:1,a:1],@4:i32[s:4,a:4],@8:i8[s:1,a:1]}")
           == SafetyLevel::PaddingRisk);

    // Helper functions
    assert(std::string(safety_label(SafetyLevel::TrivialSafe)) == "Safe");
    assert(std::string(safety_label(SafetyLevel::PointerRisk)) == "Warn");
    assert(std::string(safety_label(SafetyLevel::PlatformVariant)) == "Risk");
    assert(std::string(safety_stars(SafetyLevel::TrivialSafe)) == "***");
    assert(std::string(safety_stars(SafetyLevel::PointerRisk)) == "*!-");
    assert(std::string(safety_stars(SafetyLevel::PlatformVariant)) == "*--");

    std::cout << "  [PASS] Safety classification\n";
}

void test_safety_in_report() {
    CompatReporter reporter;
    reporter.add_platform("platform_a", platform_a::types, platform_a::type_count);
    reporter.add_platform("platform_b", platform_b::types, platform_b::type_count);

    std::ostringstream oss;
    reporter.print_report(oss);
    std::string report = oss.str();

    // Report should contain safety info
    assert(report.find("Safety") != std::string::npos);
    assert(report.find("**-") != std::string::npos);  // PacketHeader = PaddingRisk
    assert(report.find("Assumptions") != std::string::npos);
    assert(report.find("IEEE 754") != std::string::npos);

    std::cout << "  [PASS] Safety in report output\n";
}

void test_platform_metadata() {
    CompatReporter reporter;
    reporter.add_platform({
        "test_plat",
        platform_a::types,
        platform_a::type_count,
        8, 8, 4, 16, 16,
        "[64-le]",
        "LP64"
    });

    std::ostringstream oss;
    reporter.print_report(oss);
    std::string report = oss.str();

    assert(report.find("pointer=8B") != std::string::npos);
    assert(report.find("long=8B") != std::string::npos);
    assert(report.find("[64-le]") != std::string::npos);
    assert(report.find("LP64") != std::string::npos);

    std::cout << "  [PASS] Platform metadata in report\n";
}

void test_abi_equivalence() {
    // Two platforms with identical ABI fingerprints should be grouped.
    CompatReporter reporter;
    reporter.add_platform({
        "x86_64_linux_clang",
        platform_a::types, platform_a::type_count,
        8, 8, 4, 16, 16, "[64-le]", "LP64"
    });
    reporter.add_platform({
        "x86_64_linux_gcc",
        platform_a::types, platform_a::type_count,
        8, 8, 4, 16, 16, "[64-le]", "LP64"
    });
    reporter.add_platform({
        "x86_64_windows_msvc",
        platform_a::types, platform_a::type_count,
        8, 4, 2, 8, 16, "[64-le]", "LLP64"
    });

    std::ostringstream oss;
    reporter.print_report(oss);
    std::string report = oss.str();

    // Linux clang and gcc have identical ABI → grouped.
    assert(report.find("ABI equivalence") != std::string::npos);
    assert(report.find("x86_64_linux_clang") != std::string::npos);
    assert(report.find("x86_64_linux_gcc") != std::string::npos);

    // Windows has different ABI → not in the group.
    // The group line should contain both linux platforms but not windows.
    auto group_pos = report.find("ABI equivalence");
    auto group_section = report.substr(group_pos, 200);
    auto brace_pos = group_section.find("{");
    assert(brace_pos != std::string::npos);
    auto brace_end = group_section.find("}", brace_pos);
    auto group_line = group_section.substr(brace_pos, brace_end - brace_pos);
    assert(group_line.find("linux_clang") != std::string::npos);
    assert(group_line.find("linux_gcc") != std::string::npos);
    assert(group_line.find("windows") == std::string::npos);

    std::cout << "  [PASS] ABI equivalence groups\n";
}

void test_no_abi_equivalence_when_all_different() {
    // All platforms have different ABI → no equivalence section.
    CompatReporter reporter;
    reporter.add_platform({
        "linux_lp64",
        platform_a::types, platform_a::type_count,
        8, 8, 4, 16, 16, "[64-le]", "LP64"
    });
    reporter.add_platform({
        "windows_llp64",
        platform_a::types, platform_a::type_count,
        8, 4, 2, 8, 16, "[64-le]", "LLP64"
    });

    std::ostringstream oss;
    reporter.print_report(oss);
    std::string report = oss.str();

    assert(report.find("ABI equivalence") == std::string::npos);

    std::cout << "  [PASS] No ABI equivalence when all different\n";
}

// =========================================================================
// 3. are_serialization_free() — subset query API tests
// =========================================================================

// Three platforms with a mix of types.

namespace plat_linux {
    inline constexpr const char SafeType_layout[] =
        "[64-le]record[s:8,a:4]{@0:u32[s:4,a:4],@4:u32[s:4,a:4]}";
    inline constexpr const char PadType_layout[] =
        "[64-le]record[s:8,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2]}";
    inline constexpr const char PtrType_layout[] =
        "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:ptr[s:8,a:8]}";
    inline constexpr const char OpaqueType_layout[] =
        "[64-le]record[s:8,a:4]{@0:O(MyOpaque|4|4),@4:u32[s:4,a:4]}";
    inline constexpr const char WcharType_layout[] =
        "[64-le]record[s:4,a:4]{@0:wchar[s:4,a:4]}";

    inline constexpr TypeEntry types[] = {
        {"SafeType",   SafeType_layout},
        {"PadType",    PadType_layout},
        {"PtrType",    PtrType_layout},
        {"OpaqueType", OpaqueType_layout},
        {"WcharType",  WcharType_layout},
    };
    inline constexpr std::size_t type_count = 5;
}

namespace plat_macos {
    // SafeType: identical to linux
    inline constexpr const char SafeType_layout[] =
        "[64-le]record[s:8,a:4]{@0:u32[s:4,a:4],@4:u32[s:4,a:4]}";
    // PadType: identical (padding present but layout matches)
    inline constexpr const char PadType_layout[] =
        "[64-le]record[s:8,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2]}";
    // PtrType: identical (but contains pointer)
    inline constexpr const char PtrType_layout[] =
        "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:ptr[s:8,a:8]}";
    // OpaqueType: identical (but contains opaque)
    inline constexpr const char OpaqueType_layout[] =
        "[64-le]record[s:8,a:4]{@0:O(MyOpaque|4|4),@4:u32[s:4,a:4]}";
    // WcharType: same wchar_t=4 as linux
    inline constexpr const char WcharType_layout[] =
        "[64-le]record[s:4,a:4]{@0:wchar[s:4,a:4]}";

    inline constexpr TypeEntry types[] = {
        {"SafeType",   SafeType_layout},
        {"PadType",    PadType_layout},
        {"PtrType",    PtrType_layout},
        {"OpaqueType", OpaqueType_layout},
        {"WcharType",  WcharType_layout},
    };
    inline constexpr std::size_t type_count = 5;
}

namespace plat_windows {
    // SafeType: identical
    inline constexpr const char SafeType_layout[] =
        "[64-le]record[s:8,a:4]{@0:u32[s:4,a:4],@4:u32[s:4,a:4]}";
    // PadType: DIFFERENT layout on windows (hypothetical)
    inline constexpr const char PadType_layout[] =
        "[64-le]record[s:12,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2],@8:u32[s:4,a:4]}";
    // PtrType: identical
    inline constexpr const char PtrType_layout[] =
        "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:ptr[s:8,a:8]}";
    // OpaqueType: identical
    inline constexpr const char OpaqueType_layout[] =
        "[64-le]record[s:8,a:4]{@0:O(MyOpaque|4|4),@4:u32[s:4,a:4]}";
    // WcharType: wchar_t=2 on windows → different signature
    inline constexpr const char WcharType_layout[] =
        "[64-le]record[s:2,a:2]{@0:wchar[s:2,a:2]}";

    inline constexpr TypeEntry types[] = {
        {"SafeType",   SafeType_layout},
        {"PadType",    PadType_layout},
        {"PtrType",    PtrType_layout},
        {"OpaqueType", OpaqueType_layout},
        {"WcharType",  WcharType_layout},
    };
    inline constexpr std::size_t type_count = 5;
}

void test_are_serialization_free_basic() {
    CompatReporter reporter;
    reporter.add_platform("linux",   plat_linux::types,   plat_linux::type_count);
    reporter.add_platform("macos",   plat_macos::types,   plat_macos::type_count);
    reporter.add_platform("windows", plat_windows::types,  plat_windows::type_count);

    // SafeType is TrivialSafe and identical on all 3 platforms.
    assert(reporter.are_serialization_free(
        {"SafeType"}, {"linux", "macos", "windows"}));

    // PadType matches on linux+macos but DIFFERS on windows.
    assert(reporter.are_serialization_free(
        {"PadType"}, {"linux", "macos"}));
    assert(!reporter.are_serialization_free(
        {"PadType"}, {"linux", "windows"}));

    // Multiple types, subset of platforms.
    assert(reporter.are_serialization_free(
        {"SafeType", "PadType"}, {"linux", "macos"}));
    assert(!reporter.are_serialization_free(
        {"SafeType", "PadType"}, {"linux", "macos", "windows"}));

    std::cout << "  [PASS] are_serialization_free() basic queries\n";
}

void test_are_serialization_free_safety() {
    CompatReporter reporter;
    reporter.add_platform("linux", plat_linux::types, plat_linux::type_count);
    reporter.add_platform("macos", plat_macos::types, plat_macos::type_count);

    // PtrType: layout matches but PointerRisk → NOT serialization-free.
    assert(!reporter.are_serialization_free(
        {"PtrType"}, {"linux", "macos"}));

    // OpaqueType: layout matches and user guarantees opaque → IS serialization-free.
    assert(reporter.are_serialization_free(
        {"OpaqueType"}, {"linux", "macos"}));

    // PadType: PaddingRisk but layout matches → IS serialization-free.
    assert(reporter.are_serialization_free(
        {"PadType"}, {"linux", "macos"}));

    // WcharType: PlatformVariant but layout matches on linux+macos → IS serialization-free.
    assert(reporter.are_serialization_free(
        {"WcharType"}, {"linux", "macos"}));

    // WcharType: differs on windows.
    CompatReporter reporter2;
    reporter2.add_platform("linux",   plat_linux::types,   plat_linux::type_count);
    reporter2.add_platform("windows", plat_windows::types,  plat_windows::type_count);
    assert(!reporter2.are_serialization_free(
        {"WcharType"}, {"linux", "windows"}));

    std::cout << "  [PASS] are_serialization_free() safety levels\n";
}

void test_are_serialization_free_edge_cases() {
    CompatReporter reporter;
    reporter.add_platform("linux", plat_linux::types, plat_linux::type_count);
    reporter.add_platform("macos", plat_macos::types, plat_macos::type_count);

    // Empty type set → false.
    assert(!reporter.are_serialization_free({}, {"linux"}));

    // Empty platform set → false.
    assert(!reporter.are_serialization_free({"SafeType"}, {}));

    // Non-existent platform → false.
    assert(!reporter.are_serialization_free(
        {"SafeType"}, {"linux", "solaris"}));

    // Non-existent type → false.
    assert(!reporter.are_serialization_free(
        {"NoSuchType"}, {"linux", "macos"}));

    // Mix of valid and invalid type → false.
    assert(!reporter.are_serialization_free(
        {"SafeType", "NoSuchType"}, {"linux"}));

    // Single platform — self-match always works for safe types.
    assert(reporter.are_serialization_free(
        {"SafeType", "PadType", "WcharType"}, {"linux"}));

    // Vector overload (programmatic use).
    std::vector<std::string> vtypes = {"SafeType", "PadType"};
    std::vector<std::string> vplats = {"linux", "macos"};
    assert(reporter.are_serialization_free(vtypes, vplats));

    vtypes.push_back("PtrType");
    assert(!reporter.are_serialization_free(vtypes, vplats));

    std::cout << "  [PASS] are_serialization_free() edge cases\n";
}

int main() {
    std::cout << "=== test_compat_check ===\n";

    test_compat_reporter();
    test_single_platform();
    test_empty_reporter();
    test_safety_classification();
    test_safety_in_report();
    test_platform_metadata();
    test_abi_equivalence();
    test_no_abi_equivalence_when_all_different();
    test_are_serialization_free_basic();
    test_are_serialization_free_safety();
    test_are_serialization_free_edge_cases();

    std::cout << "All compat_check tests passed.\n";
    return 0;
}
