// safety_level.hpp -- SafetyLevel enum + runtime classify_signature().
// Pure C++17, no P2996 required.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_SAFETY_LEVEL_HPP
#define BOOST_TYPELAYOUT_TOOLS_SAFETY_LEVEL_HPP

#include <boost/typelayout/detail/sig_parser.hpp>

namespace boost {
namespace typelayout {

// =========================================================================
// SafetyLevel -- five-tier safety classification
//
// Ordered best (0) to worst (4).  classify<T> returns the worst applicable.
// PointerRisk > PlatformVariant because dangling pointers are a hard
// semantic error, not just a portability concern.
// =========================================================================

enum class SafetyLevel {
    TrivialSafe,     // 0 -- safe for memcpy + cross-platform transfer
    PaddingRisk,     // 1 -- has padding (info-leak risk)
    PlatformVariant, // 2 -- wchar_t / f80 / bit-fields differ across platforms
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
/// Same priority as compile-time classify<T>.
/// Note: cannot detect !trivially_copyable (not encoded in signature);
/// all export entry points enforce trivially_copyable via static_assert.
inline SafetyLevel classify_signature(std::string_view sig) noexcept {
    using detail::sig_contains_token;

    if (sig_contains_token(sig, "O("))
        return SafetyLevel::Opaque;

    bool has_pointer =
        sig_contains_token(sig, "ptr[") ||
        sig_contains_token(sig, "fnptr[") ||
        sig_contains_token(sig, "memptr[") ||
        sig_contains_token(sig, "ref[") ||
        sig_contains_token(sig, "rref[");

    if (has_pointer)
        return SafetyLevel::PointerRisk;

    bool has_platform_variant =
        sig.find("bits<") != std::string_view::npos ||
        sig.find("wchar[") != std::string_view::npos ||
        sig.find("f80[") != std::string_view::npos;

    if (has_platform_variant)
        return SafetyLevel::PlatformVariant;

    if (detail::sig_has_padding(sig))
        return SafetyLevel::PaddingRisk;

    return SafetyLevel::TrivialSafe;
}

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_SAFETY_LEVEL_HPP
