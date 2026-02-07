// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_DEFINITION_ENGINE_HPP
#define BOOST_TYPELAYOUT_CORE_DEFINITION_ENGINE_HPP

#include <boost/typelayout/core/reflection_meta.hpp>

namespace boost {
namespace typelayout {

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

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_DEFINITION_ENGINE_HPP
