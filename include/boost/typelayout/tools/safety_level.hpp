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

#include <string_view>
#include <cstddef>

namespace boost {
namespace typelayout {

// =========================================================================
// SafetyLevel -- five-tier safety classification
// =========================================================================

/// Safety classification for zero-copy / memcpy / cross-boundary transfer.
///
/// Ordered from most restrictive (worst) to least restrictive (best).
/// classify<T> returns the WORST applicable level.
enum class SafetyLevel {
    /// The type is safe for memcpy, cross-process, and cross-platform transfer.
    /// No pointers, no padding, trivially copyable, platform-independent layout.
    TrivialSafe,

    /// Layout is fixed and portable, but padding bytes exist.
    /// memcpy works correctly, but padding may leak uninitialized memory
    /// (information disclosure risk in serialization/network scenarios).
    PaddingRisk,

    /// Layout contains pointer-like fields (ptr, fnptr, memptr, ref, rref).
    /// Byte-level copy produces dangling pointers or double-free scenarios.
    /// Also used conservatively when !is_trivially_copyable (e.g. vtable).
    PointerRisk,

    /// Layout differs across platforms due to platform-dependent types
    /// (wchar_t, long double / f80, pointer size).
    /// Same-platform memcpy may be fine, but cross-platform transfer is unsafe.
    PlatformVariant,

    /// Contains opaque (unanalyzable) fields.  Safety cannot be determined.
    /// The user is responsible for manual verification.
    Opaque,
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

namespace detail {

/// Token-boundary-aware substring search for std::string_view.
///
/// Finds `needle` in `haystack`, but only accepts a match when the character
/// immediately before it is NOT an ASCII letter.  This prevents e.g.
/// "nullptr[" from being falsely matched by "ptr[".
///
/// In TypeLayout signatures, type markers are always preceded by '{', ',',
/// ':', or appear at the start of the string -- never preceded by a letter.
inline bool sig_contains_token(std::string_view haystack,
                               std::string_view needle) noexcept {
    std::size_t pos = 0;
    while (pos < haystack.size()) {
        std::size_t found = haystack.find(needle, pos);
        if (found == std::string_view::npos) return false;
        if (found == 0) return true;
        char prev = haystack[found - 1];
        bool prev_is_alpha = (prev >= 'a' && prev <= 'z') ||
                             (prev >= 'A' && prev <= 'Z');
        if (!prev_is_alpha) return true;
        // False match inside a longer token -- advance past it
        pos = found + 1;
    }
    return false;
}

} // namespace detail

/// Classify a layout signature string at runtime.
///
/// Priority order (matches the compile-time classify<T>):
///   1. Opaque          -- contains "O(" marker
///   2. PointerRisk     -- ptr[, fnptr[, memptr[, ref[, rref[
///   3. PlatformVariant -- wchar[, f80[, bits<
///   4. PaddingRisk     -- cannot be determined from signature alone;
///                         runtime classification returns TrivialSafe
///                         if no higher-priority markers are found.
///   5. TrivialSafe     -- none of the above
///
/// Rationale for PointerRisk > PlatformVariant:
///   Pointers make memcpy semantically incorrect (dangling pointers)
///   on ANY platform, which is a more severe and actionable risk than
///   cross-platform size differences.  This ordering matches the
///   compile-time classifier in classify.hpp.
///
/// NOTE: PaddingRisk requires compile-time sizeof/reflection analysis
/// and cannot be detected from the signature string alone.  The runtime
/// classifier does not report PaddingRisk.  For precise padding
/// detection, use the compile-time classify<T>.
inline SafetyLevel classify_signature(std::string_view sig) noexcept {
    using detail::sig_contains_token;

    // 1. Opaque: look for the opaque marker "O("
    if (sig.find("O(") != std::string_view::npos)
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

    // 3. Union types may contain pointer-like variants
    if (sig_contains_token(sig, "union["))
        return SafetyLevel::PointerRisk;

    // 4. Platform-variant: bit-fields and platform-dependent primitive types
    bool has_platform_variant =
        sig.find("bits<") != std::string_view::npos ||
        sig.find("wchar[") != std::string_view::npos ||
        sig.find("f80[") != std::string_view::npos;

    if (has_platform_variant)
        return SafetyLevel::PlatformVariant;

    // 5. TrivialSafe (PaddingRisk cannot be detected at runtime)
    return SafetyLevel::TrivialSafe;
}

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_SAFETY_LEVEL_HPP
