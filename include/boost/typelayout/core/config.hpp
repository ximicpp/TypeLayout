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

// =============================================================================
// C++ Version Check (C++20 minimum required)
// =============================================================================
#if defined(_MSVC_LANG)
    #define BOOST_TYPELAYOUT_CPLUSPLUS _MSVC_LANG
#else
    #define BOOST_TYPELAYOUT_CPLUSPLUS __cplusplus
#endif

#if BOOST_TYPELAYOUT_CPLUSPLUS < 202002L
    #error "Boost.TypeLayout requires C++20 or later. Use -std=c++20 or /std:c++20"
#endif

// =============================================================================
// P2996 Reflection Support Check
// =============================================================================
#if __has_include(<experimental/meta>)
    #include <experimental/meta>
    #define BOOST_TYPELAYOUT_HAS_REFLECTION 1
#else
    #define BOOST_TYPELAYOUT_HAS_REFLECTION 0
    // Note: Without P2996 reflection, struct/class layout analysis is unavailable
#endif

// =============================================================================
// Endianness Detection (C++20 std::endian preferred)
// =============================================================================
#if __has_include(<bit>)
    #include <bit>
    #if defined(__cpp_lib_endian) && __cpp_lib_endian >= 201907L
        #define TYPELAYOUT_LITTLE_ENDIAN (std::endian::native == std::endian::little)
    #else
        // <bit> available but no std::endian, fallback to macros
        #if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
            #define TYPELAYOUT_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
        #elif defined(_WIN32)
            #define TYPELAYOUT_LITTLE_ENDIAN 1
        #else
            #define TYPELAYOUT_LITTLE_ENDIAN 1
        #endif
    #endif
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #define TYPELAYOUT_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#elif defined(_WIN32)
    #define TYPELAYOUT_LITTLE_ENDIAN 1
#else
    // Cannot detect endianness reliably
    #error "Cannot detect endianness. Define TYPELAYOUT_LITTLE_ENDIAN manually (1 for little, 0 for big)"
#endif

// Version information
#define BOOST_TYPELAYOUT_VERSION_MAJOR 2
#define BOOST_TYPELAYOUT_VERSION_MINOR 0
#define BOOST_TYPELAYOUT_VERSION_PATCH 0

// Combined version: MAJOR * 100000 + MINOR * 100 + PATCH
#define BOOST_TYPELAYOUT_VERSION 200000

namespace boost {
namespace typelayout {

    // Architecture constants
    inline constexpr std::size_t pointer_size = sizeof(void*);
    inline constexpr bool is_little_endian = TYPELAYOUT_LITTLE_ENDIAN;
    inline constexpr std::size_t bit_width = pointer_size * 8;

    // =========================================================================
    // Signature Mode Configuration
    // =========================================================================

    /**
     * @brief Controls the level of detail in layout signatures.
     * 
     * Two-layer signature system:
     * 
     * - Layout:     Pure byte layout — flattens inheritance, uses "record" prefix,
     *               no names, no structural markers. Answers: "what primitive type
     *               lives at each byte offset?" Use for data exchange, shared memory,
     *               FFI, serialization.
     * 
     * - Definition: Complete type definition tree — preserves inheritance structure,
     *               includes field names and base class names, uses "record" prefix,
     *               includes "polymorphic" marker. Answers: "what is this type's
     *               full structural definition?" Use for plugin ABI verification,
     *               ODR detection, version evolution.
     * 
     * Mathematical relationship:
     *   Layout = project(Definition)   (many-to-one)
     *   definition_match(T,U) ⟹ layout_match(T,U)
     */
    enum class SignatureMode {
        Layout,      ///< Pure byte layout — flattened, no names
        Definition   ///< Full type definition tree — with names, inheritance, markers
    };

    // Default signature mode
    inline constexpr SignatureMode default_signature_mode = SignatureMode::Layout;

    // Helper template for static_assert(false) in templates
    template <typename...>
    struct always_false : std::false_type {};
    
    template <typename... Ts>
    inline constexpr bool always_false_v = always_false<Ts...>::value;

    // =========================================================================
    // Type Traits for Layout Support
    // =========================================================================

    /**
     * @brief Check if a type has determinable layout.
     * 
     * Types without determinable layout:
     * - void (no size)
     * - function types (no size)
     * - unbounded arrays T[] (unknown size)
     * 
     * This trait is SFINAE-friendly and won't trigger static_assert.
     */
    template <typename T>
    inline constexpr bool has_determinable_layout_v =
        !std::is_void_v<T> &&
        !std::is_function_v<T> &&
        !std::is_unbounded_array_v<T>;

    // =========================================================================
    // Number Buffer Size for Compile-Time Conversion
    // =========================================================================

    /// Buffer size for compile-time number-to-string conversion.
    /// 22 bytes sufficient for uint64_t max (20 digits) + sign + null.
    /// Note: from_number() returns CompileString<32> regardless of this value.
    inline constexpr std::size_t number_buffer_size = 22;

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_CONFIG_HPP
