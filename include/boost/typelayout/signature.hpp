// Boost.TypeLayout
//
// Signature Generation Public API
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_SIGNATURE_HPP
#define BOOST_TYPELAYOUT_SIGNATURE_HPP

#include <boost/typelayout/detail/config.hpp>
#include <boost/typelayout/detail/compile_string.hpp>
#include <boost/typelayout/detail/type_signature.hpp>
#include <boost/typelayout/detail/serialization_signature.hpp>
#include <boost/typelayout/detail/hash.hpp>

namespace boost {
namespace typelayout {

    // =========================================================================
    // Architecture Prefix Generation
    // =========================================================================
    
    // Generate architecture prefix string
    // Format: [BITS-ENDIAN] where BITS is 64/32, ENDIAN is le/be
    [[nodiscard]] consteval auto get_arch_prefix() noexcept {
        if constexpr (sizeof(void*) == 8) {
            if constexpr (TYPELAYOUT_LITTLE_ENDIAN) {
                return CompileString{"[64-le]"};
            } else {
                return CompileString{"[64-be]"};
            }
        } else if constexpr (sizeof(void*) == 4) {
            if constexpr (TYPELAYOUT_LITTLE_ENDIAN) {
                return CompileString{"[32-le]"};
            } else {
                return CompileString{"[32-be]"};
            }
        } else {
            // Fallback for exotic architectures (e.g., 128-bit)
            if constexpr (TYPELAYOUT_LITTLE_ENDIAN) {
                return CompileString{"["} +
                       CompileString<32>::from_number(sizeof(void*) * 8) +
                       CompileString{"-le]"};
            } else {
                return CompileString{"["} +
                       CompileString<32>::from_number(sizeof(void*) * 8) +
                       CompileString{"-be]"};
            }
        }
    }

    // =========================================================================
    // Public API
    // =========================================================================

    /// Get compile-time layout signature with architecture prefix
    /// Format: [BITS-ENDIAN]<type_signature>
    /// Example: [64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}
    template <typename T>
    [[nodiscard]] consteval auto get_layout_signature() noexcept {
        return get_arch_prefix() + TypeSignature<T>::calculate();
    }

    /// Check if two types have identical layout (includes architecture check)
    template <typename T1, typename T2>
    [[nodiscard]] consteval bool signatures_match() noexcept {
        return get_layout_signature<T1>() == get_layout_signature<T2>();
    }

    /// Get signature as C-string (for runtime use)
    template <typename T>
    [[nodiscard]] constexpr const char* get_layout_signature_cstr() noexcept {
        static constexpr auto sig = get_layout_signature<T>();
        return sig.c_str();
    }

    /// Get 64-bit layout hash (for runtime validation / protocol headers)
    template <typename T>
    [[nodiscard]] consteval uint64_t get_layout_hash() noexcept {
        constexpr auto sig = get_layout_signature<T>();
        return fnv1a_hash(sig.c_str(), sig.length());
    }

    /// Check if two types have the same layout hash
    template <typename T1, typename T2>
    [[nodiscard]] consteval bool hashes_match() noexcept {
        return get_layout_hash<T1>() == get_layout_hash<T2>();
    }

} // namespace typelayout
} // namespace boost

/// Bind type to expected signature - fails at compile time if layout differs
/// The signature includes architecture prefix (e.g., [64-le])
#define TYPELAYOUT_BIND(Type, ExpectedSig) \
    static_assert(::boost::typelayout::get_layout_signature<Type>() == ExpectedSig, \
                  "Layout mismatch for " #Type)

#endif // BOOST_TYPELAYOUT_SIGNATURE_HPP
