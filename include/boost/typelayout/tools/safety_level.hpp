// safety_level.hpp -- SafetyLevel enum + runtime classify_signature().
//
// Display-only classification for CompatReporter output.
// All definitions live in namespace compat to indicate they are
// reporting utilities, not core decision-making predicates.
// For programmatic decisions, use layout_traits<T>::has_padding,
// has_pointer, has_opaque directly.
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
///
/// LIMITATION: cannot detect !trivially_copyable -- this property is not
/// encoded in the signature string.  Invariant holds in practice because
/// all export entry points (SigExporter::add, TYPELAYOUT_EXPORT_TYPES)
/// enforce trivially_copyable via static_assert before producing
/// signature strings consumed by this function.
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
        sig.find("fld[") != std::string_view::npos;

    if (has_platform_variant)
        return SafetyLevel::PlatformVariant;

    if (detail::sig_has_padding(sig))
        return SafetyLevel::PaddingRisk;

    return SafetyLevel::TrivialSafe;
}

} // namespace compat
} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_SAFETY_LEVEL_HPP
