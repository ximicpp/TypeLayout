// Convenience macros for registering opaque container type signatures.
// Use these when a container's internal layout should be hidden behind a
// fixed-size/alignment descriptor (e.g., shared-memory containers whose
// internals are implementation-defined).
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

// ---------------------------------------------------------------------------
// TYPELAYOUT_OPAQUE_TYPE(Type, name, size, align)
//
// Register a non-template type with a fixed opaque signature.
//   Type  -- fully qualified type name (e.g., MyLib::XString)
//   name  -- short display name (e.g., "string")
//   size  -- sizeof(Type)
//   align -- alignof(Type)
//
// pointer_free is set to false (conservative default): since the type's
// internals are opaque, the library cannot determine whether it contains
// pointers.  If you know the type is pointer-free and want TrivialSafe
// classification, use TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, false) instead.
//
// Example:
//   TYPELAYOUT_OPAQUE_TYPE(MyLib::XString, "string", 32, 8)
//   // generates: O!string[s:32,a:8]
// ---------------------------------------------------------------------------
#define TYPELAYOUT_OPAQUE_TYPE(Type, name, size, align)                        \
    static_assert(sizeof(Type) == (size),                                       \
        "TYPELAYOUT_OPAQUE_TYPE: size does not match sizeof(" #Type ")");       \
    static_assert(alignof(Type) == (align),                                     \
        "TYPELAYOUT_OPAQUE_TYPE: align does not match alignof(" #Type ")");     \
    template <>                                                                 \
    struct TypeSignature<Type> {                                                \
        static constexpr bool is_opaque = true;                                \
        static constexpr bool pointer_free = false;                            \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::FixedString{                           \
                "O!" name "[s:" #size ",a:" #align "]"};                       \
        }                                                                      \
    };

// ---------------------------------------------------------------------------
// TYPELAYOUT_OPAQUE_CONTAINER(Template, name, size, align)
//
// Register a single-type-parameter template with an opaque signature
// that includes the element type's signature.
//   Template -- template name without <T> (e.g., MyLib::XVector)
//   name     -- short display name (e.g., "vector")
//   size     -- sizeof(Template<T>) (must be constant for all T)
//   align    -- alignof(Template<T>)
//
// Example:
//   TYPELAYOUT_OPAQUE_CONTAINER(MyLib::XVector, "vector", 32, 8)
//   // generates: O!vector[s:32,a:8]<element_signature>
// ---------------------------------------------------------------------------
#define TYPELAYOUT_OPAQUE_CONTAINER(Template, name, size, align)               \
    template <typename T_>                                                      \
    struct TypeSignature<Template<T_>> {                                        \
        static constexpr bool is_opaque = true;                                \
        static constexpr bool pointer_free = false;                            \
        static consteval auto calculate() noexcept {                           \
            static_assert(sizeof(Template<T_>) == (size),                       \
                "TYPELAYOUT_OPAQUE_CONTAINER: size does not match "             \
                "sizeof(" #Template "<T>)");                                    \
            static_assert(alignof(Template<T_>) == (align),                     \
                "TYPELAYOUT_OPAQUE_CONTAINER: align does not match "            \
                "alignof(" #Template "<T>)");                                   \
            return ::boost::typelayout::FixedString{                           \
                       "O!" name "[s:" #size ",a:" #align "]<"} +                   \
                   TypeSignature<T_>::calculate() +                            \
                   ::boost::typelayout::FixedString{">"};                      \
        }                                                                      \
    };

// ---------------------------------------------------------------------------
// TYPELAYOUT_OPAQUE_MAP(Template, name, size, align)
//
// Register a two-type-parameter template with an opaque signature
// that includes both key and value type signatures.
//   Template -- template name without <K,V> (e.g., MyLib::XMap)
//   name     -- short display name (e.g., "map")
//   size     -- sizeof(Template<K,V>) (must be constant for all K,V)
//   align    -- alignof(Template<K,V>)
//
// Example:
//   TYPELAYOUT_OPAQUE_MAP(MyLib::XMap, "map", 32, 8)
//   // generates: O!map[s:32,a:8]<key_signature,value_signature>
// ---------------------------------------------------------------------------
#define TYPELAYOUT_OPAQUE_MAP(Template, name, size, align)                     \
    template <typename K_, typename V_>                                         \
    struct TypeSignature<Template<K_, V_>> {                                    \
        static constexpr bool is_opaque = true;                                \
        static constexpr bool pointer_free = false;                            \
        static consteval auto calculate() noexcept {                           \
            static_assert(sizeof(Template<K_, V_>) == (size),                   \
                "TYPELAYOUT_OPAQUE_MAP: size does not match "                   \
                "sizeof(" #Template "<K,V>)");                                  \
            static_assert(alignof(Template<K_, V_>) == (align),                 \
                "TYPELAYOUT_OPAQUE_MAP: align does not match "                  \
                "alignof(" #Template "<K,V>)");                                 \
            return ::boost::typelayout::FixedString{                           \
                       "O!" name "[s:" #size ",a:" #align "]<"} +                   \
                   TypeSignature<K_>::calculate() +                            \
                   ::boost::typelayout::FixedString{","} +                     \
                   TypeSignature<V_>::calculate() +                            \
                   ::boost::typelayout::FixedString{">"};                      \
        }                                                                      \
    };

// ===========================================================================
// Auto-deducing variants — TYPELAYOUT_OPAQUE_*_AUTO
//
// These macros deduce sizeof/alignof from the type itself, eliminating the
// need for the caller to provide numeric size/align literals.
//
// They use to_fixed_string() (from fixed_string.hpp) to convert the
// compile-time sizeof/alignof values into FixedString characters inside
// the consteval calculate() body, avoiding the preprocessor # operator
// which would stringify the expression text rather than its value.
//
// The original TYPELAYOUT_OPAQUE_* macros are preserved for backward
// compatibility and for cases where the caller wants to assert a specific
// size/align value.
// ===========================================================================

// ---------------------------------------------------------------------------
// TYPELAYOUT_OPAQUE_TYPE_AUTO(Type, name)
//
// Like TYPELAYOUT_OPAQUE_TYPE but deduces size/align from sizeof/alignof.
//   Type  -- fully qualified type name
//   name  -- short display name
//
// Example:
//   TYPELAYOUT_OPAQUE_TYPE_AUTO(MyLib::XString, "string")
//   // generates: O!string[s:32,a:8]  (assuming sizeof=32, alignof=8)
// ---------------------------------------------------------------------------
#define TYPELAYOUT_OPAQUE_TYPE_AUTO(Type, name)                                \
    template <>                                                                 \
    struct TypeSignature<Type> {                                                \
        static constexpr bool is_opaque = true;                                \
        static constexpr bool pointer_free = false;                            \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::FixedString{"O!" name "[s:"} +              \
                   ::boost::typelayout::to_fixed_string(sizeof(Type)) +        \
                   ::boost::typelayout::FixedString{",a:"} +                   \
                   ::boost::typelayout::to_fixed_string(alignof(Type)) +       \
                   ::boost::typelayout::FixedString{"]"};                      \
        }                                                                      \
    };

// ---------------------------------------------------------------------------
// TYPELAYOUT_OPAQUE_CONTAINER_AUTO(Template, name)
//
// Like TYPELAYOUT_OPAQUE_CONTAINER but deduces size/align automatically.
// Uses sizeof/alignof(Template<T_>) inside the consteval body.
//
// Example:
//   TYPELAYOUT_OPAQUE_CONTAINER_AUTO(MyLib::XVector, "vector")
//   // generates: O!vector[s:32,a:8]<element_signature>
// ---------------------------------------------------------------------------
#define TYPELAYOUT_OPAQUE_CONTAINER_AUTO(Template, name)                       \
    template <typename T_>                                                      \
    struct TypeSignature<Template<T_>> {                                        \
        static constexpr bool is_opaque = true;                                \
        static constexpr bool pointer_free = false;                            \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::FixedString{"O!" name "[s:"} +              \
                   ::boost::typelayout::to_fixed_string(                       \
                       sizeof(Template<T_>)) +                                 \
                   ::boost::typelayout::FixedString{",a:"} +                   \
                   ::boost::typelayout::to_fixed_string(                       \
                       alignof(Template<T_>)) +                                \
                   ::boost::typelayout::FixedString{"]<"} +                    \
                   TypeSignature<T_>::calculate() +                            \
                   ::boost::typelayout::FixedString{">"};                      \
        }                                                                      \
    };

// ---------------------------------------------------------------------------
// TYPELAYOUT_OPAQUE_MAP_AUTO(Template, name)
//
// Like TYPELAYOUT_OPAQUE_MAP but deduces size/align automatically.
// Uses sizeof/alignof(Template<K_, V_>) inside the consteval body.
//
// Example:
//   TYPELAYOUT_OPAQUE_MAP_AUTO(MyLib::XMap, "map")
//   // generates: O!map[s:32,a:8]<key_signature,value_signature>
// ---------------------------------------------------------------------------
#define TYPELAYOUT_OPAQUE_MAP_AUTO(Template, name)                             \
    template <typename K_, typename V_>                                         \
    struct TypeSignature<Template<K_, V_>> {                                    \
        static constexpr bool is_opaque = true;                                \
        static constexpr bool pointer_free = false;                            \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::FixedString{"O!" name "[s:"} +              \
                   ::boost::typelayout::to_fixed_string(                       \
                       sizeof(Template<K_, V_>)) +                             \
                   ::boost::typelayout::FixedString{",a:"} +                   \
                   ::boost::typelayout::to_fixed_string(                       \
                       alignof(Template<K_, V_>)) +                            \
                   ::boost::typelayout::FixedString{"]<"} +                    \
                   TypeSignature<K_>::calculate() +                            \
                   ::boost::typelayout::FixedString{","} +                     \
                   TypeSignature<V_>::calculate() +                            \
                   ::boost::typelayout::FixedString{">"};                      \
        }                                                                      \
    };

// ===========================================================================
// TYPELAYOUT_REGISTER_OPAQUE -- Serialization-Free Opaque Registration
//
// Registers an opaque type whose internal layout is invisible to
// TypeLayout.  The user provides a semantic tag string and declares
// whether the type contains pointers.
//
// sizeof and alignof are deduced automatically from the type.
//
// Signature format:
//   O(Tag|size|alignment)
//
// Parameters:
//   Type        -- fully qualified type name
//   Tag         -- semantic tag string (must be globally unique across
//                  all registered opaque types)
//   HasPointer  -- bool, user assertion: true if the type contains
//                  pointers.  If true, has_pointer will be true,
//                  preventing serialization-free transfer.
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
                   ::boost::typelayout::to_fixed_string(sizeof(Type)) +        \
                   ::boost::typelayout::FixedString{"|"} +                     \
                   ::boost::typelayout::to_fixed_string(alignof(Type)) +       \
                   ::boost::typelayout::FixedString{")"};                      \
        }                                                                      \
    };

#endif // BOOST_TYPELAYOUT_OPAQUE_HPP