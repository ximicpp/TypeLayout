// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_LAYOUT_ENGINE_HPP
#define BOOST_TYPELAYOUT_CORE_LAYOUT_ENGINE_HPP

#include <boost/typelayout/core/reflection_meta.hpp>

namespace boost {
namespace typelayout {

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

#endif // BOOST_TYPELAYOUT_CORE_LAYOUT_ENGINE_HPP
