// admission.hpp -- Byte-copy safety admission predicate.
//
// is_byte_copy_safe_v<T> determines whether type T is safe for byte-level
// transport (memcpy to a buffer, send over network, write to shared memory).
//
// Safety is determined by scanning the Layout Signature for pointer tokens
// (ptr, fnptr, memptr, ref, rref, vptr) plus recursive member checking.
// Two cases the token scan alone cannot decide are handled explicitly:
// polymorphic types (hidden vptr) are rejected outright, and opaque
// members seal their internal layout, so recursion consults their
// registered pointer_free flag instead of the parent's token scan.
//
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_ADMISSION_HPP
#define BOOST_TYPELAYOUT_ADMISSION_HPP

#include <boost/typelayout/layout_traits.hpp>
#include <type_traits>

namespace boost {
namespace typelayout {
inline namespace v1 {

enum class SourceContext {
    independent,
    same_region,
    address_space_dependent
};

enum class TransferProfile {
    ordinary_copy,
    whole_region_relocation
};

constexpr SourceContext join_source_context(
    SourceContext lhs, SourceContext rhs) noexcept {
    if (lhs == SourceContext::address_space_dependent ||
        rhs == SourceContext::address_space_dependent) {
        return SourceContext::address_space_dependent;
    }
    if (lhs == SourceContext::same_region ||
        rhs == SourceContext::same_region) {
        return SourceContext::same_region;
    }
    return SourceContext::independent;
}

template <typename T>
struct source_context_traits;

namespace detail {

// Forward declaration for mutual recursion.
template <typename T>
consteval bool is_byte_copy_safe_impl() noexcept;

template <typename T>
consteval SourceContext source_context_impl() noexcept;

// Check whether all non-static data members of T are byte-copy safe.
template <typename T, std::size_t I, std::size_t N>
consteval bool all_members_byte_copy_safe() noexcept {
    if constexpr (I >= N) {
        return true;
    } else {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[I];
        using FieldType = [:type_of(member):];
        if constexpr (!is_byte_copy_safe_impl<FieldType>()) {
            return false;
        } else {
            return all_members_byte_copy_safe<T, I + 1, N>();
        }
    }
}

// Check whether all base classes of T are byte-copy safe.
template <typename T, std::size_t I, std::size_t N>
consteval bool all_bases_byte_copy_safe() noexcept {
    if constexpr (I >= N) {
        return true;
    } else {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[I];
        using BaseType = [:type_of(base_info):];
        if constexpr (!is_byte_copy_safe_impl<BaseType>()) {
            return false;
        } else {
            return all_bases_byte_copy_safe<T, I + 1, N>();
        }
    }
}

template <typename T, std::size_t I, std::size_t N>
consteval SourceContext joined_member_source_context() noexcept {
    if constexpr (I >= N) {
        return SourceContext::independent;
    } else {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[I];
        using FieldType = [:type_of(member):];
        return join_source_context(
            source_context_traits<std::remove_cv_t<FieldType>>::value,
            joined_member_source_context<T, I + 1, N>());
    }
}

template <typename T, std::size_t I, std::size_t N>
consteval SourceContext joined_base_source_context() noexcept {
    if constexpr (I >= N) {
        return SourceContext::independent;
    } else {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[I];
        using BaseType = [:type_of(base_info):];
        return join_source_context(
            source_context_traits<std::remove_cv_t<BaseType>>::value,
            joined_base_source_context<T, I + 1, N>());
    }
}

// Core decision tree for byte-copy safety.
//
// Branch 1: Opaque types -- check !has_pointer && opaque_copy_safe
// Branch 2: trivially_copyable + no pointer + no opaque member (fast path)
// Branch 3: Array -- recurse into element type
// Branch 4: Class or union -- reject polymorphic, else recurse members + bases
// Branch 5: Everything else -- false
template <typename T>
consteval bool is_byte_copy_safe_impl() noexcept {
    using Bare = std::remove_cv_t<T>;

    // Branch 1: Opaque types
    if constexpr (has_opaque_signature<Bare>) {
        return detail::is_pointer_free_layout<Bare>() &&
               opaque_copy_safe<Bare>::value;
    }
    // Branch 2: trivially_copyable + no pointer (fast path)
    // An embedded opaque member shows as O(Tag|N|A) with its pointers
    // sealed from the token scan, so signatures containing one must fall
    // through to member recursion, which checks the registered flags.
    else if constexpr (std::is_trivially_copyable_v<Bare> &&
                       detail::is_pointer_free_layout<Bare>() &&
                       !sig_has_opaque(get_layout_signature<Bare>())) {
        return true;
    }
    // Branch 3: Array -- recurse into element type
    else if constexpr (std::is_array_v<Bare>) {
        return is_byte_copy_safe_impl<std::remove_extent_t<Bare>>();
    }
    // Branch 4: Class or union -- recurse members (and bases for classes)
    // Polymorphic types are rejected here: Branch 2 is accept-only, so a
    // polymorphic type (never trivially copyable) would otherwise fall
    // through to member recursion, which never sees the hidden vptr.
    else if constexpr (std::is_class_v<Bare> || std::is_union_v<Bare>) {
        if constexpr (std::is_polymorphic_v<Bare>) {
            return false;
        } else {
            constexpr std::size_t bc = std::is_union_v<Bare> ? 0 : get_base_count<Bare>();
            constexpr std::size_t fc = get_member_count<Bare>();
            return all_bases_byte_copy_safe<Bare, 0, bc>() &&
                   all_members_byte_copy_safe<Bare, 0, fc>();
        }
    }
    // Branch 5: Otherwise not safe (e.g. bare function types)
    else {
        return false;
    }
}

template <typename T>
consteval SourceContext source_context_impl() noexcept {
    using Bare = std::remove_cv_t<T>;

    if constexpr (std::is_pointer_v<Bare> ||
                  std::is_reference_v<Bare> ||
                  std::is_member_pointer_v<Bare>) {
        return SourceContext::address_space_dependent;
    } else if constexpr (std::is_polymorphic_v<Bare>) {
        return SourceContext::address_space_dependent;
    } else if constexpr (std::is_array_v<Bare>) {
        return source_context_traits<
            std::remove_cv_t<std::remove_extent_t<Bare>>>::value;
    } else if constexpr (std::is_class_v<Bare> || std::is_union_v<Bare>) {
        constexpr std::size_t bc = std::is_union_v<Bare> ? 0 : get_base_count<Bare>();
        constexpr std::size_t fc = get_member_count<Bare>();
        return join_source_context(
            joined_base_source_context<Bare, 0, bc>(),
            joined_member_source_context<Bare, 0, fc>());
    } else {
        return SourceContext::independent;
    }
}

} // namespace detail

// is_byte_copy_safe<T> -- compile-time predicate struct.
//
// IMPORTANT: "byte-copy safe" means safe for byte-level TRANSPORT (memcpy
// to a buffer, send over network, write to shared memory), NOT safe for
// C++ object lifetime.  A type marked is_byte_copy_safe may have non-trivial
// constructors/destructors (e.g. relocatable opaque types using offset_ptr).
// The receiving end must reconstruct the C++ object appropriately -- do NOT
// memcpy into a live C++ object of non-trivially-copyable type and then
// call member functions on it.
//
// For types where memcpy produces a valid C++ object (trivially_copyable),
// check std::is_trivially_copyable_v<T> && is_byte_copy_safe_v<T>.
template <typename T>
struct is_byte_copy_safe
    : std::bool_constant<detail::is_byte_copy_safe_impl<T>()> {};

// Convenience variable template.
template <typename T>
inline constexpr bool is_byte_copy_safe_v = is_byte_copy_safe<T>::value;

template <typename T>
struct source_context_traits
    : std::integral_constant<SourceContext,
          detail::source_context_impl<T>()> {};

template <typename T>
inline constexpr SourceContext source_context_v =
    source_context_traits<std::remove_cv_t<T>>::value;

template <typename T>
struct region_relocation_traits {
    static constexpr bool enabled =
        std::is_trivially_copyable_v<std::remove_cv_t<T>>;
};

namespace detail {

template <typename T, TransferProfile Profile>
consteval bool is_admitted_impl() {
    using Bare = std::remove_cv_t<T>;
    if constexpr (Profile == TransferProfile::ordinary_copy) {
        return std::is_trivially_copyable_v<Bare> &&
               is_byte_copy_safe_v<Bare> &&
               source_context_v<Bare> == SourceContext::independent;
    } else {
        return is_byte_copy_safe_v<Bare> &&
               region_relocation_traits<Bare>::enabled &&
               source_context_v<Bare> !=
                   SourceContext::address_space_dependent;
    }
}

} // namespace detail

template <typename T, TransferProfile Profile>
inline constexpr bool is_admitted_v =
    detail::is_admitted_impl<T, Profile>();

} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_ADMISSION_HPP
