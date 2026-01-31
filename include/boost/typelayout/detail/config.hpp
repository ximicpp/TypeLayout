// Boost.TypeLayout
//
// Platform detection and configuration
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_DETAIL_CONFIG_HPP
#define BOOST_TYPELAYOUT_DETAIL_CONFIG_HPP

// Platform detection
#if defined(_MSC_VER)
    #define TYPELAYOUT_PLATFORM_WINDOWS 1
    #define TYPELAYOUT_LITTLE_ENDIAN 1
#elif defined(__clang__) || defined(__GNUC__)
    #define TYPELAYOUT_PLATFORM_WINDOWS 0
    #define TYPELAYOUT_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#else
    #error "Unsupported compiler"
#endif

// Architecture detection
#if defined(__LP64__) || defined(_WIN64) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
    #define TYPELAYOUT_ARCH_64BIT 1
#else
    #define TYPELAYOUT_ARCH_64BIT 0
#endif

// Endianness detection fallback
#ifndef TYPELAYOUT_LITTLE_ENDIAN
    #if defined(_WIN32) || defined(_WIN64) || defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
        #define TYPELAYOUT_LITTLE_ENDIAN 1
    #else
        #define TYPELAYOUT_LITTLE_ENDIAN 0
    #endif
#endif

// Platform info is now encoded in signature header instead of hard errors
// Users can still enable strict checks if desired:
#ifdef TYPELAYOUT_STRICT_PLATFORM_CHECKS
    #if !TYPELAYOUT_ARCH_64BIT
        #error "typelayout strict mode requires 64-bit architecture"
    #endif
    #if !TYPELAYOUT_LITTLE_ENDIAN
        #error "typelayout strict mode requires little-endian architecture"
    #endif
#endif

#include <type_traits>
#include <cstdint>
#include <cstddef>
#include <limits>

namespace boost {
namespace typelayout {

    // Type size/alignment requirements (fixed-width types are portable)
    static_assert(sizeof(int8_t) == 1,   "int8_t must be 1 byte");
    static_assert(sizeof(uint8_t) == 1,  "uint8_t must be 1 byte");
    static_assert(sizeof(int16_t) == 2,  "int16_t must be 2 bytes");
    static_assert(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");
    static_assert(sizeof(int32_t) == 4,  "int32_t must be 4 bytes");
    static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
    static_assert(sizeof(int64_t) == 8,  "int64_t must be 8 bytes");
    static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");
    static_assert(sizeof(float) == 4,    "float must be 4 bytes");
    static_assert(sizeof(double) == 8,   "double must be 8 bytes");
    static_assert(sizeof(char) == 1,     "char must be 1 byte");
    static_assert(sizeof(bool) == 1,     "bool must be 1 byte");

    // IEEE 754 floating-point verification
    #ifdef __STDC_IEC_559__
        static_assert(__STDC_IEC_559__, "IEEE 754 required");
    #else
        static_assert(std::numeric_limits<float>::is_iec559 || 
                      (std::numeric_limits<float>::digits == 24 && 
                       std::numeric_limits<float>::max_exponent == 128),
                      "float must be IEEE 754");
        static_assert(std::numeric_limits<double>::is_iec559 ||
                      (std::numeric_limits<double>::digits == 53 && 
                       std::numeric_limits<double>::max_exponent == 1024),
                      "double must be IEEE 754");
    #endif

    // Platform-dependent type detection
    template <typename T>
    struct is_platform_dependent : std::false_type {};
    
    // wchar_t: 2 bytes (Windows) vs 4 bytes (Linux)
    template <> struct is_platform_dependent<wchar_t> : std::true_type {};
    // long double: 8/12/16 bytes depending on platform
    template <> struct is_platform_dependent<long double> : std::true_type {};
    
#if defined(_WIN32) || defined(_WIN64)
    // Windows LLP64: long is 4 bytes, int64_t is 8 bytes (long long)
    template <> struct is_platform_dependent<long> : std::true_type {};
    template <> struct is_platform_dependent<unsigned long> : std::true_type {};
#endif
    // Linux LP64: long = int64_t, cannot distinguish, so don't flag
    
    // CV-qualified variants
    template <typename T> struct is_platform_dependent<const T> : is_platform_dependent<T> {};
    template <typename T> struct is_platform_dependent<volatile T> : is_platform_dependent<T> {};
    template <typename T> struct is_platform_dependent<const volatile T> : is_platform_dependent<T> {};
    
    // Arrays of platform-dependent types
    template <typename T, std::size_t N> 
    struct is_platform_dependent<T[N]> : is_platform_dependent<T> {};
    
    // Helper variable template
    template <typename T>
    inline constexpr bool is_platform_dependent_v = is_platform_dependent<T>::value;

    // Helper for static_assert with template types
    template <typename T>
    struct always_false : std::false_type {};

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_DETAIL_CONFIG_HPP
