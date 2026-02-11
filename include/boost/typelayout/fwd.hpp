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

    enum class SignatureMode {
        Layout,
        Definition
    };

    // TODO(P2593): Replace with std::always_false when available in C++26.
    template <typename...>
    struct always_false : std::false_type {};

    // Forward declaration of the primary TypeSignature template.
    // Specializations are in detail/type_map.hpp; the primary template
    // (struct/class/enum/union catch-all) is also defined there.
    template <typename T, SignatureMode Mode = SignatureMode::Layout>
    struct TypeSignature;

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_FWD_HPP
