// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
//
// P2996 reflection helpers. Requires a P2996-capable compiler
// (GCC 16+ or Bloomberg Clang fork).

#ifndef BOOST_TYPELAYOUT_DETAIL_REFLECT_HPP
#define BOOST_TYPELAYOUT_DETAIL_REFLECT_HPP

#include <boost/typelayout/fixed_string.hpp>
#if __has_include(<meta>)
    #include <meta>
#else
    #include <experimental/meta>
#endif
#include <type_traits>

namespace boost {
namespace typelayout {
inline namespace v1 {
namespace detail {

    template <typename T>
    consteval std::size_t get_member_count() noexcept {
        return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()).size();
    }

    template <typename T>
    consteval std::size_t get_base_count() noexcept {
        return std::meta::bases_of(^^T, std::meta::access_context::unchecked()).size();
    }

    // Recursively check for virtual inheritance in T's base hierarchy.
    template <typename T>
    consteval bool has_virtual_base() noexcept;

    template <typename T, std::size_t I, std::size_t N>
    consteval bool any_base_is_virtual() noexcept {
        if constexpr (I >= N) return false;
        else {
            constexpr auto base_info =
                std::meta::bases_of(^^T, std::meta::access_context::unchecked())[I];
            using BaseType = [:std::meta::type_of(base_info):];
            if constexpr (std::meta::is_virtual(base_info)) return true;
            else if constexpr (has_virtual_base<BaseType>()) return true;
            else return any_base_is_virtual<T, I + 1, N>();
        }
    }

    template <typename T>
    consteval bool has_virtual_base() noexcept {
        if constexpr (!std::is_class_v<T>) return false;
        else return any_base_is_virtual<T, 0, get_base_count<T>()>();
    }

} // namespace detail
} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_DETAIL_REFLECT_HPP
