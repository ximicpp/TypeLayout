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
// Relocatable variants — no trivially_copyable assertion.
//
// For types that are not trivially_copyable in the C++ sense but are still
// byte-copy safe under a relocation model (e.g. offset_ptr-based containers).
// The caller takes responsibility for the byte-copy safety guarantee.
// ===========================================================================

// TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE(Type, name)
//   Concrete type, pointer_free = true (no element type to scan).
//   Relocatable semantics inherently exclude native pointers — types using
//   offset_ptr are byte-copy safe and contain no address-space dependencies.
#define TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE(Type, name)                         \
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

// TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(Template, name)
//   Single-parameter container template.  Embeds element type signature.
//   pointer_free is derived from the generated signature (scans for all
//   pointer-like tokens, matching layout_traits sig_has_pointer).
#define TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(Template, name)                \
    template <typename T_>                                                      \
    struct TypeSignature<Template<T_>> {                                        \
        static constexpr bool is_opaque = true;                                \
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
        static constexpr bool pointer_free =                                   \
            !calculate().contains_token(                                       \
                ::boost::typelayout::FixedString{"ptr["}) &&                   \
            !calculate().contains_token(                                       \
                ::boost::typelayout::FixedString{"fnptr["}) &&                 \
            !calculate().contains_token(                                       \
                ::boost::typelayout::FixedString{"memptr["}) &&                \
            !calculate().contains_token(                                       \
                ::boost::typelayout::FixedString{"ref["}) &&                   \
            !calculate().contains_token(                                       \
                ::boost::typelayout::FixedString{"rref["});                    \
    };

// TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(Template, name)
//   Two-parameter container template.  Embeds key + value type signatures.
//   pointer_free is derived from the generated signature (scans for all
//   pointer-like tokens, matching layout_traits sig_has_pointer).
#define TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(Template, name)                      \
    template <typename K_, typename V_>                                         \
    struct TypeSignature<Template<K_, V_>> {                                    \
        static constexpr bool is_opaque = true;                                \
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
        static constexpr bool pointer_free =                                   \
            !calculate().contains_token(                                       \
                ::boost::typelayout::FixedString{"ptr["}) &&                   \
            !calculate().contains_token(                                       \
                ::boost::typelayout::FixedString{"fnptr["}) &&                 \
            !calculate().contains_token(                                       \
                ::boost::typelayout::FixedString{"memptr["}) &&                \
            !calculate().contains_token(                                       \
                ::boost::typelayout::FixedString{"ref["}) &&                   \
            !calculate().contains_token(                                       \
                ::boost::typelayout::FixedString{"rref["});                    \
    };

#endif // BOOST_TYPELAYOUT_OPAQUE_HPP
