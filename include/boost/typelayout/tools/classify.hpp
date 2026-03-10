// classify<T> -- Compile-time SafetyLevel from layout_traits<T>.
// Requires P2996.
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

namespace detail {

// Priority: Opaque > PointerRisk > PlatformVariant > PaddingRisk > TrivialSafe
// (see safety_level.hpp for rationale).
template <typename T>
consteval SafetyLevel compute_safety_level() noexcept {
    using traits = layout_traits<T>;

    if constexpr (traits::has_opaque)
        return SafetyLevel::Opaque;
    else if constexpr (traits::has_pointer || !std::is_trivially_copyable_v<T>)
        return SafetyLevel::PointerRisk;
    else if constexpr (traits::is_platform_variant || traits::has_bit_field)
        return SafetyLevel::PlatformVariant;
    else if constexpr (traits::has_padding)
        return SafetyLevel::PaddingRisk;
    else {
        return SafetyLevel::TrivialSafe;
    }
}

} // namespace detail

template <typename T>
struct classify {
    static constexpr SafetyLevel value = detail::compute_safety_level<T>();
};

template <typename T>
inline constexpr SafetyLevel classify_v = classify<T>::value;

template <typename T>
inline constexpr bool is_trivial_safe_v =
    (classify_v<T> == SafetyLevel::TrivialSafe);

template <typename T>
inline constexpr bool is_memcpy_safe_v =
    (classify_v<T> == SafetyLevel::TrivialSafe ||
     classify_v<T> == SafetyLevel::PaddingRisk);

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_CLASSIFY_HPP