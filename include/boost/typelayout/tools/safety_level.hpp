// safety_level.hpp -- SafetyLevel enum + runtime classify_signature().
//
// Internal display-only classification for CompatReporter output.
// NOT part of the public API. For programmatic decisions, use
// is_byte_copy_safe_v<T> and layout signature comparison.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_SAFETY_LEVEL_HPP
#define BOOST_TYPELAYOUT_TOOLS_SAFETY_LEVEL_HPP

#include <boost/typelayout/detail/sig_parser.hpp>

namespace boost {
namespace typelayout {
inline namespace v1 {
namespace compat {
namespace detail {

// =========================================================================
// SafetyLevel -- five-tier safety classification for display purposes.
//
// Ordered best (0) to worst (4).  classify_signature() returns the worst
// applicable level found in a signature string.
// =========================================================================

enum class SafetyLevel {
    TrivialSafe,     // 0 -- safe for memcpy + cross-platform transfer
    PaddingRisk,     // 1 -- has padding (info-leak risk)
    PlatformVariant, // 2 -- wchar_t / fld (long double) / bit-fields differ across platforms
    PointerRisk,     // 3 -- contains pointers; memcpy produces dangling refs
    Opaque,          // 4 -- unanalyzable; user must verify manually
};

constexpr const char* safety_level_name(SafetyLevel level) noexcept {
    switch (level) {
        case SafetyLevel::TrivialSafe:     return "TrivialSafe";
        case SafetyLevel::PaddingRisk:     return "PaddingRisk";
        case SafetyLevel::PointerRisk:     return "PointerRisk";
        case SafetyLevel::PlatformVariant: return "PlatformVariant";
        case SafetyLevel::Opaque:          return "Opaque";
    }
    return "?";
}

/// Runtime classify: scans signature string for safety level.
inline SafetyLevel classify_signature(std::string_view sig) noexcept {
    using ::boost::typelayout::v1::detail::sig_contains_token;

    if (sig_contains_token(sig, "O("))
        return SafetyLevel::Opaque;

    bool has_pointer = ::boost::typelayout::v1::detail::sig_has_pointer(sig);

    if (has_pointer)
        return SafetyLevel::PointerRisk;

    bool has_platform_variant =
        sig.find("bits<") != std::string_view::npos ||
        sig.find("wchar[") != std::string_view::npos ||
        sig_contains_token(sig, "fld64[") ||
        sig_contains_token(sig, "fld80[") ||
        sig_contains_token(sig, "fld106[") ||
        sig_contains_token(sig, "fld128[");

    if (has_platform_variant)
        return SafetyLevel::PlatformVariant;

    if (::boost::typelayout::v1::detail::sig_has_padding(sig))
        return SafetyLevel::PaddingRisk;

    return SafetyLevel::TrivialSafe;
}

} // namespace detail
} // namespace compat
} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_SAFETY_LEVEL_HPP
