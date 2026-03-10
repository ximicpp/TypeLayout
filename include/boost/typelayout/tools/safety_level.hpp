// safety_level.hpp -- Unified safety classification enum and runtime
// signature-based classifier.
//
// This header does NOT require P2996 -- it is pure C++17.
// It provides the shared SafetyLevel enum and a runtime function
// classify_signature(string_view) that scans a layout signature string
// to produce a safety classification.
//
// Used by:
//   - classify.hpp       (compile-time classifier, wraps layout_traits)
//   - compat_check.hpp   (runtime cross-platform reporter)
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
// =========================================================================

/// Safety classification for zero-copy / memcpy / cross-boundary transfer.
///
/// Ordered from best (TrivialSafe = 0) to worst (Opaque = 4).
/// Higher integer value = higher risk.  classify<T> returns the
/// WORST (highest) applicable level.
///
/// Severity ordering (worst to best):
///   Opaque > PointerRisk > PlatformVariant > PaddingRisk > TrivialSafe
///
/// PointerRisk (3) > PlatformVariant (2) because pointer-containing types
/// cause dangling pointers / double-free on memcpy — a hard semantic error.
/// PlatformVariant types are merely layout-unstable across platforms, which
/// is a portability concern but not an immediate memory safety violation.
enum class SafetyLevel {
    /// The type is safe for memcpy, cross-process, and cross-platform transfer.
    /// No pointers, no padding, trivially copyable, platform-independent layout.
    TrivialSafe,     // = 0

    /// Layout is fixed and portable, but padding bytes exist.
    /// memcpy works correctly, but padding may leak uninitialized memory
    /// (information disclosure risk in serialization/network scenarios).
    PaddingRisk,     // = 1

    /// Layout differs across platforms due to platform-dependent types
    /// (wchar_t, long double / f80, bit-fields).
    /// Same-platform memcpy may be fine, but cross-platform transfer is unsafe.
    PlatformVariant, // = 2

    /// Layout contains pointer-like fields (ptr, fnptr, memptr, ref, rref).
    /// Byte-level copy produces dangling pointers or double-free scenarios.
    /// Also used conservatively when !is_trivially_copyable (e.g. vtable).
    PointerRisk,     // = 3

    /// Contains opaque (unanalyzable) fields.  Safety cannot be determined.
    /// The user is responsible for manual verification.
    Opaque,          // = 4
};

// =========================================================================
// safety_level_name -- human-readable label
// =========================================================================

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

// =========================================================================
// Runtime signature classification
//
// Scans a layout signature string (std::string_view) to determine its
// safety level.  This is the runtime counterpart of classify<T> which
// works at compile time using layout_traits<T>.
//
// This function does NOT require P2996 -- it only performs string
// matching on signature patterns.
// =========================================================================

/// Classify a layout signature string at runtime.
///
/// Priority order (matches the compile-time classify<T>):
///   1. Opaque          -- contains "O(" marker
///   2. PointerRisk     -- ptr[, fnptr[, memptr[, ref[, rref[
///   3. PlatformVariant -- wchar[, f80[, bits<
///   4. PaddingRisk     -- record size exceeds coverage of leaf fields
///   5. TrivialSafe     -- none of the above
///
/// Rationale for PointerRisk > PlatformVariant:
///   Pointers make memcpy semantically incorrect (dangling pointers)
///   on ANY platform, which is a more severe and actionable risk than
///   cross-platform size differences.  This ordering matches the
///   compile-time classifier in classify.hpp.
///
/// PaddingRisk is detected by parsing the signature to find gaps between
/// leaf field entries.  The record's total size (from the "record[s:N,...]"
/// header) is compared against the coverage of all "@offset:type[s:N,...]"
/// entries.  Any uncovered byte indicates padding.
inline SafetyLevel classify_signature(std::string_view sig) noexcept {
    using detail::sig_contains_token;

    // 1. Opaque: look for "O(" marker (TYPELAYOUT_REGISTER_OPAQUE).
    //    Use token-boundary matching to avoid false positives from
    //    type names that happen to contain "O(" as a substring.
    if (sig_contains_token(sig, "O("))
        return SafetyLevel::Opaque;

    // 2. Pointer risk: pointer-like fields make memcpy produce dangling refs.
    //    Pointers are also platform-variant (32 vs 64 bit), but the
    //    dangling-pointer risk is more severe and takes priority.
    bool has_pointer =
        sig_contains_token(sig, "ptr[") ||
        sig_contains_token(sig, "fnptr[") ||
        sig_contains_token(sig, "memptr[") ||
        sig_contains_token(sig, "ref[") ||
        sig_contains_token(sig, "rref[");

    if (has_pointer)
        return SafetyLevel::PointerRisk;

    // 3. Platform-variant: bit-fields and platform-dependent primitive types
    bool has_platform_variant =
        sig.find("bits<") != std::string_view::npos ||
        sig.find("wchar[") != std::string_view::npos ||
        sig.find("f80[") != std::string_view::npos;

    if (has_platform_variant)
        return SafetyLevel::PlatformVariant;

    // 4. Padding risk: record has uncovered bytes between or after fields
    if (detail::sig_has_padding(sig))
        return SafetyLevel::PaddingRisk;

    // 5. TrivialSafe
    return SafetyLevel::TrivialSafe;
}

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_SAFETY_LEVEL_HPP
