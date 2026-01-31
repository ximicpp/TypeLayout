// Boost.TypeLayout
//
// Compile-time memory layout signature generator using C++26 static reflection (P2996)
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// This is a convenience header that includes all TypeLayout components.
// For fine-grained control, include individual headers:
//
//   #include <boost/typelayout/signature.hpp>      // Core signature API
//   #include <boost/typelayout/verification.hpp>  // Dual-hash verification
//   #include <boost/typelayout/portability.hpp>   // Portability checking
//   #include <boost/typelayout/concepts.hpp>      // C++20 Concepts
//   #include <boost/typelayout/fwd.hpp>           // Forward declarations
//
// Constraints:
//   - IEEE 754 floating-point required
//   - Platform info encoded in signature header (architecture + endianness)
//
// Guarantees:
//   - Identical signature => Identical memory layout on same platform
//
// Requirements:
//   - C++26 with P2996 static reflection support
//   - Tested with Bloomberg Clang P2996 fork

#ifndef BOOST_TYPELAYOUT_TYPELAYOUT_HPP
#define BOOST_TYPELAYOUT_TYPELAYOUT_HPP

// Core components
#include <boost/typelayout/detail/config.hpp>
#include <boost/typelayout/detail/compile_string.hpp>
#include <boost/typelayout/detail/hash.hpp>
#include <boost/typelayout/detail/reflection_helpers.hpp>
#include <boost/typelayout/detail/type_signature.hpp>

// Public API
#include <boost/typelayout/signature.hpp>
#include <boost/typelayout/verification.hpp>
#include <boost/typelayout/portability.hpp>
#include <boost/typelayout/concepts.hpp>

#endif // BOOST_TYPELAYOUT_TYPELAYOUT_HPP