// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CONFIG_HPP
#define BOOST_TYPELAYOUT_CONFIG_HPP

// =========================================================================
// Library Version
// =========================================================================

#define BOOST_TYPELAYOUT_VERSION 100      // 1.0.0 (major * 10000 + minor * 100 + patch)
#define BOOST_TYPELAYOUT_VERSION_MAJOR 1
#define BOOST_TYPELAYOUT_VERSION_MINOR 0
#define BOOST_TYPELAYOUT_VERSION_PATCH 0

// =========================================================================
// P2996 Reflection Availability
// =========================================================================
// Define BOOST_TYPELAYOUT_HAS_REFLECTION when the compiler supports P2996.
// Core headers (signature generation, layout_traits, classify, admission)
// require P2996.  Tools-layer headers marked "C++17, no P2996" do not.

#if defined(__cpp_reflection) || defined(__clang__) && __has_feature(cxx_reflection)
    #define BOOST_TYPELAYOUT_HAS_REFLECTION 1
#else
    #define BOOST_TYPELAYOUT_HAS_REFLECTION 0
#endif

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

// =========================================================================
// IEEE 754 floating-point enforcement
// =========================================================================
// TypeLayout signatures encode float as f32 and double as f64, assuming
// IEEE 754 binary representation.  This static_assert makes the assumption
// a compile-time guarantee -- compilation fails on non-conforming platforms.

#include <limits>
static_assert(std::numeric_limits<float>::is_iec559,
              "TypeLayout requires IEEE 754 (IEC 559) float");
static_assert(std::numeric_limits<double>::is_iec559,
              "TypeLayout requires IEEE 754 (IEC 559) double");

#endif // BOOST_TYPELAYOUT_CONFIG_HPP
