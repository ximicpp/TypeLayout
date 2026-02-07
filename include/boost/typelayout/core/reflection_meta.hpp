// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_REFLECTION_META_HPP
#define BOOST_TYPELAYOUT_CORE_REFLECTION_META_HPP

#include <boost/typelayout/core/config.hpp>
#include <boost/typelayout/core/compile_string.hpp>
#include <experimental/meta>

namespace boost {
namespace typelayout {

    // Forward declaration — defined in type_signature.hpp
    template <typename T, SignatureMode Mode = SignatureMode::Layout>
    struct TypeSignature;

    // --- Qualified name builder ---
    //
    // P2996 Bloomberg toolchain lacks qualified_name_of, so we walk
    // parent_of chains and join with "::".

    template<std::meta::info R>
    consteval auto qualified_name_for() noexcept {
        using namespace std::meta;
        constexpr auto parent = parent_of(R);
        constexpr std::string_view self = identifier_of(R);
        if constexpr (is_namespace(parent) && has_identifier(parent)) {
            constexpr std::string_view pname = identifier_of(parent);
            // Check grandparent to recurse further
            constexpr auto grandparent = parent_of(parent);
            if constexpr (is_namespace(grandparent) && has_identifier(grandparent)) {
                return qualified_name_for<parent>() +
                       CompileString{"::"} +
                       CompileString<self.size() + 1>(self);
            } else {
                return CompileString<pname.size() + 1>(pname) +
                       CompileString{"::"} +
                       CompileString<self.size() + 1>(self);
            }
        } else {
            return CompileString<self.size() + 1>(self);
        }
    }

    // --- Basic reflection helpers ---

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
            constexpr size_t NameLen = name.size() + 1;
            return CompileString<NameLen>(name);
        } else {
            return CompileString{"<anon:"} +
                   CompileString<number_buffer_size>::from_number(Index) +
                   CompileString{">"};
        }
    }

    template<std::meta::info BaseInfo>
    consteval auto get_base_name() noexcept {
        return qualified_name_for<std::meta::type_of(BaseInfo)>();
    }

    // Qualified name for a type T (used for enums in Definition mode)
    template<typename T>
    consteval auto get_type_qualified_name() noexcept {
        return qualified_name_for<^^T>();
    }

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_REFLECTION_META_HPP
