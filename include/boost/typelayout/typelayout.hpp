// Boost.TypeLayout v2.0
//
// Primary Include Header
//
// This header provides the two-layer signature engine:
// - Layer 1 (Layout):     Pure byte layout, flattened, no names
// - Layer 2 (Definition): Full type definition, tree, with names
// - P2996 static reflection based type introspection
// - Binary compatibility verification
// - Layout concepts (LayoutCompatible, DefinitionCompatible)
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_HPP
#define BOOST_TYPELAYOUT_HPP

// Core configuration
#include <boost/typelayout/core/config.hpp>

// Compile-time string utilities
#include <boost/typelayout/core/compile_string.hpp>

// Type signature generation (includes reflection_helpers internally)
#include <boost/typelayout/core/type_signature.hpp>

// Primary API: layout signature and hash
#include <boost/typelayout/core/signature.hpp>

// Verification utilities
#include <boost/typelayout/core/verification.hpp>

// Core concepts: LayoutCompatible, DefinitionCompatible
#include <boost/typelayout/core/concepts.hpp>

#endif // BOOST_TYPELAYOUT_HPP