// Boost.TypeLayout - C++20 Concepts
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_CONCEPTS_HPP
#define BOOST_TYPELAYOUT_CORE_CONCEPTS_HPP

#include <boost/typelayout/core/signature.hpp>

namespace boost {
namespace typelayout {

// Type has determinable layout (excludes void, function types, unbounded arrays)
// Uses has_determinable_layout_v trait to avoid static_assert hard errors
template<typename T>
concept LayoutSupported = has_determinable_layout_v<T>;

// Two types have identical layout signatures
// Requires both types to have determinable layouts first (SFINAE-friendly)
template<typename T, typename U>
concept LayoutCompatible = LayoutSupported<T> && LayoutSupported<U> 
                           && signatures_match<T, U>();

// Type matches expected signature string
// Requires the type to have a determinable layout first
template<typename T, fixed_string ExpectedSig>
concept LayoutMatch = LayoutSupported<T> 
                      && (get_layout_signature<T>() == static_cast<const char*>(ExpectedSig));

// Type hash matches expected value
// Requires the type to have a determinable layout first
template<typename T, uint64_t ExpectedHash>
concept LayoutHashMatch = LayoutSupported<T> 
                          && (get_layout_hash<T>() == ExpectedHash);

// Two types have identical layout hashes
// Requires both types to have determinable layouts first (SFINAE-friendly)
template<typename T, typename U>
concept LayoutHashCompatible = LayoutSupported<T> && LayoutSupported<U> 
                               && hashes_match<T, U>();

// =============================================================================
// Physical layout concepts (ignore inheritance structure)
// =============================================================================

// Two types have identical physical byte layout (ignores inheritance structure)
template<typename T, typename U>
concept PhysicalLayoutCompatible = LayoutSupported<T> && LayoutSupported<U>
                                   && physical_signatures_match<T, U>();

// Physical hash match
template<typename T, typename U>
concept PhysicalHashCompatible = LayoutSupported<T> && LayoutSupported<U>
                                 && physical_hashes_match<T, U>();

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_CONCEPTS_HPP
