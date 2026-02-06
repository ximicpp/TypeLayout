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
template<typename T, typename U>
concept LayoutCompatible = signatures_match<T, U>();

// Type matches expected signature string
template<typename T, fixed_string ExpectedSig>
concept LayoutMatch = (get_layout_signature<T>() == static_cast<const char*>(ExpectedSig));

// Type hash matches expected value
template<typename T, uint64_t ExpectedHash>
concept LayoutHashMatch = (get_layout_hash<T>() == ExpectedHash);

// Two types have identical layout hashes
template<typename T, typename U>
concept LayoutHashCompatible = hashes_match<T, U>();

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_CONCEPTS_HPP