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
    // TODO(P2996): Replace with std::meta::qualified_name_of when available.

    template<std::meta::info R>
    consteval auto qualified_name_for() noexcept {
        using namespace std::meta;
        constexpr auto parent = parent_of(R);
        constexpr std::string_view self = identifier_of(R);
        if constexpr (is_namespace(parent) && has_identifier(parent)) {
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

    /// Check whether an enum type has a fixed underlying type and is thus
    /// trivially portable across processes on the same architecture.
    /// Scoped enums (enum class) always have a fixed underlying type.
    /// Unscoped enums have a fixed type only when explicitly specified.
    ///
    /// NOTE: C++ provides no API to distinguish between an explicitly specified
    /// underlying type (`enum E : int`) and a compiler-inferred one (`enum E {A}`).
    /// std::underlying_type_t resolves to a concrete type in both cases.
    /// This function therefore returns true for ALL enums with an integral
    /// underlying type (excluding bool).  For unscoped enums without an explicit
    /// underlying type, the result is a best-effort approximation -- the user
    /// should ensure that cross-platform enums use explicit `: type` specifiers.
    template <typename T>
    [[nodiscard]] consteval bool is_fixed_enum() noexcept {
        static_assert(std::is_enum_v<T>, "is_fixed_enum requires an enum type");
        using U = std::underlying_type_t<T>;
        return std::is_integral_v<U> && !std::is_same_v<U, bool>;
    }

    template<std::meta::info Member, std::size_t Index>
    consteval auto get_member_name() noexcept {
        using namespace std::meta;
        if constexpr (has_identifier(Member)) {
            constexpr std::string_view name = identifier_of(Member);
            return FixedString<name.size()>(name);
        } else {
            return FixedString{"<anon:"} +
                   to_fixed_string(Index) +
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

    // Check whether T has at least one direct polymorphic base class.
    // Used to determine if T itself introduces the vptr (as opposed to
    // inheriting it from a base).
    template<typename T, std::size_t I>
    consteval bool base_is_polymorphic() noexcept {
        constexpr auto base_info = std::meta::bases_of(^^T, std::meta::access_context::unchecked())[I];
        using Base = [:std::meta::type_of(base_info):];
        return std::is_polymorphic_v<Base>;
    }

    template<typename T, std::size_t... Is>
    consteval bool any_base_polymorphic(std::index_sequence<Is...>) noexcept {
        return (... || base_is_polymorphic<T, Is>());
    }

    template<typename T>
    consteval bool has_polymorphic_base() noexcept {
        constexpr std::size_t bc = get_base_count<T>();
        if constexpr (bc == 0) return false;
        else return any_base_polymorphic<T>(std::make_index_sequence<bc>{});
    }

    // T introduces vptr if T is polymorphic and no direct base is polymorphic.
    template<typename T>
    consteval bool introduces_vptr() noexcept {
        return std::is_polymorphic_v<T> && !has_polymorphic_base<T>();
    }

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_DETAIL_REFLECT_HPP
