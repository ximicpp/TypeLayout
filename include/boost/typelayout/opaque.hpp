// Opaque type registration macro.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_OPAQUE_HPP
#define BOOST_TYPELAYOUT_OPAQUE_HPP

#include <boost/typelayout/fixed_string.hpp>

// TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, HasPointer)
//   Tag  -- globally unique string identifier
//   HasPointer -- true if the type contains pointers
//   Type must be trivially_copyable (enforced by static_assert).
//   Signature format: O(Tag|size|alignment)
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

// ===========================================================================
// Auto-deducing variants — TYPELAYOUT_OPAQUE_*_AUTO
//
// These macros deduce sizeof/alignof from the type itself, eliminating the
// need for the caller to provide numeric size/align literals.  The generated
// signature uses the same O(Tag|size|align) format as TYPELAYOUT_REGISTER_OPAQUE.
//
// All AUTO macros set pointer_free = true by default, which is correct for
// types that use offset_ptr (relative addressing) rather than raw pointers.
// ===========================================================================

// ---------------------------------------------------------------------------
// TYPELAYOUT_OPAQUE_TYPE_AUTO(Type, name)
//
// Auto-deducing opaque registration for a concrete type.
//   Type  -- fully qualified type name
//   name  -- short display name (used as the opaque tag)
//
// Example:
//   TYPELAYOUT_OPAQUE_TYPE_AUTO(MyLib::XString, "string")
//   // generates: O(string|32|8)  (assuming sizeof=32, alignof=8)
// ---------------------------------------------------------------------------
#define TYPELAYOUT_OPAQUE_TYPE_AUTO(Type, name)                                \
    static_assert(std::is_trivially_copyable_v<Type>,                           \
        "TYPELAYOUT_OPAQUE_TYPE_AUTO: opaque type must be trivially copyable"); \
    template <>                                                                 \
    struct TypeSignature<Type> {                                                \
        static constexpr bool is_opaque = true;                                \
        static constexpr bool pointer_free = true;                             \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::FixedString{"O("} +                    \
                   ::boost::typelayout::FixedString{name} +                    \
                   ::boost::typelayout::FixedString{"|"} +                     \
                   ::boost::typelayout::to_fixed_string<sizeof(Type)>() +      \
                   ::boost::typelayout::FixedString{"|"} +                     \
                   ::boost::typelayout::to_fixed_string<alignof(Type)>() +     \
                   ::boost::typelayout::FixedString{")"};                      \
        }                                                                      \
    };

// ---------------------------------------------------------------------------
// TYPELAYOUT_OPAQUE_CONTAINER_AUTO(Template, name)
//
// Auto-deducing opaque registration for a single-type-parameter container
// template.  The signature embeds the element type's signature for recursive
// safety analysis.
//
// Example:
//   TYPELAYOUT_OPAQUE_CONTAINER_AUTO(MyLib::XVector, "vector")
//   // generates: O(vector|32|8)<element_sig>
// ---------------------------------------------------------------------------
#define TYPELAYOUT_OPAQUE_CONTAINER_AUTO(Template, name)                       \
    template <typename T_>                                                      \
    struct TypeSignature<Template<T_>> {                                        \
        static constexpr bool is_opaque = true;                                \
        static constexpr bool pointer_free = true;                             \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::FixedString{"O("} +                    \
                   ::boost::typelayout::FixedString{name} +                    \
                   ::boost::typelayout::FixedString{"|"} +                     \
                   ::boost::typelayout::to_fixed_string<                       \
                       sizeof(Template<T_>)>() +                               \
                   ::boost::typelayout::FixedString{"|"} +                     \
                   ::boost::typelayout::to_fixed_string<                       \
                       alignof(Template<T_>)>() +                              \
                   ::boost::typelayout::FixedString{")<"} +                    \
                   TypeSignature<T_>::calculate() +                            \
                   ::boost::typelayout::FixedString{">"};                      \
        }                                                                      \
    };

// ---------------------------------------------------------------------------
// TYPELAYOUT_OPAQUE_MAP_AUTO(Template, name)
//
// Auto-deducing opaque registration for a two-type-parameter container
// template.  The signature embeds both key and value type signatures.
//
// Example:
//   TYPELAYOUT_OPAQUE_MAP_AUTO(MyLib::XMap, "map")
//   // generates: O(map|48|8)<key_sig,value_sig>
// ---------------------------------------------------------------------------
#define TYPELAYOUT_OPAQUE_MAP_AUTO(Template, name)                             \
    template <typename K_, typename V_>                                         \
    struct TypeSignature<Template<K_, V_>> {                                    \
        static constexpr bool is_opaque = true;                                \
        static constexpr bool pointer_free = true;                             \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::FixedString{"O("} +                    \
                   ::boost::typelayout::FixedString{name} +                    \
                   ::boost::typelayout::FixedString{"|"} +                     \
                   ::boost::typelayout::to_fixed_string<                       \
                       sizeof(Template<K_, V_>)>() +                           \
                   ::boost::typelayout::FixedString{"|"} +                     \
                   ::boost::typelayout::to_fixed_string<                       \
                       alignof(Template<K_, V_>)>() +                          \
                   ::boost::typelayout::FixedString{")<"} +                    \
                   TypeSignature<K_>::calculate() +                            \
                   ::boost::typelayout::FixedString{","} +                     \
                   TypeSignature<V_>::calculate() +                            \
                   ::boost::typelayout::FixedString{">"};                      \
        }                                                                      \
    };

#endif // BOOST_TYPELAYOUT_OPAQUE_HPP
