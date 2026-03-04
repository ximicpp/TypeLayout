// Runtime-only test for sig_has_padding() and classify_signature().
// Does NOT require P2996 -- C++17 is sufficient.
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
    std::cout << "  [PASS] edge cases\n";
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
    test_consistency();
    std::cout << "All runtime padding tests passed.\n";
    return 0;
}
