// layout_traits<T> -- Compile-time type layout descriptor.
// Aggregates signature + has_pointer for byte-copy-safe admission.
// Requires P2996 for struct/class types.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_LAYOUT_TRAITS_HPP
#define BOOST_TYPELAYOUT_LAYOUT_TRAITS_HPP

#include <boost/typelayout/signature.hpp>
#include <boost/typelayout/detail/sig_parser.hpp>

namespace boost {
namespace typelayout {
inline namespace v1 {

namespace detail {

template <typename T>
struct layout_traits {
    static constexpr auto signature = get_layout_signature<T>();

    static constexpr bool has_pointer = []() consteval {
        if constexpr (requires { TypeSignature<std::remove_cv_t<T>>::pointer_free; }) {
            // Opaque type with explicit user assertion
            return !TypeSignature<std::remove_cv_t<T>>::pointer_free;
        } else {
            return detail::sig_has_pointer(signature);
        }
    }();
};

} // namespace detail

} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_LAYOUT_TRAITS_HPP
