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
[[nodiscard]] consteval bool type_has_pointer_layout() noexcept {
    using Bare = std::remove_cv_t<T>;
    if constexpr (requires { TypeSignature<Bare>::pointer_free; }) {
        return !TypeSignature<Bare>::pointer_free;
    } else {
        return sig_has_pointer(get_layout_signature<Bare>());
    }
}

template <typename T>
[[nodiscard]] consteval bool is_pointer_free_layout() noexcept {
    return !type_has_pointer_layout<T>();
}

template <typename T>
struct layout_traits {
    static constexpr auto signature = get_layout_signature<T>();
    static constexpr bool has_pointer = type_has_pointer_layout<T>();
};

} // namespace detail

} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_LAYOUT_TRAITS_HPP
