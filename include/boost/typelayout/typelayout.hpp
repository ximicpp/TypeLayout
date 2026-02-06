// Boost.TypeLayout
//
// Primary Include Header
//
// This header provides the complete layout signature engine:
// - Compile-time layout signature generation
// - P2996 static reflection based type introspection
// - Binary compatibility verification
// - Layout concepts (LayoutCompatible, LayoutMatch)
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

// P2996 reflection helpers
#include <boost/typelayout/core/reflection_helpers.hpp>

// Type signature generation
#include <boost/typelayout/core/type_signature.hpp>

// Primary API: layout signature and hash
#include <boost/typelayout/core/signature.hpp>

// Verification utilities
#include <boost/typelayout/core/verification.hpp>

// Core concepts: LayoutCompatible, LayoutMatch
#include <boost/typelayout/core/concepts.hpp>

#endif // BOOST_TYPELAYOUT_HPP