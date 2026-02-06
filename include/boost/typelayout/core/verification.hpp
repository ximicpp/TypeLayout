// Boost.TypeLayout - Two-Layer Verification (v2.0)
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_VERIFICATION_HPP
#define BOOST_TYPELAYOUT_CORE_VERIFICATION_HPP

#include <boost/typelayout/core/signature.hpp>

namespace boost {
namespace typelayout {

    // ========================================================
    // Dual-hash Verification Structure
    // ========================================================

    /// Dual-hash verification: FNV-1a + DJB2 + length (~2^128 collision resistance)
    struct LayoutVerification {
        uint64_t fnv1a;     ///< FNV-1a 64-bit hash
        uint64_t djb2;      ///< DJB2 64-bit hash (independent algorithm)
        uint32_t length;    ///< Signature length
        
        constexpr bool operator==(const LayoutVerification&) const noexcept = default;
    };

    // ========================================================
    // Layer 1: Layout Verification
    // ========================================================

    /// Get dual-hash verification based on Layout signature
    template <typename T>
    [[nodiscard]] consteval LayoutVerification get_layout_verification() noexcept {
        constexpr auto sig = get_layout_signature<T>();
        return { 
            fnv1a_hash(sig.c_str(), sig.length()), 
            djb2_hash(sig.c_str(), sig.length()),
            static_cast<uint32_t>(sig.length())
        };
    }

    /// Check if two types have matching Layout verification
    template <typename T1, typename T2>
    [[nodiscard]] consteval bool layout_verifications_match() noexcept {
        return get_layout_verification<T1>() == get_layout_verification<T2>();
    }

    // ========================================================
    // Layer 2: Definition Verification
    // ========================================================

    /// Get dual-hash verification based on Definition signature
    template <typename T>
    [[nodiscard]] consteval LayoutVerification get_definition_verification() noexcept {
        constexpr auto sig = get_definition_signature<T>();
        return {
            fnv1a_hash(sig.c_str(), sig.length()),
            djb2_hash(sig.c_str(), sig.length()),
            static_cast<uint32_t>(sig.length())
        };
    }

    /// Check if two types have matching Definition verification
    template <typename T1, typename T2>
    [[nodiscard]] consteval bool definition_verifications_match() noexcept {
        return get_definition_verification<T1>() == get_definition_verification<T2>();
    }

    // ========================================================
    // Collision Detection
    // ========================================================

    /// Check no Layout hash collision within a type library (compile-time)
    template<typename... Types>
    [[nodiscard]] consteval bool no_hash_collision() noexcept {
        if constexpr (sizeof...(Types) <= 1) {
            return true;
        } else {
            constexpr uint64_t hashes[] = { get_layout_hash<Types>()... };
            constexpr size_t N = sizeof...(Types);
            for (size_t i = 0; i < N; ++i)
                for (size_t j = i + 1; j < N; ++j)
                    if (hashes[i] == hashes[j]) return false;
            return true;
        }
    }

    /// Check no Layout verification collision (dual-hash + length)
    template<typename... Types>
    [[nodiscard]] consteval bool no_verification_collision() noexcept {
        if constexpr (sizeof...(Types) <= 1) {
            return true;
        } else {
            constexpr LayoutVerification vs[] = { get_layout_verification<Types>()... };
            constexpr size_t N = sizeof...(Types);
            for (size_t i = 0; i < N; ++i)
                for (size_t j = i + 1; j < N; ++j)
                    if (vs[i] == vs[j]) return false;
            return true;
        }
    }

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_VERIFICATION_HPP