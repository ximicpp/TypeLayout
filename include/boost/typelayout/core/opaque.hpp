// Convenience macros for registering opaque container type signatures.
// Use these when a container's internal layout should be hidden behind a
// fixed-size/alignment descriptor (e.g., shared-memory containers whose
// internals are implementation-defined).
//
// DESIGN NOTE — Opaque vs. Two-Layer Signatures:
//   Opaque types produce the SAME signature in both Layout and Definition
//   modes, because no internal structure is available to differentiate.
//   This means opaque types act as an incomplete supplement to the Layout
//   layer: they provide sizeof/alignof identity but NOT field-level identity.
//
//   Correctness boundary:
//     - TypeLayout guarantees: sizeof and alignof match (via static_assert).
//     - User guarantees: internal layout consistency across compilation units.
//   The Encoding Faithfulness theorem (Thm 4.8) holds for opaque types
//   only under the assumption that user-provided annotations are correct
//   (Opaque Annotation Correctness axiom).
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
    static_assert(sizeof(Type) == (size),                                       \
        "TYPELAYOUT_OPAQUE_TYPE: size does not match sizeof(" #Type ")");       \
    static_assert(alignof(Type) == (align),                                     \
        "TYPELAYOUT_OPAQUE_TYPE: align does not match alignof(" #Type ")");     \
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
            static_assert(sizeof(Template<T_>) == (size),                       \
                "TYPELAYOUT_OPAQUE_CONTAINER: size does not match "             \
                "sizeof(" #Template "<T>)");                                    \
            static_assert(alignof(Template<T_>) == (align),                     \
                "TYPELAYOUT_OPAQUE_CONTAINER: align does not match "            \
                "alignof(" #Template "<T>)");                                   \
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
            static_assert(sizeof(Template<K_, V_>) == (size),                   \
                "TYPELAYOUT_OPAQUE_MAP: size does not match "                   \
                "sizeof(" #Template "<K,V>)");                                  \
            static_assert(alignof(Template<K_, V_>) == (align),                 \
                "TYPELAYOUT_OPAQUE_MAP: align does not match "                  \
                "alignof(" #Template "<K,V>)");                                 \
            return ::boost::typelayout::FixedString{                           \
                       name "[s:" #size ",a:" #align "]<"} +                   \
                   TypeSignature<K_, Mode_>::calculate() +                     \
                   ::boost::typelayout::FixedString{","} +                     \
                   TypeSignature<V_, Mode_>::calculate() +                     \
                   ::boost::typelayout::FixedString{">"};                      \
        }                                                                      \
    };

#endif // BOOST_TYPELAYOUT_CORE_OPAQUE_HPP
