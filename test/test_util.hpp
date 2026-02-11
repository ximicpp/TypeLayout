// Shared test utilities for TypeLayout compile-time tests.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TEST_UTIL_HPP
#define BOOST_TYPELAYOUT_TEST_UTIL_HPP

#include <boost/typelayout/fwd.hpp>
#include <boost/typelayout/fixed_string.hpp>

namespace boost {
namespace typelayout {
namespace test {

// Compile-time substring search inside a FixedString.
template <size_t N>
consteval bool contains(const FixedString<N>& haystack, const char* needle) noexcept {
    size_t nlen = 0;
    while (needle[nlen] != '\0') ++nlen;
    if (nlen == 0) return true;
    size_t hlen = haystack.length();
    if (nlen > hlen) return false;
    for (size_t i = 0; i + nlen <= hlen; ++i) {
        bool match = true;
        for (size_t j = 0; j < nlen; ++j) {
            if (haystack.value[i + j] != needle[j]) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

} // namespace test
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TEST_UTIL_HPP
