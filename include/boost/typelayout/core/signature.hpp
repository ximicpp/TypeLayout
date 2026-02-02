// Boost.TypeLayout
//
// Core Signature Generation Public API
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP
#define BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP

#include <boost/typelayout/core/config.hpp>
#include <boost/typelayout/core/compile_string.hpp>
#include <boost/typelayout/core/type_signature.hpp>
#include <boost/typelayout/core/hash.hpp>

namespace boost {
namespace typelayout {

    /**
     * @defgroup core_api Core API
     * @brief Core functions for compile-time type layout analysis.
     * @{
     */

    // =========================================================================
    // Architecture Prefix Generation
    // =========================================================================
    
    /**
     * @brief Generate architecture prefix string for the current platform.
     * 
     * Creates a compile-time string describing the current platform's pointer
     * size and endianness. This prefix is included in all layout signatures
     * to ensure cross-platform compatibility checks.
     * 
     * @return CompileString containing architecture prefix, e.g., "[64-le]" or "[32-be]"
     * 
     * @note This function is `consteval` and executes entirely at compile-time.
     * 
     * @par Format:
     * - `[64-le]` - 64-bit little-endian (x86-64, ARM64 LE)
     * - `[64-be]` - 64-bit big-endian (PPC64, SPARC64)
     * - `[32-le]` - 32-bit little-endian (x86, ARM LE)
     * - `[32-be]` - 32-bit big-endian (PPC, SPARC)
     * 
     * @par Example:
     * @code
     * constexpr auto prefix = boost::typelayout::get_arch_prefix();
     * // On x86-64: prefix == "[64-le]"
     * @endcode
     */
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
    // Core Public API
    // =========================================================================

    /**
     * @brief Get compile-time layout signature for a type.
     * 
     * Generates a unique string that describes the complete memory layout of type T,
     * including:
     * - Architecture prefix (pointer size and endianness)
     * - Type size and alignment
     * - Field names, types, and offsets (for structs/classes)
     * - Bit-field layout details (if present)
     * - Base class information (for inheritance)
     * 
     * @tparam T The type to generate a signature for
     * @return CompileString containing the complete layout signature
     * 
     * @note This function is `consteval` and executes entirely at compile-time.
     * @note Requires C++26 with P2996 reflection support.
     * 
     * @par Format:
     * `[BITS-ENDIAN]<type_descriptor>`
     * 
     * @par Examples:
     * @code
     * // Primitive types
     * constexpr auto int_sig = get_layout_signature<int>();
     * // Result: "[64-le]i32[s:4,a:4]"
     * 
     * // Struct types
     * struct Point { int x; int y; };
     * constexpr auto point_sig = get_layout_signature<Point>();
     * // Result: "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}"
     * 
     * // Arrays
     * constexpr auto arr_sig = get_layout_signature<int[10]>();
     * // Result: "[64-le]array[s:40,a:4]<i32[s:4,a:4],10>"
     * @endcode
     * 
     * @see get_layout_hash() For a compact 64-bit hash of the signature
     * @see signatures_match() For comparing two type signatures
     */
    template <typename T>
    [[nodiscard]] consteval auto get_layout_signature() noexcept {
        return get_arch_prefix() + TypeSignature<T>::calculate();
    }

    /**
     * @brief Check if two types have identical memory layouts.
     * 
     * Compares the complete layout signatures of two types, including
     * architecture prefix. Returns true only if both types have the
     * exact same memory representation on the current platform.
     * 
     * @tparam T1 First type to compare
     * @tparam T2 Second type to compare
     * @return true if layouts are identical, false otherwise
     * 
     * @note This function is `consteval` and executes entirely at compile-time.
     * 
     * @par Example:
     * @code
     * struct A { int x; int y; };
     * struct B { int a; int b; };
     * struct C { int x; double y; };
     * 
     * static_assert(signatures_match<A, B>());  // Same layout, different names
     * static_assert(!signatures_match<A, C>()); // Different layouts
     * @endcode
     * 
     * @see get_layout_signature() For the underlying signature generation
     * @see hashes_match() For hash-based comparison
     */
    template <typename T1, typename T2>
    [[nodiscard]] consteval bool signatures_match() noexcept {
        return get_layout_signature<T1>() == get_layout_signature<T2>();
    }

    /**
     * @brief Get layout signature as a null-terminated C string.
     * 
     * Returns a pointer to a static null-terminated string containing
     * the layout signature. Useful for runtime logging, debugging,
     * or interfacing with C APIs.
     * 
     * @tparam T The type to get the signature for
     * @return const char* Pointer to the null-terminated signature string
     * 
     * @note The returned pointer is valid for the lifetime of the program.
     * @note This function is `constexpr` and can be used at compile-time.
     * 
     * @par Example:
     * @code
     * struct Point { int x; int y; };
     * 
     * // Runtime usage
     * std::cout << "Point layout: " << get_layout_signature_cstr<Point>() << std::endl;
     * 
     * // Protocol logging
     * void send_data(const void* data, size_t size, const char* type_sig);
     * send_data(&point, sizeof(Point), get_layout_signature_cstr<Point>());
     * @endcode
     */
    template <typename T>
    [[nodiscard]] constexpr const char* get_layout_signature_cstr() noexcept {
        static constexpr auto sig = get_layout_signature<T>();
        return sig.c_str();
    }

    /**
     * @brief Get a 64-bit hash of the type's layout signature.
     * 
     * Computes an FNV-1a hash of the complete layout signature. This hash
     * can be used for:
     * - Compact storage in protocol headers
     * - Fast runtime validation
     * - Hash table keys
     * 
     * @tparam T The type to hash
     * @return uint64_t 64-bit FNV-1a hash of the layout signature
     * 
     * @note This function is `consteval` and executes entirely at compile-time.
     * @note Hash collisions are theoretically possible but extremely rare.
     * 
     * @par Example:
     * @code
     * struct Message { uint32_t id; uint64_t timestamp; char payload[64]; };
     * 
     * // Protocol header with type hash
     * struct Header {
     *     uint64_t type_hash;
     *     uint32_t payload_size;
     * };
     * 
     * constexpr uint64_t MESSAGE_HASH = get_layout_hash<Message>();
     * 
     * // Validation at runtime
     * bool validate_message(const Header& header) {
     *     return header.type_hash == MESSAGE_HASH;
     * }
     * @endcode
     * 
     * @see get_layout_signature() For the full string signature
     * @see hashes_match() For comparing two type hashes
     */
    template <typename T>
    [[nodiscard]] consteval uint64_t get_layout_hash() noexcept {
        constexpr auto sig = get_layout_signature<T>();
        return fnv1a_hash(sig.c_str(), sig.length());
    }

    /**
     * @brief Check if two types have the same layout hash.
     * 
     * Compares the 64-bit FNV-1a hashes of two types' layout signatures.
     * This is equivalent to `get_layout_hash<T1>() == get_layout_hash<T2>()`
     * but may be more efficient in some contexts.
     * 
     * @tparam T1 First type to compare
     * @tparam T2 Second type to compare
     * @return true if hashes are identical, false otherwise
     * 
     * @note This function is `consteval` and executes entirely at compile-time.
     * @note Hash comparison is faster than full signature comparison.
     * 
     * @par Example:
     * @code
     * struct NetworkPacket { uint32_t header; uint8_t data[256]; };
     * struct LocalPacket { uint32_t header; uint8_t data[256]; };
     * 
     * // Verify binary compatibility
     * static_assert(hashes_match<NetworkPacket, LocalPacket>(),
     *               "Packet types must have compatible layouts");
     * @endcode
     * 
     * @see get_layout_hash() For the underlying hash computation
     * @see signatures_match() For full signature comparison
     */
    template <typename T1, typename T2>
    [[nodiscard]] consteval bool hashes_match() noexcept {
        return get_layout_hash<T1>() == get_layout_hash<T2>();
    }

    /** @} */ // end of core_api group

} // namespace typelayout
} // namespace boost

/**
 * @brief Static assertion macro for layout verification.
 * 
 * Binds a type to an expected signature string and fails compilation
 * if the type's actual layout differs from the expected value.
 * Useful for:
 * - ABI stability checks
 * - Protocol versioning
 * - Regression testing
 * 
 * @param Type The type to verify
 * @param ExpectedSig The expected signature string (CompileString)
 * 
 * @par Example:
 * @code
 * struct Point { int x; int y; };
 * 
 * // Lock down the expected layout
 * TYPELAYOUT_BIND(Point, "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}");
 * 
 * // If someone modifies Point, compilation will fail
 * @endcode
 */
#define TYPELAYOUT_BIND(Type, ExpectedSig) \
    static_assert(::boost::typelayout::get_layout_signature<Type>() == ExpectedSig, \
                  "Layout mismatch for " #Type)

#endif // BOOST_TYPELAYOUT_CORE_SIGNATURE_HPP