// Convenience macros for registering opaque container type signatures.
// Use these when a container's internal layout should be hidden behind a
// fixed-size/alignment descriptor (e.g., shared-memory containers whose
// internals are implementation-defined).
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_OPAQUE_HPP
#define BOOST_TYPELAYOUT_CORE_OPAQUE_HPP

#include <boost/typelayout/core/fwd.hpp>

// ---------------------------------------------------------------------------
// TYPELAYOUT_OPAQUE_TYPE(Type, name, size, align)
//
// Register a non-template type with a fixed opaque signature.
//   Type  — fully qualified type name (e.g., MyLib::XString)
//   name  — short display name (e.g., "string")
//   size  — sizeof(Type)
//   align — alignof(Type)
//
// Example:
//   TYPELAYOUT_OPAQUE_TYPE(MyLib::XString, "string", 32, 8)
//   // generates: string[s:32,a:8]
// ---------------------------------------------------------------------------
#define TYPELAYOUT_OPAQUE_TYPE(Type, name, size, align)                        \
    template <::boost::typelayout::SignatureMode Mode_>                         \
    struct TypeSignature<Type, Mode_> {                                         \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::FixedString{                           \
                name "[s:" #size ",a:" #align "]"};                            \
        }                                                                      \
    };

// ---------------------------------------------------------------------------
// TYPELAYOUT_OPAQUE_CONTAINER(Template, name, size, align)
//
// Register a single-type-parameter template with an opaque signature
// that includes the element type's signature.
//   Template — template name without <T> (e.g., MyLib::XVector)
//   name     — short display name (e.g., "vector")
//   size     — sizeof(Template<T>) (must be constant for all T)
//   align    — alignof(Template<T>)
//
// Example:
//   TYPELAYOUT_OPAQUE_CONTAINER(MyLib::XVector, "vector", 32, 8)
//   // generates: vector[s:32,a:8]<element_signature>
// ---------------------------------------------------------------------------
#define TYPELAYOUT_OPAQUE_CONTAINER(Template, name, size, align)               \
    template <typename T_, ::boost::typelayout::SignatureMode Mode_>            \
    struct TypeSignature<Template<T_>, Mode_> {                                \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::FixedString{                           \
                       name "[s:" #size ",a:" #align "]<"} +                   \
                   TypeSignature<T_, Mode_>::calculate() +                     \
                   ::boost::typelayout::FixedString{">"};                      \
        }                                                                      \
    };

// ---------------------------------------------------------------------------
// TYPELAYOUT_OPAQUE_MAP(Template, name, size, align)
//
// Register a two-type-parameter template with an opaque signature
// that includes both key and value type signatures.
//   Template — template name without <K,V> (e.g., MyLib::XMap)
//   name     — short display name (e.g., "map")
//   size     — sizeof(Template<K,V>) (must be constant for all K,V)
//   align    — alignof(Template<K,V>)
//
// Example:
//   TYPELAYOUT_OPAQUE_MAP(MyLib::XMap, "map", 32, 8)
//   // generates: map[s:32,a:8]<key_signature,value_signature>
// ---------------------------------------------------------------------------
#define TYPELAYOUT_OPAQUE_MAP(Template, name, size, align)                     \
    template <typename K_, typename V_,                                         \
              ::boost::typelayout::SignatureMode Mode_>                         \
    struct TypeSignature<Template<K_, V_>, Mode_> {                            \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::FixedString{                           \
                       name "[s:" #size ",a:" #align "]<"} +                   \
                   TypeSignature<K_, Mode_>::calculate() +                     \
                   ::boost::typelayout::FixedString{","} +                     \
                   TypeSignature<V_, Mode_>::calculate() +                     \
                   ::boost::typelayout::FixedString{">"};                      \
        }                                                                      \
    };

#endif // BOOST_TYPELAYOUT_CORE_OPAQUE_HPP
