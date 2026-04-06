// sig_parser.hpp -- Signature string parsing utilities.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_DETAIL_SIG_PARSER_HPP
#define BOOST_TYPELAYOUT_DETAIL_SIG_PARSER_HPP

#include <string_view>
#include <cstddef>

namespace boost {
namespace typelayout {
inline namespace v1 {
namespace detail {

/// Token-boundary-aware find: matches only when the preceding char is not alnum.
constexpr bool sig_contains_token(std::string_view haystack,
                                  std::string_view needle) noexcept {
    std::size_t pos = 0;
    while (pos < haystack.size()) {
        std::size_t found = haystack.find(needle, pos);
        if (found == std::string_view::npos) return false;
        if (found == 0) return true;
        char prev = haystack[found - 1];
        bool prev_is_alnum = (prev >= 'a' && prev <= 'z') ||
                             (prev >= 'A' && prev <= 'Z') ||
                             (prev >= '0' && prev <= '9');
        if (!prev_is_alnum) return true;
        pos = found + 1;
    }
    return false;
}

/// Check whether a signature (string_view) contains pointer-like tokens.
/// Keep in sync with fixed_string.hpp::sig_has_pointer (consteval FixedString version).
inline bool sig_has_pointer(std::string_view sig) noexcept {
    return sig_contains_token(sig, "ptr[") ||
           sig_contains_token(sig, "fnptr[") ||
           sig_contains_token(sig, "memptr[") ||
           sig_contains_token(sig, "ref[") ||
           sig_contains_token(sig, "rref[") ||
           sig_contains_token(sig, "vptr");
}

} // namespace detail
} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_DETAIL_SIG_PARSER_HPP
