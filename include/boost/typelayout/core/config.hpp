// Boost.TypeLayout
//
// Core Configuration
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_CORE_CONFIG_HPP
#define BOOST_TYPELAYOUT_CORE_CONFIG_HPP

#include <cstdint>
#include <cstddef>
#include <type_traits>

// P2996 reflection support check
#if __has_include(<experimental/meta>)
    #include <experimental/meta>
    #define BOOST_TYPELAYOUT_HAS_REFLECTION 1
#else
    #define BOOST_TYPELAYOUT_HAS_REFLECTION 0
#endif

// Endianness detection
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #define TYPELAYOUT_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#elif defined(_WIN32)
    #define TYPELAYOUT_LITTLE_ENDIAN 1
#else
    #define TYPELAYOUT_LITTLE_ENDIAN 1  // Assume little-endian as default
#endif

// Version information
#define BOOST_TYPELAYOUT_VERSION_MAJOR 1
#define BOOST_TYPELAYOUT_VERSION_MINOR 0
#define BOOST_TYPELAYOUT_VERSION_PATCH 0

// Combined version: MAJOR * 100000 + MINOR * 100 + PATCH
#define BOOST_TYPELAYOUT_VERSION 100000

namespace boost {
namespace typelayout {

    // Architecture constants
    inline constexpr std::size_t pointer_size = sizeof(void*);
    inline constexpr bool is_little_endian = TYPELAYOUT_LITTLE_ENDIAN;
    inline constexpr std::size_t bit_width = pointer_size * 8;

    // Helper template for static_assert(false) in templates
    template <typename...>
    struct always_false : std::false_type {};
    
    template <typename... Ts>
    inline constexpr bool always_false_v = always_false<Ts...>::value;

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_CONFIG_HPP
