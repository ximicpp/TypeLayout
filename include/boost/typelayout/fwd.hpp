// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_FWD_HPP
#define BOOST_TYPELAYOUT_FWD_HPP

#include <boost/typelayout/config.hpp>

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <string_view>

namespace boost {
namespace typelayout {
inline namespace v1 {

    // TODO(P2593): Replace with std::always_false when available in C++26.
    template <typename...>
    struct always_false : std::false_type {};

    // Forward declaration of the primary TypeSignature template.
    // Specializations are in detail/type_map.hpp; the primary template
    // (struct/class/enum/union catch-all) is also defined there.
    template <typename T>
    struct TypeSignature;

    // Default trait for opaque element type safety.
    // Returns false unless explicitly specialized by opaque registration macros.
    // Used by is_byte_copy_safe to check whether an opaque container's
    // element types are themselves byte-copy safe.
    template <typename T>
    struct opaque_copy_safe : std::false_type {};

} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_FWD_HPP