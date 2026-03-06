// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
//
// P2996 reflection meta-operations used by the signature engines.
// Requires the Bloomberg P2996 experimental toolchain.

#ifndef BOOST_TYPELAYOUT_DETAIL_REFLECT_HPP
#define BOOST_TYPELAYOUT_DETAIL_REFLECT_HPP

#include <boost/typelayout/fixed_string.hpp>
#include <experimental/meta>
#include <type_traits>

namespace boost {
namespace typelayout {

    // =========================================================================
    // P2996 Reflection Meta-Operations
    // =========================================================================

    // Qualified name builder -- P2996 Bloomberg toolchain lacks
    // qualified_name_of, so we walk parent_of chains and join with "::".
    // Handles both namespace parents and nested class parents.
    // Recursion stops when the parent has no identifier (global namespace,
    // translation unit, or anonymous namespace).
    // TODO(P2996): Replace with std::meta::qualified_name_of when available.

    template<std::meta::info R>
    consteval auto qualified_name_for() noexcept {
        using namespace std::meta;
        constexpr auto parent = parent_of(R);
        constexpr std::string_view self = identifier_of(R);
        if constexpr (has_identifier(parent)) {
            return qualified_name_for<parent>() +
                   FixedString{"::"} +
                   FixedString<self.size()>(self);
        } else {
            return FixedString<self.size()>(self);
        }
    }

    template <typename T>
    consteval std::size_t get_member_count() noexcept {
        return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()).size();
    }

    template <typename T>
    consteval std::size_t get_base_count() noexcept {
        return std::meta::bases_of(^^T, std::meta::access_context::unchecked()).size();
    }

    template<std::meta::info Member, std::size_t Index>
    consteval auto get_member_name() noexcept {
        using namespace std::meta;
        if constexpr (has_identifier(Member)) {
            constexpr std::string_view name = identifier_of(Member);
            return FixedString<name.size()>(name);
        } else {
            return FixedString{"<anon:"} +
                   to_fixed_string<Index>() +
                   FixedString{">"};
        }
    }

    template<std::meta::info BaseInfo>
    consteval auto get_base_name() noexcept {
        return qualified_name_for<std::meta::type_of(BaseInfo)>();
    }

    template<typename T>
    consteval auto get_type_qualified_name() noexcept {
        return qualified_name_for<^^T>();
    }

    // Recursively check whether T (or any of its base classes) uses
    // virtual inheritance.  Virtual bases introduce hidden vbptrs whose
    // layout is compiler-specific, and diamond inheritance causes the
    // flattening engine to double-count the shared virtual base.
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

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_DETAIL_REFLECT_HPP
