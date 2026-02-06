// Boost.TypeLayout - C++20 Concepts (v2.0 Two-Layer)
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_CONCEPTS_HPP
#define BOOST_TYPELAYOUT_CORE_CONCEPTS_HPP

#include <boost/typelayout/core/signature.hpp>

namespace boost {
namespace typelayout {

// Type has determinable layout (excludes void, function types, unbounded arrays)
template<typename T>
concept LayoutSupported = has_determinable_layout_v<T>;

// =============================================================================
// Layer 1: Layout (byte-level) concepts
// =============================================================================

// Two types have identical Layout (byte-level) signatures
template<typename T, typename U>
concept LayoutCompatible = LayoutSupported<T> && LayoutSupported<U>
                           && layout_signatures_match<T, U>();

// Two types have identical Layout hashes
template<typename T, typename U>
concept LayoutHashCompatible = LayoutSupported<T> && LayoutSupported<U>
                               && layout_hashes_match<T, U>();

// =============================================================================
// Layer 2: Definition (structural) concepts
// =============================================================================

// Two types have identical Definition signatures
template<typename T, typename U>
concept DefinitionCompatible = LayoutSupported<T> && LayoutSupported<U>
                               && definition_signatures_match<T, U>();

// Two types have identical Definition hashes
template<typename T, typename U>
concept DefinitionHashCompatible = LayoutSupported<T> && LayoutSupported<U>
                                   && definition_hashes_match<T, U>();

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_CONCEPTS_HPP