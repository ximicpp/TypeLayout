// Tests for SigExporter output structure and correctness.
//
// Verifies that SigExporter::write_stdout() produces output containing
// expected signature strings and platform metadata.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/tools/sig_export.hpp>
#include <sstream>
#include <cassert>
#include <iostream>
#include <cstdint>
#include <string>

using namespace boost::typelayout;

namespace test_types {
    struct Simple { int32_t x; double y; };
    struct Pair { int32_t a; int32_t b; };
}

void test_export_contains_signatures() {
    SigExporter ex("test_platform");
    ex.add<test_types::Simple>("Simple");
    ex.add<test_types::Pair>("Pair");

    // Capture stdout output
    std::ostringstream oss;
    auto* old_buf = std::cout.rdbuf(oss.rdbuf());
    ex.write_stdout();
    std::cout.rdbuf(old_buf);

    std::string output = oss.str();

    // Verify signatures are present
    constexpr auto simple_layout = get_layout_signature<test_types::Simple>();
    constexpr auto simple_def = get_definition_signature<test_types::Simple>();
    constexpr auto pair_layout = get_layout_signature<test_types::Pair>();

    assert(output.find(std::string(simple_layout.value, simple_layout.length())) != std::string::npos);
    assert(output.find(std::string(simple_def.value, simple_def.length())) != std::string::npos);
    assert(output.find(std::string(pair_layout.value, pair_layout.length())) != std::string::npos);

    // Verify type names are present
    assert(output.find("Simple_layout") != std::string::npos);
    assert(output.find("Simple_definition") != std::string::npos);
    assert(output.find("Pair_layout") != std::string::npos);

    std::cout << "  [PASS] Export contains signatures\n";
}

void test_export_contains_metadata() {
    SigExporter ex("test_platform");
    ex.add<test_types::Simple>("Simple");

    std::ostringstream oss;
    auto* old_buf = std::cout.rdbuf(oss.rdbuf());
    ex.write_stdout();
    std::cout.rdbuf(old_buf);

    std::string output = oss.str();

    // Platform metadata
    assert(output.find("test_platform") != std::string::npos);
    assert(output.find("platform_name") != std::string::npos);
    assert(output.find("arch_prefix") != std::string::npos);
    assert(output.find("pointer_size") != std::string::npos);
    assert(output.find("sizeof_long") != std::string::npos);
    assert(output.find("sizeof_wchar_t") != std::string::npos);
    assert(output.find("sizeof_long_double") != std::string::npos);

    std::cout << "  [PASS] Export contains platform metadata\n";
}

void test_export_contains_structure() {
    SigExporter ex("test_platform");
    ex.add<test_types::Simple>("Simple");

    std::ostringstream oss;
    auto* old_buf = std::cout.rdbuf(oss.rdbuf());
    ex.write_stdout();
    std::cout.rdbuf(old_buf);

    std::string output = oss.str();

    // Include guard
    assert(output.find("#ifndef") != std::string::npos);
    assert(output.find("#define") != std::string::npos);
    assert(output.find("#endif") != std::string::npos);

    // Namespace
    assert(output.find("namespace boost") != std::string::npos);
    assert(output.find("namespace typelayout") != std::string::npos);
    assert(output.find("namespace platform") != std::string::npos);
    assert(output.find("namespace test_platform") != std::string::npos);

    // Type registry
    assert(output.find("TypeEntry types[]") != std::string::npos);
    assert(output.find("type_count") != std::string::npos);

    // Platform info accessor
    assert(output.find("get_platform_info") != std::string::npos);

    std::cout << "  [PASS] Export contains structure\n";
}

void test_export_type_count() {
    SigExporter ex("test_platform");
    ex.add<test_types::Simple>("Simple");
    ex.add<test_types::Pair>("Pair");

    std::ostringstream oss;
    auto* old_buf = std::cout.rdbuf(oss.rdbuf());
    ex.write_stdout();
    std::cout.rdbuf(old_buf);

    std::string output = oss.str();

    // type_count should be 2
    assert(output.find("type_count = 2") != std::string::npos);

    std::cout << "  [PASS] Export type count correct\n";
}

void test_export_empty() {
    SigExporter ex("empty_platform");

    std::ostringstream oss;
    auto* old_buf = std::cout.rdbuf(oss.rdbuf());
    ex.write_stdout();
    std::cout.rdbuf(old_buf);

    std::string output = oss.str();

    // Should still produce valid structure
    assert(output.find("type_count = 0") != std::string::npos);
    assert(output.find("#ifndef") != std::string::npos);

    std::cout << "  [PASS] Export empty (zero types)\n";
}

int main() {
    std::cout << "=== SigExporter Tests ===\n";

    test_export_contains_signatures();
    test_export_contains_metadata();
    test_export_contains_structure();
    test_export_type_count();
    test_export_empty();

    std::cout << "All SigExporter tests passed.\n";
    return 0;
}
