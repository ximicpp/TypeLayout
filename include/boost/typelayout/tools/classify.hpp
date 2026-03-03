// classify<T> -- Compile-time safety classification based on layout_traits.
//
// This is the Tool layer of TypeLayout.  It consumes the natural by-products
// of layout_traits<T> (has_pointer, has_padding, has_opaque, is_platform_variant)
// along with standard type_traits to produce a SafetyLevel judgement.
//
// classify is a convenience tool, not a core commitment.  Users may bypass it
// and read layout_traits properties directly for custom policies.
//
// SafetyLevel uses a five-tier scheme (defined in safety_level.hpp):
//   Opaque          -- contains unanalyzable fields, safety unknown
//   PointerRisk     -- contains pointers; memcpy produces dangling pointers
//   PlatformVariant -- layout differs across platforms (wchar_t, long double)
//   PaddingRisk     -- layout is fixed, but padding may leak uninitialized bytes
//   TrivialSafe     -- safe for zero-copy / memcpy / cross-boundary transfer
//
// Requires P2996 (Bloomberg Clang) because layout_traits<T> needs reflection.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_CLASSIFY_HPP
#define BOOST_TYPELAYOUT_TOOLS_CLASSIFY_HPP

#include <boost/typelayout/layout_traits.hpp>
#include <boost/typelayout/tools/safety_level.hpp>
#include <type_traits>

namespace boost {
namespace typelayout {

// SafetyLevel enum, safety_level_name(), and classify_signature() are
// provided by <boost/typelayout/tools/safety_level.hpp> and are available
// in this header through the include above.

// =========================================================================
// classify<T> -- the classifier struct
// =========================================================================

namespace detail {

/// Compute the safety level for type T by consuming layout_traits<T>.
///
/// Priority order (highest severity wins):
///   1. Opaque           -- has_opaque
///   2. PointerRisk      -- has_pointer OR !trivially_copyable
///   3. PlatformVariant  -- is_platform_variant OR has_bit_field
///   4. PaddingRisk      -- has_padding
///   5. TrivialSafe      -- none of the above
///
/// Rationale for PointerRisk > PlatformVariant:
///   Pointers make memcpy semantically incorrect (dangling pointers,
///   double-free) on ANY platform, not just cross-platform.  This is
///   a more severe and more actionable risk than size differences.
///   The runtime classifier (classify_signature) uses the same ordering,
///   ensuring consistency between compile-time and runtime results.
///
/// Note on bit-fields: Types with bit-fields (has_bit_field) have
/// compiler-dependent layout, which makes them platform-variant.
/// The signature encodes bit-fields as "bits<N,...>", and we check
/// has_bit_field explicitly to ensure PlatformVariant is returned.
template <typename T>
consteval SafetyLevel compute_safety_level() noexcept {
    using traits = layout_traits<T>;

    // 1. Opaque: unanalyzable fields
    if constexpr (traits::has_opaque) {
        return SafetyLevel::Opaque;
    }
    // 2. Pointer risk: contains pointers, or not trivially copyable
    //    (non-trivially-copyable types may have vtable pointers or
    //    user-defined copy semantics that make memcpy incorrect).
    //    Pointers also imply platform variance (32 vs 64 bit), but
    //    the dangling-pointer risk is more severe and more actionable.
    else if constexpr (traits::has_pointer || !std::is_trivially_copyable_v<T>) {
        return SafetyLevel::PointerRisk;
    }
    // 3. Platform-variant: layout differs across platforms
    //    (wchar_t size, long double / f80, bit-field layout)
    else if constexpr (traits::is_platform_variant || traits::has_bit_field) {
        return SafetyLevel::PlatformVariant;
    }
    // 4. Padding risk: layout is fixed, but has padding bytes
    else if constexpr (traits::has_padding) {
        return SafetyLevel::PaddingRisk;
    }
    // 5. All clear
    else {
        return SafetyLevel::TrivialSafe;
    }
}

} // namespace detail

/// Compile-time safety classification of type T.
///
/// Consumes layout_traits<T> natural by-products and std type_traits
/// to produce a SafetyLevel judgement.
///
/// Example:
///   static_assert(classify<int32_t>::value == SafetyLevel::TrivialSafe);
///   static_assert(classify_v<int*> == SafetyLevel::PointerRisk);
template <typename T>
struct classify {
    static constexpr SafetyLevel value = detail::compute_safety_level<T>();
};

/// Variable template shorthand for classify<T>::value.
template <typename T>
inline constexpr SafetyLevel classify_v = classify<T>::value;

// =========================================================================
// Convenience predicates
// =========================================================================

/// True if T is safe for zero-copy transfer (TrivialSafe).
template <typename T>
inline constexpr bool is_trivial_safe_v =
    (classify_v<T> == SafetyLevel::TrivialSafe);

/// True if T is safe for same-platform memcpy (TrivialSafe or PaddingRisk).
/// Padding risk only matters for information-disclosure scenarios;
/// the memcpy itself is semantically correct.
template <typename T>
inline constexpr bool is_memcpy_safe_v =
    (classify_v<T> == SafetyLevel::TrivialSafe ||
     classify_v<T> == SafetyLevel::PaddingRisk);

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_CLASSIFY_HPP