// Test: Cross-Platform Compatibility Check Utilities
//
// Tests sig_match, layout_match, definition_match (constexpr + runtime),
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

// definition_match is an alias for sig_match
static_assert(definition_match(
    "[64-le]record[s:8,a:4]{@0[x]:i32[s:4,a:4]}",
    "[64-le]record[s:8,a:4]{@0[x]:i32[s:4,a:4]}"),
    "definition_match should work like sig_match");

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
    inline constexpr const char PacketHeader_definition[] =
        "[64-le]record[s:16,a:4]{@0[magic]:u32[s:4,a:4],@4[version]:u16[s:2,a:2]}";

    inline constexpr const char UnsafeType_layout[] =
        "[64-le]record[s:16,a:8]{@0:i64[s:8,a:8],@8:wchar[s:4,a:4]}";
    inline constexpr const char UnsafeType_definition[] =
        "[64-le]record[s:16,a:8]{@0[a]:i64[s:8,a:8],@8[wc]:wchar[s:4,a:4]}";

    inline constexpr TypeEntry types[] = {
        {"PacketHeader", PacketHeader_layout, PacketHeader_definition},
        {"UnsafeType", UnsafeType_layout, UnsafeType_definition},
    };
    inline constexpr int type_count = 2;
}

namespace platform_b {
    // PacketHeader — same
    inline constexpr const char PacketHeader_layout[] =
        "[64-le]record[s:16,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2]}";
    inline constexpr const char PacketHeader_definition[] =
        "[64-le]record[s:16,a:4]{@0[magic]:u32[s:4,a:4],@4[version]:u16[s:2,a:2]}";

    // UnsafeType — different (wchar_t=2B on Windows)
    inline constexpr const char UnsafeType_layout[] =
        "[64-le]record[s:12,a:8]{@0:i32[s:4,a:4],@8:wchar[s:2,a:2]}";
    inline constexpr const char UnsafeType_definition[] =
        "[64-le]record[s:12,a:8]{@0[a]:i32[s:4,a:4],@8[wc]:wchar[s:2,a:2]}";

    inline constexpr TypeEntry types[] = {
        {"PacketHeader", PacketHeader_layout, PacketHeader_definition},
        {"UnsafeType", UnsafeType_layout, UnsafeType_definition},
    };
    inline constexpr int type_count = 2;
}

void test_compat_reporter() {
    CompatReporter reporter;
    reporter.add_platform("platform_a", platform_a::types, platform_a::type_count);
    reporter.add_platform("platform_b", platform_b::types, platform_b::type_count);

    // Test compare()
    auto results = reporter.compare();
    assert(results.size() == 2);

    // PacketHeader should match
    assert(results[0].name == "PacketHeader");
    assert(results[0].layout_match == true);
    assert(results[0].definition_match == true);

    // UnsafeType should differ
    assert(results[1].name == "UnsafeType");
    assert(results[1].layout_match == false);
    assert(results[1].definition_match == false);

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
    assert(report.find("Serialization-free") != std::string::npos);
    assert(report.find("Needs serialization") != std::string::npos);
    assert(report.find("50%") != std::string::npos);  // 1/2 compatible

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

void test_platform_metadata() {
    CompatReporter reporter;
    reporter.add_platform({
        "test_plat",
        platform_a::types,
        platform_a::type_count,
        8, 8, 4, 16, 16,
        "[64-le]"
    });

    std::ostringstream oss;
    reporter.print_report(oss);
    std::string report = oss.str();

    assert(report.find("pointer=8B") != std::string::npos);
    assert(report.find("long=8B") != std::string::npos);
    assert(report.find("[64-le]") != std::string::npos);

    std::cout << "  [PASS] Platform metadata in report\n";
}

int main() {
    std::cout << "=== test_compat_check ===\n";

    test_compat_reporter();
    test_single_platform();
    test_empty_reporter();
    test_platform_metadata();

    std::cout << "All compat_check tests passed.\n";
    return 0;
}
