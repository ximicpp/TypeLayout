// Opaque type registration macro.
//
// Opaque Tag Matching Assumption:
//   When two endpoints compare opaque type signatures for cross-platform
//   transfer safety (via is_transfer_safe), matching is based on:
//     - tag name (e.g. "vector", "string")
//     - sizeof
//     - alignof
//     - element type signature (for container/map templates)
//
//   This assumes that types registered with the same tag, sizeof, and alignof
//   have identical internal binary layout. This is the user's responsibility
//   to guarantee -- typically ensured by using the same library version
//   (e.g. Boost.Interprocess) and the same compiler ABI on both endpoints.
//
//   TypeLayout does NOT verify internal field layout of opaque types.
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
    };                                                                         \
    template <>                                                                 \
    struct opaque_copy_safe<Type> : std::true_type {};

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
    };                                                                         \
    template <>                                                                 \
    struct opaque_copy_safe<Type> : std::true_type {};

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
            !::boost::typelayout::detail::sig_has_pointer(calculate());        \
    };                                                                         \
    template <typename T_>                                                      \
    struct opaque_copy_safe<Template<T_>>                                   \
        : std::bool_constant<                                                  \
              ::boost::typelayout::is_byte_copy_safe_v<T_>> {};

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
            !::boost::typelayout::detail::sig_has_pointer(calculate());        \
    };                                                                         \
    template <typename K_, typename V_>                                         \
    struct opaque_copy_safe<Template<K_, V_>>                               \
        : std::bool_constant<                                                  \
              ::boost::typelayout::is_byte_copy_safe_v<K_> &&                  \
              ::boost::typelayout::is_byte_copy_safe_v<V_>> {};

#endif // BOOST_TYPELAYOUT_OPAQUE_HPP
