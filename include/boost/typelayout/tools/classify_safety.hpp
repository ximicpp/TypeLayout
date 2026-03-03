// Compile-time safety classifier for cross-platform type layout portability.
//
// BACKWARD COMPATIBILITY WRAPPER
//
// This file preserves the original classify_safety<T>() API that returns
// the three-tier compat::SafetyLevel (Safe/Warning/Risk).  Internally it
// now delegates to the new five-tier classify<T> in classify.hpp and maps
// the result to the legacy three-tier enum.
//
// The mapping:
//   TrivialSafe     -> Safe
//   PaddingRisk     -> Safe     (padding is not a portability risk)
//   PointerRisk     -> Warning  (pointers, non-trivially-copyable)
//   PlatformVariant -> Risk     (bit-fields, wchar_t, long double)
//   Opaque          -> Risk     (unknown = worst case)
//
// For new code, prefer using boost::typelayout::classify_v<T> from
// <boost/typelayout/tools/classify.hpp> which provides finer granularity.
//
// Requires P2996 (Bloomberg Clang) because layout_traits<T> needs reflection.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_CLASSIFY_SAFETY_HPP
#define BOOST_TYPELAYOUT_TOOLS_CLASSIFY_SAFETY_HPP

#include <boost/typelayout/tools/classify.hpp>
#include <boost/typelayout/tools/compat_check.hpp>

namespace boost {
namespace typelayout {
namespace compat {

// =========================================================================
// classify_safety<T> -- backward-compatible three-tier classifier
// =========================================================================
// Maps the new five-tier SafetyLevel to the legacy three-tier
// compat::SafetyLevel used by compat_check.hpp and existing tests.
//
// Classification mapping:
//
//   New (boost::typelayout::SafetyLevel)   ->  Legacy (compat::SafetyLevel)
//   ─────────────────────────────────────────────────────────────────────────
//   TrivialSafe                            ->  Safe
//   PaddingRisk                            ->  Safe
//   PointerRisk                            ->  Warning
//   PlatformVariant                        ->  Risk
//   Opaque                                 ->  Risk

/// Map the new five-tier SafetyLevel to the legacy three-tier enum.
consteval compat::SafetyLevel map_to_legacy(
        boost::typelayout::SafetyLevel level) noexcept {
    switch (level) {
        case boost::typelayout::SafetyLevel::TrivialSafe:
        case boost::typelayout::SafetyLevel::PaddingRisk:
            return compat::SafetyLevel::Safe;
        case boost::typelayout::SafetyLevel::PointerRisk:
            return compat::SafetyLevel::Warning;
        case boost::typelayout::SafetyLevel::PlatformVariant:
        case boost::typelayout::SafetyLevel::Opaque:
            return compat::SafetyLevel::Risk;
    }
    return compat::SafetyLevel::Risk;
}

/// Compile-time safety classification of type T (legacy three-tier).
///
/// @tparam T  The type to classify.
/// @return compat::SafetyLevel::Safe, Warning, or Risk.
///
/// Example:
///   static_assert(classify_safety<int32_t>() == SafetyLevel::Safe);
///   static_assert(classify_safety<long double>() == SafetyLevel::Risk);
template<typename T>
[[nodiscard]] consteval compat::SafetyLevel classify_safety() {
    return map_to_legacy(boost::typelayout::classify_v<T>);
}

/// Convenience: is the type's layout safe for zero-copy cross-platform transfer?
template<typename T>
[[nodiscard]] consteval bool is_layout_safe() {
    return classify_safety<T>() == compat::SafetyLevel::Safe;
}

/// Single-platform Serialization-free predicate.
///
/// A type is "locally serialization-free" when its layout signature contains
/// no risk or warning markers (i.e. classify_safety<T>() == Safe).
///
/// The full cross-platform "Serialization-free" guarantee (as reported by
/// CompatReporter::print_report()) additionally requires:
///   C1: Layout signatures MATCH across all target platforms.
///   C2: Safety classification is Safe (no pointers, bit-fields, etc.).
///
/// This predicate covers C2 only.  C1 requires comparing .sig.hpp files
/// from multiple platforms -- use TYPELAYOUT_ASSERT_COMPAT for that.
///
/// Named to align with the "Serialization-free (C1+C2)" concept in
/// compat_check.hpp, making it the natural anchor for downstream
/// libraries (e.g. XOffsetDatastructure) that build domain-specific
/// safety checks on top of TypeLayout.
///
/// Example:
///   static_assert(is_serialization_free_local<int32_t>());   // OK
///   static_assert(!is_serialization_free_local<int*>());     // has pointer
template<typename T>
[[nodiscard]] consteval bool is_serialization_free_local() {
    return classify_safety<T>() == compat::SafetyLevel::Safe;
}

} // namespace compat
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_CLASSIFY_SAFETY_HPP