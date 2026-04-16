// Opaque type registration macro.
//
// Opaque Tag Matching Assumption:
//   When two endpoints compare opaque type signatures for cross-platform
//   transfer safety, matching is based on:
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

namespace boost {
namespace typelayout {
inline namespace v1 {

namespace detail {

template <std::size_t Size, std::size_t Align, typename Tag>
[[nodiscard]] consteval auto opaque_signature(const Tag& tag) noexcept {
    return FixedString{"O("} + FixedString{tag} + FixedString{"|"} +
           to_fixed_string<Size>() + FixedString{"|"} +
           to_fixed_string<Align>() + FixedString{")"};
}

template <std::size_t Size, std::size_t Align, typename Elem, typename Tag>
[[nodiscard]] consteval auto opaque_container_signature(const Tag& tag) noexcept {
    return opaque_signature<Size, Align>(tag) + FixedString{"<"} +
           TypeSignature<Elem>::calculate() + FixedString{">"};
}

template <std::size_t Size, std::size_t Align, typename Key, typename Value, typename Tag>
[[nodiscard]] consteval auto opaque_map_signature(const Tag& tag) noexcept {
    return opaque_signature<Size, Align>(tag) + FixedString{"<"} +
           TypeSignature<Key>::calculate() + FixedString{","} +
           TypeSignature<Value>::calculate() + FixedString{">"};
}

template <typename Sig>
[[nodiscard]] consteval bool signature_pointer_free(const Sig& sig) noexcept {
    return !sig_has_pointer(sig);
}

} // namespace detail

} // inline namespace v1
} // namespace typelayout
} // namespace boost

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
            return ::boost::typelayout::detail::opaque_signature<              \
                sizeof(Type), alignof(Type)>(Tag);                             \
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
            return ::boost::typelayout::detail::opaque_signature<              \
                sizeof(Type), alignof(Type)>(name);                            \
        }                                                                      \
    };                                                                         \
    template <>                                                                 \
    struct opaque_copy_safe<Type> : std::true_type {};

// TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(Template, name)
//   Single-parameter container template.  Embeds element type signature.
//   pointer_free is derived from the generated signature (scans for all
//   pointer-like tokens via detail::sig_has_pointer).
#define TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(Template, name)                \
    template <typename T_>                                                      \
    struct TypeSignature<Template<T_>> {                                        \
        static constexpr bool is_opaque = true;                                \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::detail::opaque_container_signature<    \
                sizeof(Template<T_>), alignof(Template<T_>), T_>(name);        \
        }                                                                      \
        static constexpr bool pointer_free =                                   \
            ::boost::typelayout::detail::signature_pointer_free(calculate());  \
    };                                                                         \
    template <typename T_>                                                      \
    struct opaque_copy_safe<Template<T_>>                                   \
        : std::bool_constant<                                                  \
              ::boost::typelayout::is_byte_copy_safe_v<T_>> {};

// TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(Template, name)
//   Two-parameter container template.  Embeds key + value type signatures.
//   pointer_free is derived from the generated signature (scans for all
//   pointer-like tokens via detail::sig_has_pointer).
#define TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(Template, name)                      \
    template <typename K_, typename V_>                                         \
    struct TypeSignature<Template<K_, V_>> {                                    \
        static constexpr bool is_opaque = true;                                \
        static consteval auto calculate() noexcept {                           \
            return ::boost::typelayout::detail::opaque_map_signature<          \
                sizeof(Template<K_, V_>), alignof(Template<K_, V_>),           \
                K_, V_>(name);                                                 \
        }                                                                      \
        static constexpr bool pointer_free =                                   \
            ::boost::typelayout::detail::signature_pointer_free(calculate());  \
    };                                                                         \
    template <typename K_, typename V_>                                         \
    struct opaque_copy_safe<Template<K_, V_>>                               \
        : std::bool_constant<                                                  \
              ::boost::typelayout::is_byte_copy_safe_v<K_> &&                  \
              ::boost::typelayout::is_byte_copy_safe_v<V_>> {};

#endif // BOOST_TYPELAYOUT_OPAQUE_HPP
