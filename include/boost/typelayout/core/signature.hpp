// Boost.TypeLayout - Two-Layer Signature API (v2.0)
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
        auto bits = CompileString<32>::from_number(sizeof(void*) * 8);
        return CompileString{"["} + bits + CompileString{TYPELAYOUT_LITTLE_ENDIAN ? "-le]" : "-be]"};
    }
}

// =============================================================================
// Layer 1: Layout Signature — Pure byte layout (flattened, no names)
// =============================================================================

/**
 * @brief Get the Layout signature for a type.
 * 
 * Layout signatures capture pure byte-level layout:
 * - Uses "record" prefix for all class types
 * - Flattens inheritance hierarchy
 * - No field names, no base class names, no structural markers
 * - Guarantees: identical byte layout → identical signature
 * 
 * Use case: shared memory, FFI, serialization, data exchange
 */
template <typename T>
[[nodiscard]] consteval auto get_layout_signature() noexcept {
    return get_arch_prefix() + TypeSignature<T, SignatureMode::Layout>::calculate();
}

/**
 * @brief Check if two types have identical Layout (byte-level) signatures.
 */
template <typename T1, typename T2>
[[nodiscard]] consteval bool layout_signatures_match() noexcept {
    return get_layout_signature<T1>() == get_layout_signature<T2>();
}

/**
 * @brief Get a 64-bit FNV-1a hash of the Layout signature.
 */
template <typename T>
[[nodiscard]] consteval uint64_t get_layout_hash() noexcept {
    constexpr auto sig = get_layout_signature<T>();
    return fnv1a_hash(sig.c_str(), sig.length());
}

/**
 * @brief Check if two types have identical Layout hashes.
 */
template <typename T1, typename T2>
[[nodiscard]] consteval bool layout_hashes_match() noexcept {
    return get_layout_hash<T1>() == get_layout_hash<T2>();
}

/**
 * @brief Get Layout signature as C string (for runtime/logging).
 */
template <typename T>
[[nodiscard]] constexpr const char* get_layout_signature_cstr() noexcept {
    static constexpr auto sig = get_layout_signature<T>();
    return sig.c_str();
}

// =============================================================================
// Layer 2: Definition Signature — Full type definition (tree, with names)
// =============================================================================

/**
 * @brief Get the Definition signature for a type.
 * 
 * Definition signatures capture complete type structure:
 * - Uses "record" prefix for all class types
 * - Preserves inheritance tree with ~base<Name> / ~vbase<Name>
 * - Includes field names as @OFF[name]:TYPE
 * - Includes "polymorphic" marker for types with virtual functions
 * - Does NOT include the outer type's own name
 * 
 * Use case: plugin ABI verification, ODR detection, version evolution
 */
template <typename T>
[[nodiscard]] consteval auto get_definition_signature() noexcept {
    return get_arch_prefix() + TypeSignature<T, SignatureMode::Definition>::calculate();
}

/**
 * @brief Check if two types have identical Definition signatures.
 */
template <typename T1, typename T2>
[[nodiscard]] consteval bool definition_signatures_match() noexcept {
    return get_definition_signature<T1>() == get_definition_signature<T2>();
}

/**
 * @brief Get a 64-bit FNV-1a hash of the Definition signature.
 */
template <typename T>
[[nodiscard]] consteval uint64_t get_definition_hash() noexcept {
    constexpr auto sig = get_definition_signature<T>();
    return fnv1a_hash(sig.c_str(), sig.length());
}

/**
 * @brief Check if two types have identical Definition hashes.
 */
template <typename T1, typename T2>
[[nodiscard]] consteval bool definition_hashes_match() noexcept {
    return get_definition_hash<T1>() == get_definition_hash<T2>();
}

/**
 * @brief Get Definition signature as C string (for runtime/logging).
 */
template <typename T>
[[nodiscard]] constexpr const char* get_definition_signature_cstr() noexcept {
    static constexpr auto sig = get_definition_signature<T>();
    return sig.c_str();
}

// =============================================================================
// Variable templates
// =============================================================================

template <typename T>
inline constexpr auto layout_signature_v = get_layout_signature<T>();

template <typename T>
inline constexpr uint64_t layout_hash_v = get_layout_hash<T>();

template <typename T>
inline constexpr auto definition_signature_v = get_definition_signature<T>();

template <typename T>
inline constexpr uint64_t definition_hash_v = get_definition_hash<T>();

} // namespace typelayout
} // namespace boost

// =============================================================================
// Macros
// =============================================================================

// Layout layer assertions
#define TYPELAYOUT_ASSERT_LAYOUT_COMPATIBLE(Type1, Type2) \
    static_assert(::boost::typelayout::layout_signatures_match<Type1, Type2>(), \
                  "Layout mismatch between " #Type1 " and " #Type2)

#define TYPELAYOUT_BIND_LAYOUT(Type, ExpectedSig) \
    static_assert(::boost::typelayout::get_layout_signature<Type>() == ExpectedSig, \
                  "Layout signature mismatch for " #Type)

// Definition layer assertions
#define TYPELAYOUT_ASSERT_DEFINITION_COMPATIBLE(Type1, Type2) \
    static_assert(::boost::typelayout::definition_signatures_match<Type1, Type2>(), \
                  "Definition mismatch between " #Type1 " and " #Type2)

#define TYPELAYOUT_BIND_DEFINITION(Type, ExpectedSig) \
    static_assert(::boost::typelayout::get_definition_signature<Type>() == ExpectedSig, \
                  "Definition signature mismatch for " #Type)

#endif // BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP