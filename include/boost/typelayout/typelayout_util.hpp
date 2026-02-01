// Boost.TypeLayout
//
// Utility Include Header - Serialization Utilities
//
// This header provides the UTILITY layer (Layer 2) built on core:
// - Platform set configuration for cross-platform serialization
// - Serialization safety checking (is_serializable_v)
// - Serialization concepts (Serializable, ZeroCopyTransmittable)
// - Bit-field detection
//
// Note: This header automatically includes the core layer.
//
// For core-only functionality, include <boost/typelayout/typelayout.hpp>
// For everything, include <boost/typelayout/typelayout_all.hpp>
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_UTIL_HPP
#define BOOST_TYPELAYOUT_UTIL_HPP

// Core layer (required dependency)
#include <boost/typelayout/typelayout.hpp>

// Platform set definitions and basic serialization checks
#include <boost/typelayout/util/platform_set.hpp>

// Full serialization checking with P2996 reflection
#include <boost/typelayout/util/serialization_check.hpp>

// Serialization concepts: Serializable, ZeroCopyTransmittable, etc.
#include <boost/typelayout/util/concepts.hpp>

#endif // BOOST_TYPELAYOUT_UTIL_HPP
