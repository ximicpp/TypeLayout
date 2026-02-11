// Direct unit tests for FixedString<N> and to_fixed_string().
//
// FixedString is the foundation type for the entire TypeLayout library.
// These tests verify all public operations independently of signature generation.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include <boost/typelayout/core/fwd.hpp>
#include <iostream>
#include <string_view>
#include <cstdint>

using namespace boost::typelayout;

// =========================================================================
// 1. Constructor tests
// =========================================================================

// From string literal
static_assert(FixedString{"hello"}.length() == 5, "literal ctor: length");
static_assert(FixedString{"hello"}.value[0] == 'h', "literal ctor: first char");
static_assert(FixedString{"hello"}.value[4] == 'o', "literal ctor: last char");
static_assert(FixedString{"hello"}.value[5] == '\0', "literal ctor: null terminator");

// Default constructor
static_assert(FixedString<1>{}.length() == 0, "default ctor: length 0");
static_assert(FixedString<10>{}.length() == 0, "default ctor: length 0 (larger buffer)");
static_assert(FixedString<1>{}.value[0] == '\0', "default ctor: null at [0]");

// Single character
static_assert(FixedString{"x"}.length() == 1, "single char: length 1");

// Empty string literal
static_assert(FixedString{""}.length() == 0, "empty literal: length 0");

// =========================================================================
// 2. Concatenation tests
// =========================================================================

static_assert((FixedString{"ab"} + FixedString{"cd"}).length() == 4,
    "concat: length is sum");
static_assert((FixedString{"ab"} + FixedString{"cd"}) == "abcd",
    "concat: content correct");

// Concat with empty
static_assert((FixedString{"hello"} + FixedString{""}) == "hello",
    "concat: x + empty == x");
static_assert((FixedString{""} + FixedString{"world"}) == "world",
    "concat: empty + x == x");

// Concat both empty
static_assert((FixedString{""} + FixedString{""}).length() == 0,
    "concat: empty + empty = empty");

// Triple concat
static_assert((FixedString{"a"} + FixedString{"b"} + FixedString{"c"}) == "abc",
    "concat: triple");

// Concat preserves all characters
static_assert((FixedString{"[s:"} + FixedString{"4"} + FixedString{",a:"} + FixedString{"4"} + FixedString{"]"}) == "[s:4,a:4]",
    "concat: signature-like pattern");

// =========================================================================
// 3. Equality tests
// =========================================================================

// Same size, same content
static_assert(FixedString{"abc"} == FixedString{"abc"}, "eq: same");
static_assert(!(FixedString{"abc"} == FixedString{"abd"}), "eq: different last char");
static_assert(!(FixedString{"abc"} == FixedString{"ab"}), "eq: different length");

// Cross-size equality
static_assert([]() consteval {
    auto a = FixedString{"test"};       // FixedString<5>
    auto b = FixedString{"test"} + FixedString{""};  // larger buffer, same content
    return a == b;
}(), "eq: cross-size with same content");

// With c-string
static_assert(FixedString{"hello"} == "hello", "eq: vs c-string");
static_assert(!(FixedString{"hello"} == "world"), "eq: vs c-string (different)");
static_assert(!(FixedString{"hello"} == "hell"), "eq: vs c-string (shorter)");

// Empty equality
static_assert(FixedString{""} == "", "eq: empty vs empty c-string");
static_assert(!(FixedString{""} == "x"), "eq: empty vs non-empty");

// =========================================================================
// 4. length() tests
// =========================================================================

static_assert(FixedString{""}.length() == 0, "length: empty");
static_assert(FixedString{"a"}.length() == 1, "length: 1");
static_assert(FixedString{"abcdefghij"}.length() == 10, "length: 10");

// =========================================================================
// 5. skip_first() tests
// =========================================================================

static_assert(FixedString{",hello"}.skip_first() == "hello",
    "skip_first: removes leading comma");
static_assert(FixedString{"x"}.skip_first() == "",
    "skip_first: single char -> empty");
static_assert(FixedString{""}.skip_first().length() == 0,
    "skip_first: empty -> empty");
static_assert(FixedString{",@0:i32"}.skip_first() == "@0:i32",
    "skip_first: signature-like pattern");

// =========================================================================
// 6. to_fixed_string() tests
// =========================================================================

static_assert(to_fixed_string(0) == "0", "to_fixed_string: zero");
static_assert(to_fixed_string(1) == "1", "to_fixed_string: one");
static_assert(to_fixed_string(42) == "42", "to_fixed_string: two digits");
static_assert(to_fixed_string(100) == "100", "to_fixed_string: three digits");
static_assert(to_fixed_string(1234567890) == "1234567890", "to_fixed_string: large");

// Size values commonly used in signatures
static_assert(to_fixed_string(std::size_t(4)) == "4", "to_fixed_string: size_t 4");
static_assert(to_fixed_string(std::size_t(8)) == "8", "to_fixed_string: size_t 8");
static_assert(to_fixed_string(std::size_t(16)) == "16", "to_fixed_string: size_t 16");

// =========================================================================
// 7. operator std::string_view() tests (compile-time)
// =========================================================================

static_assert([]() consteval {
    auto fs = FixedString{"hello"};
    std::string_view sv = fs;
    return sv.size() == 5 && sv[0] == 'h' && sv[4] == 'o';
}(), "string_view conversion: content and length");

static_assert([]() consteval {
    auto fs = FixedString{""};
    std::string_view sv = fs;
    return sv.empty();
}(), "string_view conversion: empty");

// =========================================================================
// Main -- runtime confirmation
// =========================================================================

int main() {
    std::cout << "=== FixedString Unit Tests ===\n\n";

    // Runtime verification of string_view conversion
    auto fs = FixedString{"runtime test"};
    std::string_view sv = fs;
    std::cout << "  string_view: \"" << sv << "\" (len=" << sv.size() << ")\n";

    // Runtime verification of to_fixed_string
    std::cout << "  to_fixed_string(42): \"" << to_fixed_string(42) << "\"\n";
    std::cout << "  to_fixed_string(0):  \"" << to_fixed_string(0) << "\"\n";

    std::cout << "\nAll static_assert tests passed at compile time.\n";
    return 0;
}
