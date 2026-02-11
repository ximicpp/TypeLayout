// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CONFIG_HPP
#define BOOST_TYPELAYOUT_CONFIG_HPP

// =========================================================================
// Platform Configuration
// =========================================================================

// Endianness detection
#if __has_include(<bit>)
    #include <bit>
    #if defined(__cpp_lib_endian) && __cpp_lib_endian >= 201907L
        #define TYPELAYOUT_LITTLE_ENDIAN (std::endian::native == std::endian::little)
    #elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
        #define TYPELAYOUT_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    #elif defined(_WIN32)
        #define TYPELAYOUT_LITTLE_ENDIAN 1
    #else
        #define TYPELAYOUT_LITTLE_ENDIAN 1
    #endif
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #define TYPELAYOUT_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#elif defined(_WIN32)
    #define TYPELAYOUT_LITTLE_ENDIAN 1
#else
    #error "Cannot detect endianness. Define TYPELAYOUT_LITTLE_ENDIAN manually."
#endif

#endif // BOOST_TYPELAYOUT_CONFIG_HPP
