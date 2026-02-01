// Boost.TypeLayout
//
// P2996 Static Reflection Helpers (Core)
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_CORE_REFLECTION_HELPERS_HPP
#define BOOST_TYPELAYOUT_CORE_REFLECTION_HELPERS_HPP

#include <boost/typelayout/core/config.hpp>
#include <boost/typelayout/core/compile_string.hpp>
#include <experimental/meta>

namespace boost {
namespace typelayout {

    // Forward declaration
    template <typename T>
    struct TypeSignature;

    // =========================================================================
    // Reflection Helpers
    // =========================================================================

    // Get member count of a type
    template <typename T>
    consteval std::size_t get_member_count() noexcept {
        using namespace std::meta;
        auto all_members = nonstatic_data_members_of(^^T, access_context::unchecked());
        return all_members.size();
    }

    // Get base class count of a type
    template <typename T>
    consteval std::size_t get_base_count() noexcept {
        using namespace std::meta;
        auto bases = bases_of(^^T, access_context::unchecked());
        return bases.size();
    }

    // Check if a type has base classes
    template <typename T>
    consteval bool has_bases() noexcept {
        return get_base_count<T>() > 0;
    }

    // Get field offset at compile time
    template<typename T, size_t Index>
    consteval size_t get_field_offset() noexcept {
        using namespace std::meta;
        auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
        return offset_of(members[Index]).bytes;
    }

    // =========================================================================
    // Field Signature Generation
    // =========================================================================

    // Helper to generate field name or anonymous placeholder
    template<std::meta::info Member, std::size_t Index>
    static consteval auto get_member_name() noexcept {
        using namespace std::meta;
        if constexpr (has_identifier(Member)) {
            constexpr std::string_view name = identifier_of(Member);
            constexpr size_t NameLen = name.size() + 1;
            return CompileString<NameLen>(name);
        } else {
            return CompileString{"<anon:"} +
                   CompileString<16>::from_number(Index) +
                   CompileString{">"};
        }
    }

    // Build signature for a single field
    template<typename T, std::size_t Index>
    static consteval auto get_field_signature() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            constexpr std::size_t byte_offset = bit_off.bytes;
            constexpr std::size_t bit_offset = bit_off.bits;
            constexpr std::size_t bit_width = bit_size_of(member);
            
            return CompileString{"@"} +
                   CompileString<32>::from_number(byte_offset) +
                   CompileString{"."} +
                   CompileString<32>::from_number(bit_offset) +
                   CompileString{"["} +
                   get_member_name<member, Index>() +
                   CompileString{"]:bits<"} +
                   CompileString<32>::from_number(bit_width) +
                   CompileString{","} +
                   TypeSignature<FieldType>::calculate() +
                   CompileString{">"};
        } else {
            constexpr std::size_t offset = offset_of(member).bytes;
            
            return CompileString{"@"} +
                   CompileString<32>::from_number(offset) +
                   CompileString{"["} +
                   get_member_name<member, Index>() +
                   CompileString{"]:"} +
                   TypeSignature<FieldType>::calculate();
        }
    }

    // Build field signature with comma prefix (except first)
    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto build_field_with_comma() noexcept {
        if constexpr (IsFirst) {
            return get_field_signature<T, Index>();
        } else {
            return CompileString{","} + get_field_signature<T, Index>();
        }
    }

    // Concatenate all field signatures
    template<typename T, std::size_t... Indices>
    consteval auto concatenate_field_signatures(std::index_sequence<Indices...>) noexcept {
        return (build_field_with_comma<T, Indices, (Indices == 0)>() + ...);
    }

    // Get all fields signature for a struct/class
    template <typename T>
    consteval auto get_fields_signature() noexcept {
        constexpr std::size_t count = get_member_count<T>();
        if constexpr (count == 0) {
            return CompileString{""};
        } else {
            return concatenate_field_signatures<T>(std::make_index_sequence<count>{});
        }
    }

    // =========================================================================
    // Base Class Signature Generation
    // =========================================================================

    template<typename T, std::size_t Index>
    static consteval auto get_base_signature() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[Index];
        
        using BaseType = [:type_of(base_info):];
        constexpr std::size_t base_offset = offset_of(base_info).bytes;
        constexpr bool is_virtual_base = is_virtual(base_info);

        if constexpr (is_virtual_base) {
            return CompileString{"@"} +
                   CompileString<32>::from_number(base_offset) +
                   CompileString{"[vbase]:"} +
                   TypeSignature<BaseType>::calculate();
        } else {
            return CompileString{"@"} +
                   CompileString<32>::from_number(base_offset) +
                   CompileString{"[base]:"} +
                   TypeSignature<BaseType>::calculate();
        }
    }

    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto build_base_with_comma() noexcept {
        if constexpr (IsFirst) {
            return get_base_signature<T, Index>();
        } else {
            return CompileString{","} + get_base_signature<T, Index>();
        }
    }

    template<typename T, std::size_t... Indices>
    consteval auto concatenate_base_signatures(std::index_sequence<Indices...>) noexcept {
        return (build_base_with_comma<T, Indices, (Indices == 0)>() + ...);
    }

    template <typename T>
    consteval auto get_bases_signature() noexcept {
        constexpr std::size_t count = get_base_count<T>();
        if constexpr (count == 0) {
            return CompileString{""};
        } else {
            return concatenate_base_signatures<T>(std::make_index_sequence<count>{});
        }
    }

    // =========================================================================
    // Combined Layout Content Signature
    // =========================================================================

    template <typename T>
    consteval auto get_layout_content_signature() noexcept {
        constexpr std::size_t base_count = get_base_count<T>();
        constexpr std::size_t field_count = get_member_count<T>();
        
        if constexpr (base_count == 0 && field_count == 0) {
            return CompileString{""};
        } else if constexpr (base_count == 0) {
            return get_fields_signature<T>();
        } else if constexpr (field_count == 0) {
            return get_bases_signature<T>();
        } else {
            return get_bases_signature<T>() + CompileString{","} + get_fields_signature<T>();
        }
    }

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_REFLECTION_HELPERS_HPP
