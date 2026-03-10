// Macros for registering opaque type signatures.
// Use these when a type's internal layout should be hidden behind a
// fixed-size/alignment descriptor (e.g., types whose internals are
// implementation-defined or not analyzable via reflection).
//
// DESIGN NOTE -- Opaque Signatures:
//   Opaque types produce a signature based solely on sizeof/alignof,
//   bypassing field-level introspection. They provide size and alignment
//   identity but NOT internal structural identity.
//
//   Correctness boundary:
//     - TypeLayout guarantees: sizeof and alignof match (via static_assert).
//     - User guarantees: internal layout consistency across compilation units.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_OPAQUE_HPP
#define BOOST_TYPELAYOUT_OPAQUE_HPP

#include <boost/typelayout/fixed_string.hpp>

// ===========================================================================
// TYPELAYOUT_REGISTER_OPAQUE -- Register an opaque type
//
// Registers an opaque type whose internal layout is invisible to
// TypeLayout.  The user provides a semantic tag string and declares
// whether the type contains pointers.
//
// sizeof and alignof are deduced automatically from the type.
//
// Signature format:  O(Tag|size|alignment)
//
// Parameters:
//   Type        -- fully qualified type name
//   Tag         -- semantic tag string (must be globally unique across
//                  all registered opaque types)
//   HasPointer  -- bool: true if the type contains pointers.
//                  If true, has_pointer will be true, preventing
//                  serialization-free transfer.
//
// User responsibilities:
//   1. Tag is unique among all registered opaque types.
//   2. HasPointer accurately reflects the type's contents.
//   3. The type is trivially_copyable (enforced by static_assert).
//   4. Both endpoints use the same opaque type definition (layout match).
//
// Example:
//   TYPELAYOUT_REGISTER_OPAQUE(AesKey256, "AesKey256", false)
//   // signature: "[64-le]O(AesKey256|32|1)"
// ===========================================================================
#define TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, HasPointer)                      \
    static_assert(std::is_trivially_copyable_v<Type>,                           \
        "TYPELAYOUT_REGISTER_OPAQUE: opaque type must be trivially copyable");  \
    template <>                                                                 \
    struct TypeSignature<Type> {                                                \
        static constexpr bool is_opaque = true;                                \
        static constexpr bool pointer_free = !(HasPointer);                    \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::FixedString{"O("} +                    \
                   ::boost::typelayout::FixedString{Tag} +                     \
                   ::boost::typelayout::FixedString{"|"} +                     \
                   ::boost::typelayout::to_fixed_string<sizeof(Type)>() +      \
                   ::boost::typelayout::FixedString{"|"} +                     \
                   ::boost::typelayout::to_fixed_string<alignof(Type)>() +     \
                   ::boost::typelayout::FixedString{")"};                      \
        }                                                                      \
    };

#endif // BOOST_TYPELAYOUT_OPAQUE_HPP
