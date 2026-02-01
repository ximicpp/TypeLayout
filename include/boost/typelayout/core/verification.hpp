// Boost.TypeLayout
//
// Core Layout Verification (Dual-Hash)
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_CORE_VERIFICATION_HPP
#define BOOST_TYPELAYOUT_CORE_VERIFICATION_HPP

#include <boost/typelayout/core/signature.hpp>

namespace boost {
namespace typelayout {

    // =========================================================================
    // Dual-Hash Verification Structure
    // =========================================================================

    /// Dual-hash verification: FNV-1a + DJB2 + length (~2^128 collision resistance)
    struct LayoutVerification {
        uint64_t fnv1a;     ///< FNV-1a 64-bit hash
        uint64_t djb2;      ///< DJB2 64-bit hash (independent algorithm)
        uint32_t length;    ///< Signature length
        
        constexpr bool operator==(const LayoutVerification&) const noexcept = default;
    };

    /// Get dual-hash verification for a type
    template <typename T>
    [[nodiscard]] consteval LayoutVerification get_layout_verification() noexcept {
        constexpr auto sig = get_layout_signature<T>();
        return { 
            fnv1a_hash(sig.c_str(), sig.length()), 
            djb2_hash(sig.c_str(), sig.length()),
            static_cast<uint32_t>(sig.length())
        };
    }

    /// Check if two types have matching verification (dual-hash + length)
    template <typename T1, typename T2>
    [[nodiscard]] consteval bool verifications_match() noexcept {
        return get_layout_verification<T1>() == get_layout_verification<T2>();
    }

    // =========================================================================
    // Collision Detection
    // =========================================================================

    /// Check no hash collision within a type library (compile-time)
    template<typename... Types>
    [[nodiscard]] consteval bool no_hash_collision() noexcept {
        if constexpr (sizeof...(Types) <= 1) {
            return true;
        } else {
            constexpr uint64_t hashes[] = { get_layout_hash<Types>()... };
            constexpr size_t N = sizeof...(Types);
            for (size_t i = 0; i < N; ++i) {
                for (size_t j = i + 1; j < N; ++j) {
                    if (hashes[i] == hashes[j]) return false;
                }
            }
            return true;
        }
    }

    /// Check no verification collision (dual-hash + length)
    template<typename... Types>
    [[nodiscard]] consteval bool no_verification_collision() noexcept {
        if constexpr (sizeof...(Types) <= 1) {
            return true;
        } else {
            constexpr LayoutVerification vs[] = { get_layout_verification<Types>()... };
            constexpr size_t N = sizeof...(Types);
            for (size_t i = 0; i < N; ++i) {
                for (size_t j = i + 1; j < N; ++j) {
                    if (vs[i] == vs[j]) return false;
                }
            }
            return true;
        }
    }

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_VERIFICATION_HPP
