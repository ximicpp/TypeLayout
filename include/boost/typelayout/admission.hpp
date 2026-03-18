// admission.hpp -- Byte-copy safety admission predicate.
//
// is_byte_copy_safe_v<T> determines whether type T is safe for byte-level
// transport (memcpy to a buffer, send over network, write to shared memory).
//
// The predicate recursively checks each member and base class:
//   - Trivially copyable types without pointers: safe
//   - Registered relocatable opaque types with safe elements: safe
//   - Types with pointers, references, or unregistered opaque members: unsafe
//
// Relationship to compat::SafetyLevel (tools/safety_level.hpp):
//   SafetyLevel and is_byte_copy_safe are orthogonal dimensions:
//   - SafetyLevel measures "how much external trust is required" (display-only).
//   - is_byte_copy_safe measures "given that trust, is the type safe".
//   A type can be Opaque AND is_byte_copy_safe == true: the safety level
//   describes analyzability, is_byte_copy_safe describes the conclusion.
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

namespace detail {

// Forward declaration for mutual recursion.
template <typename T>
consteval bool is_byte_copy_safe_impl() noexcept;

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

// Core decision tree for byte-copy safety.
//
// Branch 1: Opaque types -- check !has_pointer && opaque_elements_safe
// Branch 2: Locally serialization-free types -- trivially_copyable + no pointer
// Branch 3: Non-union, non-polymorphic class types -- recurse members + bases
// Branch 4: Everything else -- false
template <typename T>
consteval bool is_byte_copy_safe_impl() noexcept {
    using Bare = std::remove_cv_t<T>;

    // Branch 1: Opaque types
    if constexpr (has_opaque_signature<Bare>) {
        return !layout_traits<Bare>::has_pointer &&
               opaque_elements_safe<Bare>::value;
    }
    // Branch 2: Locally serialization-free (trivially_copyable + no pointer)
    else if constexpr (std::is_trivially_copyable_v<Bare> &&
                       !layout_traits<Bare>::has_pointer) {
        return true;
    }
    // Branch 3: Non-union, non-polymorphic class -- recurse
    else if constexpr (std::is_class_v<Bare> &&
                       !std::is_union_v<Bare> &&
                       !std::is_polymorphic_v<Bare>) {
        constexpr std::size_t bc = get_base_count<Bare>();
        constexpr std::size_t fc = get_member_count<Bare>();
        return all_bases_byte_copy_safe<Bare, 0, bc>() &&
               all_members_byte_copy_safe<Bare, 0, fc>();
    }
    // Branch 4: Otherwise not safe
    else {
        return false;
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

// Array specialization: recurse into element type.
template <typename T, std::size_t N>
struct is_byte_copy_safe<T[N]>
    : std::bool_constant<detail::is_byte_copy_safe_impl<T>()> {};

// Convenience variable template.
template <typename T>
inline constexpr bool is_byte_copy_safe_v = is_byte_copy_safe<T>::value;

} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_ADMISSION_HPP