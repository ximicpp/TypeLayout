// Boost.TypeLayout
//
// Core C++20 Concepts
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_CORE_CONCEPTS_HPP
#define BOOST_TYPELAYOUT_CORE_CONCEPTS_HPP

#include <boost/typelayout/core/signature.hpp>

namespace boost {
namespace typelayout {

    /**
     * @defgroup core_concepts Core Concepts
     * @brief C++20 concepts for compile-time layout validation.
     * @{
     */

    // =========================================================================
    // Core Layout Concepts
    // =========================================================================

    /**
     * @brief Concept: Type has determinable memory layout.
     * 
     * Satisfied when type T can have its layout signature computed at compile-time.
     * This is true for most types except: void, function types, incomplete types.
     * 
     * @tparam T The type to check
     * 
     * @par Example:
     * @code
     * template<typename T>
     *     requires LayoutSupported<T>
     * void serialize(const T& value);
     * @endcode
     */
    template<typename T>
    concept LayoutSupported = requires {
        { get_layout_signature<T>() };
    };

    /**
     * @brief Concept: Two types have compatible memory layouts.
     * 
     * Satisfied when types T and U have identical layout signatures.
     * 
     * @tparam T First type to compare
     * @tparam U Second type to compare
     * 
     * @par Example:
     * @code
     * template<typename T, typename U>
     *     requires LayoutCompatible<T, U>
     * void safe_memcpy(T& dest, const U& src);
     * @endcode
     */
    template<typename T, typename U>
    concept LayoutCompatible = signatures_match<T, U>();

    /**
     * @brief Concept: Type layout matches expected signature string.
     * 
     * Satisfied when type T's layout signature matches ExpectedSig.
     * 
     * @tparam T The type to verify
     * @tparam ExpectedSig The expected signature as fixed string
     */
    template<typename T, fixed_string ExpectedSig>
    concept LayoutMatch = (get_layout_signature<T>() == static_cast<const char*>(ExpectedSig));

    /**
     * @brief Concept: Type layout hash matches expected hash value.
     * 
     * Satisfied when type T's 64-bit layout hash equals ExpectedHash.
     * 
     * @tparam T The type to verify
     * @tparam ExpectedHash The expected 64-bit hash value
     */
    template<typename T, uint64_t ExpectedHash>
    concept LayoutHashMatch = (get_layout_hash<T>() == ExpectedHash);

    /**
     * @brief Concept: Two types have compatible layout hashes.
     * 
     * Satisfied when types T and U have identical 64-bit layout hashes.
     * 
     * @tparam T First type to compare
     * @tparam U Second type to compare
     */
    template<typename T, typename U>
    concept LayoutHashCompatible = hashes_match<T, U>();

    /** @} */ // end of core_concepts group

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_CONCEPTS_HPP
