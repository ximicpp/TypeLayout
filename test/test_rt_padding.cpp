// Runtime-only test for sig_has_padding() and classify_signature().
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/tools/safety_level.hpp>
#include <cassert>
#include <iostream>

using namespace boost::typelayout;

void test_basic() {
    using detail::sig_has_padding;
    assert(!sig_has_padding("[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}"));
    assert(sig_has_padding("[64-le]record[s:8,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2]}"));
    assert(sig_has_padding("[64-le]record[s:8,a:4]{@0:i8[s:1,a:1],@4:i32[s:4,a:4]}"));
    assert(!sig_has_padding("[64-le]record[s:4,a:4]{@0:i32[s:4,a:4]}"));
    assert(sig_has_padding("[64-le]record[s:4,a:1]{@0:i8[s:1,a:1]}"));
    assert(sig_has_padding("[64-le]record[s:12,a:4]{@0:i8[s:1,a:1],@4:i32[s:4,a:4],@8:i8[s:1,a:1]}"));
    assert(!sig_has_padding("[64-le]record[s:12,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4],@8:i32[s:4,a:4]}"));
    std::cout << "  [PASS] basic\n";
}

void test_edge() {
    using detail::sig_has_padding;
    assert(!sig_has_padding("[64-le]i32[s:4,a:4]"));
    assert(!sig_has_padding("[64-le]record[s:1,a:1]{}"));
    assert(!sig_has_padding("[64-le]record[s:4,a:4]{@0:i32[s:4,a:4],@0:i32[s:4,a:4]}"));
    assert(sig_has_padding("[64-le]record[s:16,a:4]{@0:i8[s:1,a:1],@8:i32[s:4,a:4]}"));

    // EBO embedded signature: s:0 entries should be skipped (field_size == 0).
    // record[s:4,a:4] with an empty base (s:0) at @0 and int32_t at @0 => fully covered.
    assert(!sig_has_padding(
        "[64-le]record[s:4,a:4]{@0:record[s:0,a:1]{},@0:i32[s:4,a:4]}"));
    // Same but with tail padding: record size 8, int32_t at @0, empty base at @0.
    assert(sig_has_padding(
        "[64-le]record[s:8,a:4]{@0:record[s:0,a:1]{},@0:i32[s:4,a:4]}"));

    std::cout << "  [PASS] edge cases\n";
}

void test_nested_array_element_padding() {
    using detail::sig_has_padding;

    // Outer record has no outer gap (array field covers all bytes), but
    // the element type record[s:8,a:4]{@0:i8,@4:i32} has an inner gap.
    // Fixes P1b: sig_has_padding_impl must scan all nested record blocks.
    //
    // Signature for struct Foo { PaddedStruct items[2]; }:
    //   record[s:16,a:4]{@0:array[s:16,a:4]<record[s:8,a:4]{@0:i8[s:1,a:1],@4:i32[s:4,a:4]},2>}
    assert(sig_has_padding(
        "[64-le]record[s:16,a:4]"
        "{@0:array[s:16,a:4]"
        "<record[s:8,a:4]{@0:i8[s:1,a:1],@4:i32[s:4,a:4]},2>}"));

    // Same but with a compact element type (no element padding): must be false.
    // record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]} has no gap.
    assert(!sig_has_padding(
        "[64-le]record[s:16,a:4]"
        "{@0:array[s:16,a:4]"
        "<record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]},2>}"));

    // Deeply nested: outer ok, middle ok, inner has padding.
    assert(sig_has_padding(
        "[64-le]record[s:32,a:4]"
        "{@0:array[s:32,a:4]"
        "<record[s:16,a:4]"
        "{@0:array[s:16,a:4]<record[s:8,a:4]{@0:i8[s:1,a:1],@4:i32[s:4,a:4]},2>}"
        ",2>}"));

    std::cout << "  [PASS] nested array element padding\n";
}

void test_consistency() {
    using detail::sig_has_padding;
    assert(sig_has_padding("[64-le]record[s:8,a:4]{@0:i8[s:1,a:1],@4:i32[s:4,a:4]}"));
    assert(classify_signature("[64-le]record[s:8,a:4]{@0:i8[s:1,a:1],@4:i32[s:4,a:4]}") == SafetyLevel::PaddingRisk);
    assert(!sig_has_padding("[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}"));
    assert(classify_signature("[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}") == SafetyLevel::TrivialSafe);
    assert(classify_signature("[64-le]record[s:16,a:8]{@0:i8[s:1,a:1],@8:ptr[s:8,a:8]}") == SafetyLevel::PointerRisk);
    assert(classify_signature("[64-le]record[s:8,a:4]{@0:i8[s:1,a:1],@4:wchar[s:4,a:4]}") == SafetyLevel::PlatformVariant);
    std::cout << "  [PASS] consistency\n";
}

int main() {
    std::cout << "=== test_rt_padding ===\n";
    test_basic();
    test_edge();
    test_nested_array_element_padding();
    test_consistency();
    std::cout << "All runtime padding tests passed.\n";
    return 0;
}
