// Boost.TypeLayout
//
// Forward Declarations
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_FWD_HPP
#define BOOST_TYPELAYOUT_FWD_HPP

#include <cstddef>
#include <cstdint>

namespace boost {
namespace typelayout {

    // =========================================================================
    // Forward Declarations
    // =========================================================================

    // Compile-time string template
    template <std::size_t N>
    struct CompileString;

    // Type signature generator
    template <typename T>
    struct TypeSignature;

    // Layout verification structure
    struct LayoutVerification;

    // Fixed string for NTTP
    template <std::size_t N>
    struct fixed_string;

    // =========================================================================
    // Function Declarations
    // =========================================================================

    // Signature generation
    template <typename T>
    [[nodiscard]] consteval auto get_layout_signature() noexcept;

    template <typename T>
    [[nodiscard]] constexpr const char* get_layout_signature_cstr() noexcept;

    // Hash functions
    template <typename T>
    [[nodiscard]] consteval std::uint64_t get_layout_hash() noexcept;

    template <typename T>
    [[nodiscard]] consteval LayoutVerification get_layout_verification() noexcept;

    // Comparison functions
    template <typename T1, typename T2>
    [[nodiscard]] consteval bool signatures_match() noexcept;

    template <typename T1, typename T2>
    [[nodiscard]] consteval bool hashes_match() noexcept;

    template <typename T1, typename T2>
    [[nodiscard]] consteval bool verifications_match() noexcept;

    // Serialization safety checking
    template <typename T>
    [[nodiscard]] consteval bool is_trivially_serializable() noexcept;

    /// @deprecated Use is_trivially_serializable<T>() instead
    template <typename T>
    [[nodiscard]] [[deprecated("Use is_trivially_serializable<T>() instead")]]
    consteval bool is_portable() noexcept;

    template <typename T>
    [[nodiscard]] consteval bool has_bitfields() noexcept;

    // Collision detection
    template <typename... Types>
    [[nodiscard]] consteval bool no_hash_collision() noexcept;

    template <typename... Types>
    [[nodiscard]] consteval bool no_verification_collision() noexcept;

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_FWD_HPP