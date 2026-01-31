// Boost.TypeLayout
//
// C++20 Concepts
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_CONCEPTS_HPP
#define BOOST_TYPELAYOUT_CONCEPTS_HPP

#include <boost/typelayout/signature.hpp>
#include <boost/typelayout/portability.hpp>

namespace boost {
namespace typelayout {

    // =========================================================================
    // Type Concepts
    // =========================================================================

    /// Type contains no platform-dependent members
    template<typename T>
    concept Portable = is_portable<T>();

    /// Two types have compatible memory layouts
    template<typename T, typename U>
    concept LayoutCompatible = signatures_match<T, U>();

    /// Type layout matches expected signature string
    template<typename T, fixed_string ExpectedSig>
    concept LayoutMatch = (get_layout_signature<T>() == static_cast<const char*>(ExpectedSig));

    /// Type layout hash matches expected hash value
    template<typename T, uint64_t ExpectedHash>
    concept LayoutHashMatch = (get_layout_hash<T>() == ExpectedHash);

    /// Two types have compatible layout hashes
    template<typename T, typename U>
    concept LayoutHashCompatible = hashes_match<T, U>();

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CONCEPTS_HPP
