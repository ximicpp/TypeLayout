// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_REFLECTION_HELPERS_HPP
#define BOOST_TYPELAYOUT_CORE_REFLECTION_HELPERS_HPP

#ifndef BOOST_TYPELAYOUT_INTERNAL_INCLUDE_
    #error "Do not include reflection_helpers.hpp directly. Include <boost/typelayout/core/type_signature.hpp> instead."
#endif

#include <boost/typelayout/core/config.hpp>
#include <boost/typelayout/core/compile_string.hpp>
#include <experimental/meta>

namespace boost {
namespace typelayout {

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

    // --- Definition mode: fields ---

    template<typename T, std::size_t Index>
    consteval auto definition_field_signature() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            return CompileString{"@"} +
                   CompileString<number_buffer_size>::from_number(bit_off.bytes) +
                   CompileString{"."} +
                   CompileString<number_buffer_size>::from_number(bit_off.bits) +
                   CompileString{"["} + get_member_name<member, Index>() +
                   CompileString{"]:bits<"} +
                   CompileString<number_buffer_size>::from_number(bit_size_of(member)) +
                   CompileString{","} +
                   TypeSignature<FieldType, SignatureMode::Definition>::calculate() +
                   CompileString{">"};
        } else {
            return CompileString{"@"} +
                   CompileString<number_buffer_size>::from_number(offset_of(member).bytes) +
                   CompileString{"["} + get_member_name<member, Index>() +
                   CompileString{"]:"} +
                   TypeSignature<FieldType, SignatureMode::Definition>::calculate();
        }
    }

    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto definition_field_with_comma() noexcept {
        if constexpr (IsFirst)
            return definition_field_signature<T, Index>();
        else
            return CompileString{","} + definition_field_signature<T, Index>();
    }

    template<typename T, std::size_t... Is>
    consteval auto concatenate_definition_fields(std::index_sequence<Is...>) noexcept {
        return (definition_field_with_comma<T, Is, (Is == 0)>() + ...);
    }

    template <typename T>
    consteval auto definition_fields() noexcept {
        constexpr std::size_t count = get_member_count<T>();
        if constexpr (count == 0)
            return CompileString{""};
        else
            return concatenate_definition_fields<T>(std::make_index_sequence<count>{});
    }

    // --- Definition mode: bases ---

    template<typename T, std::size_t Index>
    consteval auto definition_base_signature() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[Index];
        using BaseType = [:type_of(base_info):];

        if constexpr (is_virtual(base_info))
            return CompileString{"~vbase<"} + get_base_name<base_info>() + CompileString{">:"} +
                   TypeSignature<BaseType, SignatureMode::Definition>::calculate();
        else
            return CompileString{"~base<"} + get_base_name<base_info>() + CompileString{">:"} +
                   TypeSignature<BaseType, SignatureMode::Definition>::calculate();
    }

    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto definition_base_with_comma() noexcept {
        if constexpr (IsFirst)
            return definition_base_signature<T, Index>();
        else
            return CompileString{","} + definition_base_signature<T, Index>();
    }

    template<typename T, std::size_t... Is>
    consteval auto concatenate_definition_bases(std::index_sequence<Is...>) noexcept {
        return (definition_base_with_comma<T, Is, (Is == 0)>() + ...);
    }

    template <typename T>
    consteval auto definition_bases() noexcept {
        constexpr std::size_t count = get_base_count<T>();
        if constexpr (count == 0)
            return CompileString{""};
        else
            return concatenate_definition_bases<T>(std::make_index_sequence<count>{});
    }

    // --- Definition mode: combined ---

    template <typename T>
    consteval auto definition_content() noexcept {
        constexpr std::size_t bc = get_base_count<T>();
        constexpr std::size_t fc = get_member_count<T>();
        if constexpr (bc == 0 && fc == 0) return CompileString{""};
        else if constexpr (bc == 0) return definition_fields<T>();
        else if constexpr (fc == 0) return definition_bases<T>();
        else return definition_bases<T>() + CompileString{","} + definition_fields<T>();
    }

    // --- Layout mode: inheritance and composition flattening ---
    //
    // Every helper returns a comma-PREFIXED string. The top-level function
    // strips the leading comma via skip_first().

    // Forward declarations for mutual recursion
    template <typename T, std::size_t OffsetAdj>
    consteval auto layout_all_prefixed() noexcept;

    template<typename T, std::size_t Index, std::size_t OffsetAdj>
    consteval auto layout_field_with_comma() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            return CompileString{",@"} +
                   CompileString<number_buffer_size>::from_number(bit_off.bytes + OffsetAdj) +
                   CompileString{"."} +
                   CompileString<number_buffer_size>::from_number(bit_off.bits) +
                   CompileString{":bits<"} +
                   CompileString<number_buffer_size>::from_number(bit_size_of(member)) +
                   CompileString{","} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate() +
                   CompileString{">"};
        } else if constexpr (std::is_class_v<FieldType> && !std::is_union_v<FieldType>) {
            // Flatten nested class/struct: recursively expand its fields at the adjusted offset
            constexpr std::size_t field_offset = offset_of(member).bytes + OffsetAdj;
            return layout_all_prefixed<FieldType, field_offset>();
        } else {
            return CompileString{",@"} +
                   CompileString<number_buffer_size>::from_number(offset_of(member).bytes + OffsetAdj) +
                   CompileString{":"} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate();
        }
    }

    template <typename T, std::size_t OffsetAdj, std::size_t... Is>
    consteval auto layout_direct_fields_prefixed(std::index_sequence<Is...>) noexcept {
        if constexpr (sizeof...(Is) == 0) return CompileString{""};
        else return (layout_field_with_comma<T, Is, OffsetAdj>() + ...);
    }

    template <typename T, std::size_t BaseIndex, std::size_t OffsetAdj>
    consteval auto layout_one_base_prefixed() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[BaseIndex];
        using BaseType = [:type_of(base_info):];
        // Include both virtual and non-virtual bases.
        // offset_of gives the correct offset for the type being reflected.
        return layout_all_prefixed<BaseType, offset_of(base_info).bytes + OffsetAdj>();
    }

    template <typename T, std::size_t OffsetAdj, std::size_t... Is>
    consteval auto layout_bases_prefixed(std::index_sequence<Is...>) noexcept {
        if constexpr (sizeof...(Is) == 0) return CompileString{""};
        else return (layout_one_base_prefixed<T, Is, OffsetAdj>() + ...);
    }

    template <typename T, std::size_t OffsetAdj>
    consteval auto layout_all_prefixed() noexcept {
        constexpr std::size_t bc = get_base_count<T>();
        constexpr std::size_t fc = get_member_count<T>();
        if constexpr (bc == 0 && fc == 0) return CompileString{""};
        else if constexpr (bc == 0) return layout_direct_fields_prefixed<T, OffsetAdj>(std::make_index_sequence<fc>{});
        else if constexpr (fc == 0) return layout_bases_prefixed<T, OffsetAdj>(std::make_index_sequence<bc>{});
        else return layout_bases_prefixed<T, OffsetAdj>(std::make_index_sequence<bc>{}) +
                    layout_direct_fields_prefixed<T, OffsetAdj>(std::make_index_sequence<fc>{});
    }

    template <typename T>
    consteval auto get_layout_content() noexcept {
        return layout_all_prefixed<T, 0>().skip_first();
    }

    // --- Layout mode for unions: no flattening ---
    // Union members are kept as atomic type signatures (not recursively expanded),
    // because expanding would mix sub-fields from overlapping members.

    template<typename T, std::size_t Index>
    consteval auto layout_union_field() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            return CompileString{"@"} +
                   CompileString<number_buffer_size>::from_number(bit_off.bytes) +
                   CompileString{"."} +
                   CompileString<number_buffer_size>::from_number(bit_off.bits) +
                   CompileString{":bits<"} +
                   CompileString<number_buffer_size>::from_number(bit_size_of(member)) +
                   CompileString{","} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate() +
                   CompileString{">"};
        } else {
            return CompileString{"@"} +
                   CompileString<number_buffer_size>::from_number(offset_of(member).bytes) +
                   CompileString{":"} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate();
        }
    }

    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto layout_union_field_with_comma() noexcept {
        if constexpr (IsFirst)
            return layout_union_field<T, Index>();
        else
            return CompileString{","} + layout_union_field<T, Index>();
    }

    template<typename T, std::size_t... Is>
    consteval auto concatenate_layout_union_fields(std::index_sequence<Is...>) noexcept {
        return (layout_union_field_with_comma<T, Is, (Is == 0)>() + ...);
    }

    template <typename T>
    consteval auto get_layout_union_content() noexcept {
        constexpr std::size_t count = get_member_count<T>();
        if constexpr (count == 0)
            return CompileString{""};
        else
            return concatenate_layout_union_fields<T>(std::make_index_sequence<count>{});
    }

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_REFLECTION_HELPERS_HPP