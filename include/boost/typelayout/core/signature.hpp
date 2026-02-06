// Boost.TypeLayout - Core Signature Generation API
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP
#define BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP

#include <boost/typelayout/core/config.hpp>
#include <boost/typelayout/core/compile_string.hpp>
#include <boost/typelayout/core/type_signature.hpp>
#include <boost/typelayout/utils/hash.hpp>

namespace boost {
namespace typelayout {

// Architecture prefix: "[64-le]", "[64-be]", "[32-le]", "[32-be]"
[[nodiscard]] consteval auto get_arch_prefix() noexcept {
    if constexpr (sizeof(void*) == 8) {
        return CompileString{TYPELAYOUT_LITTLE_ENDIAN ? "[64-le]" : "[64-be]"};
    } else if constexpr (sizeof(void*) == 4) {
        return CompileString{TYPELAYOUT_LITTLE_ENDIAN ? "[32-le]" : "[32-be]"};
    } else {
        // Exotic architectures
        auto bits = CompileString<32>::from_number(sizeof(void*) * 8);
        return CompileString{"["} + bits + CompileString{TYPELAYOUT_LITTLE_ENDIAN ? "-le]" : "-be]"};
    }
}

// =============================================================================
// Primary API: get_layout_signature with mode parameter
// =============================================================================

/**
 * @brief Get the layout signature for a type with the specified mode.
 * 
 * @tparam T The type to analyze
 * @tparam Mode The signature mode (default: Structural)
 *   - SignatureMode::Structural: Layout-only (no member/type names) - DEFAULT
 *     Guarantees: identical signature ⟺ identical memory layout
 *   - SignatureMode::Annotated: Includes member and type names for debugging
 * 
 * @return A compile-time string containing the layout signature
 * 
 * Example output (Structural mode):
 *   "[64-le]struct[s:16,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8]}"
 * 
 * Example output (Annotated mode):
 *   "[64-le]struct[s:16,a:8]{@0[id]:u32[s:4,a:4],@8[timestamp]:u64[s:8,a:8]}"
 */
template <typename T, SignatureMode Mode = SignatureMode::Structural>
[[nodiscard]] consteval auto get_layout_signature() noexcept {
    return get_arch_prefix() + TypeSignature<T, Mode>::calculate();
}

// =============================================================================
// Convenience aliases
// =============================================================================

/**
 * @brief Get the structural signature (layout-only, no names).
 * This is the default mode and guarantees: identical signature ⟺ identical layout.
 */
template <typename T>
[[nodiscard]] consteval auto get_structural_signature() noexcept {
    return get_layout_signature<T, SignatureMode::Structural>();
}

/**
 * @brief Get the annotated signature (includes member and type names).
 * Useful for debugging and diagnostics, NOT for layout comparison.
 */
template <typename T>
[[nodiscard]] consteval auto get_annotated_signature() noexcept {
    return get_layout_signature<T, SignatureMode::Annotated>();
}

// =============================================================================
// Signature comparison (always uses Structural mode)
// =============================================================================

/**
 * @brief Check if two types have identical layout signatures.
 * ALWAYS uses Structural mode to ensure the comparison is layout-based only.
 */
template <typename T1, typename T2>
[[nodiscard]] consteval bool signatures_match() noexcept {
    return get_layout_signature<T1, SignatureMode::Structural>() 
        == get_layout_signature<T2, SignatureMode::Structural>();
}

// =============================================================================
// C string access
// =============================================================================

// Signature as C string (for runtime/logging) - Structural mode
template <typename T>
[[nodiscard]] constexpr const char* get_layout_signature_cstr() noexcept {
    static constexpr auto sig = get_layout_signature<T, SignatureMode::Structural>();
    return sig.c_str();
}

// Annotated signature as C string (for debugging)
template <typename T>
[[nodiscard]] constexpr const char* get_annotated_signature_cstr() noexcept {
    static constexpr auto sig = get_layout_signature<T, SignatureMode::Annotated>();
    return sig.c_str();
}

// =============================================================================
// Hash computation (ALWAYS based on Structural signature)
// =============================================================================

/**
 * @brief Get a 64-bit FNV-1a hash of the type's memory layout.
 * 
 * ALWAYS computed from structural layout (names never affect the hash).
 * This guarantees: same layout → same hash, regardless of member names.
 * 
 * @note Due to current P2996 toolchain limitations, large structures
 * (>50 members) may hit constexpr step limits. This will improve as
 * the toolchain matures.
 */
template <typename T>
[[nodiscard]] consteval uint64_t get_layout_hash() noexcept {
    // Compute hash from structural signature
    constexpr auto sig = get_layout_signature<T, SignatureMode::Structural>();
    return fnv1a_hash(sig.c_str(), sig.length());
}

/**
 * @brief Get layout hash from signature string (legacy method).
 * @deprecated Use get_layout_hash<T>() instead.
 */
template <typename T>
[[deprecated("Use get_layout_hash<T>() instead")]]
[[nodiscard]] consteval uint64_t get_layout_hash_from_signature() noexcept {
    return get_layout_hash<T>();
}

/**
 * @brief Check if two types have identical layout hashes.
 * Based on Structural layouts, so name differences don't affect the result.
 */
template <typename T1, typename T2>
[[nodiscard]] consteval bool hashes_match() noexcept {
    return get_layout_hash<T1>() == get_layout_hash<T2>();
}

// =============================================================================
// Variable templates (modern C++ style)
// =============================================================================

template <typename T>
inline constexpr uint64_t layout_hash_v = get_layout_hash<T>();

template <typename T>
inline constexpr auto layout_signature_v = get_layout_signature<T, SignatureMode::Structural>();

template <typename T>
inline constexpr auto annotated_signature_v = get_layout_signature<T, SignatureMode::Annotated>();

} // namespace typelayout
} // namespace boost

// =============================================================================
// Macros
// =============================================================================

// Static assertion: bind type to expected signature (Structural mode)
#define TYPELAYOUT_BIND(Type, ExpectedSig) \
    static_assert(::boost::typelayout::get_layout_signature<Type, ::boost::typelayout::SignatureMode::Structural>() == ExpectedSig, \
                  "Layout mismatch for " #Type)

// Static assertion: verify two types have identical layouts
#define TYPELAYOUT_ASSERT_COMPATIBLE(Type1, Type2) \
    static_assert(::boost::typelayout::signatures_match<Type1, Type2>(), \
                  "Layout mismatch between " #Type1 " and " #Type2)

#endif // BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP