// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
//
// Public API: get_layout_signature<T>() and cross-type comparison helpers.

#ifndef BOOST_TYPELAYOUT_SIGNATURE_HPP
#define BOOST_TYPELAYOUT_SIGNATURE_HPP

#include <boost/typelayout/detail/type_map.hpp>

namespace boost {
namespace typelayout {

[[nodiscard]] consteval auto get_arch_prefix() noexcept {
    if constexpr (sizeof(void*) == 8)
        return FixedString{TYPELAYOUT_LITTLE_ENDIAN ? "[64-le]" : "[64-be]"};
    else if constexpr (sizeof(void*) == 4)
        return FixedString{TYPELAYOUT_LITTLE_ENDIAN ? "[32-le]" : "[32-be]"};
    else
        static_assert(sizeof(void*) == 4 || sizeof(void*) == 8, "Unsupported pointer size");
}

// Layout signature -- pure byte identity (flattened, no names)

template <typename T>
[[nodiscard]] consteval auto get_layout_signature() noexcept {
    return get_arch_prefix() + TypeSignature<T>::calculate();
}

template <typename T1, typename T2>
[[nodiscard]] consteval bool layout_signatures_match() noexcept {
    return get_layout_signature<T1>() == get_layout_signature<T2>();
}

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_SIGNATURE_HPP