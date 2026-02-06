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

// This is an internal header. Include <boost/typelayout/core/type_signature.hpp> instead.
#ifndef BOOST_TYPELAYOUT_INTERNAL_INCLUDE_
    #error "Do not include reflection_helpers.hpp directly. Include <boost/typelayout/core/type_signature.hpp> instead."
#endif

#include <boost/typelayout/core/config.hpp>
#include <boost/typelayout/core/compile_string.hpp>
#include <experimental/meta>

namespace boost {
namespace typelayout {

    // Forward declaration
    template <typename T, SignatureMode Mode = SignatureMode::Layout>
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
    // Helper: get member name or anonymous placeholder
    // =========================================================================

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

    // =========================================================================
    // Helper: get base class short name
    // =========================================================================

    template<std::meta::info BaseInfo>
    consteval auto get_base_name() noexcept {
        using namespace std::meta;
        constexpr auto base_type = type_of(BaseInfo);
        constexpr std::string_view name = identifier_of(base_type);
        constexpr size_t NameLen = name.size() + 1;
        return CompileString<NameLen>(name);
    }

    // =========================================================================
    // Definition Mode: Field Signature Generation
    // =========================================================================

    /// Build signature for a single field in Definition mode: @OFF[name]:TYPE
    template<typename T, std::size_t Index>
    consteval auto definition_field_signature() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            constexpr std::size_t byte_offset = bit_off.bytes;
            constexpr std::size_t bit_offset = bit_off.bits;
            constexpr std::size_t bit_width = bit_size_of(member);

            return CompileString{"@"} +
                   CompileString<number_buffer_size>::from_number(byte_offset) +
                   CompileString{"."} +
                   CompileString<number_buffer_size>::from_number(bit_offset) +
                   CompileString{"["} +
                   get_member_name<member, Index>() +
                   CompileString{"]:bits<"} +
                   CompileString<number_buffer_size>::from_number(bit_width) +
                   CompileString{","} +
                   TypeSignature<FieldType, SignatureMode::Definition>::calculate() +
                   CompileString{">"};
        } else {
            constexpr std::size_t offset = offset_of(member).bytes;

            return CompileString{"@"} +
                   CompileString<number_buffer_size>::from_number(offset) +
                   CompileString{"["} +
                   get_member_name<member, Index>() +
                   CompileString{"]:"} +
                   TypeSignature<FieldType, SignatureMode::Definition>::calculate();
        }
    }

    /// Build definition field with comma prefix (except first)
    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto definition_field_with_comma() noexcept {
        if constexpr (IsFirst) {
            return definition_field_signature<T, Index>();
        } else {
            return CompileString{","} + definition_field_signature<T, Index>();
        }
    }

    /// Concatenate all definition field signatures
    template<typename T, std::size_t... Indices>
    consteval auto concatenate_definition_fields(std::index_sequence<Indices...>) noexcept {
        return (definition_field_with_comma<T, Indices, (Indices == 0)>() + ...);
    }

    /// Get all fields signature in Definition mode
    template <typename T>
    consteval auto definition_fields() noexcept {
        constexpr std::size_t count = get_member_count<T>();
        if constexpr (count == 0) {
            return CompileString{""};
        } else {
            return concatenate_definition_fields<T>(std::make_index_sequence<count>{});
        }
    }

    // =========================================================================
    // Definition Mode: Base Class Signature Generation
    // =========================================================================

    /// Build signature for a single base class in Definition mode:
    /// ~base<Name>:record[...]{...} or ~vbase<Name>:record[...]{...}
    template<typename T, std::size_t Index>
    consteval auto definition_base_signature() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[Index];
        using BaseType = [:type_of(base_info):];
        constexpr bool is_virtual_base = is_virtual(base_info);

        if constexpr (is_virtual_base) {
            return CompileString{"~vbase<"} +
                   get_base_name<base_info>() +
                   CompileString{">:"} +
                   TypeSignature<BaseType, SignatureMode::Definition>::calculate();
        } else {
            return CompileString{"~base<"} +
                   get_base_name<base_info>() +
                   CompileString{">:"} +
                   TypeSignature<BaseType, SignatureMode::Definition>::calculate();
        }
    }

    /// Build definition base with comma prefix (except first)
    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto definition_base_with_comma() noexcept {
        if constexpr (IsFirst) {
            return definition_base_signature<T, Index>();
        } else {
            return CompileString{","} + definition_base_signature<T, Index>();
        }
    }

    /// Concatenate all definition base signatures
    template<typename T, std::size_t... Indices>
    consteval auto concatenate_definition_bases(std::index_sequence<Indices...>) noexcept {
        return (definition_base_with_comma<T, Indices, (Indices == 0)>() + ...);
    }

    /// Get all bases signature in Definition mode
    template <typename T>
    consteval auto definition_bases() noexcept {
        constexpr std::size_t count = get_base_count<T>();
        if constexpr (count == 0) {
            return CompileString{""};
        } else {
            return concatenate_definition_bases<T>(std::make_index_sequence<count>{});
        }
    }

    // =========================================================================
    // Definition Mode: Combined Content
    // =========================================================================

    /// Get combined definition content (bases + fields)
    template <typename T>
    consteval auto definition_content() noexcept {
        constexpr std::size_t base_count = get_base_count<T>();
        constexpr std::size_t field_count = get_member_count<T>();

        if constexpr (base_count == 0 && field_count == 0) {
            return CompileString{""};
        } else if constexpr (base_count == 0) {
            return definition_fields<T>();
        } else if constexpr (field_count == 0) {
            return definition_bases<T>();
        } else {
            return definition_bases<T>() + CompileString{","} + definition_fields<T>();
        }
    }

    // =========================================================================
    // Layout Mode: Inheritance Flattening
    // =========================================================================
    //
    // Layout mode flattens non-virtual base class sub-objects so that
    // a derived type and a flat struct with identical byte layout produce
    // identical signatures.
    //
    // Strategy: Every helper returns a comma-PREFIXED string (e.g. ",@0:i32[s:4,a:4]").
    // The top-level function strips the leading comma at the end.
    // Empty results (from empty bases) are empty strings, which concatenate harmlessly.
    //
    // Limitation (v1): Virtual bases are skipped during recursive flattening
    // because their offsets within intermediate bases depend on the most-derived
    // type and cannot be reliably adjusted.
    // =========================================================================

    /// Emit a single direct field of T with absolute offset adjustment.
    /// Always returns a comma-prefixed string: ",@OFFSET:TYPE"
    template<typename T, std::size_t Index, std::size_t OffsetAdj>
    consteval auto layout_field_with_comma() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            constexpr std::size_t byte_offset = bit_off.bytes + OffsetAdj;
            constexpr std::size_t bit_offset = bit_off.bits;
            constexpr std::size_t bit_width = bit_size_of(member);

            return CompileString{",@"} +
                   CompileString<number_buffer_size>::from_number(byte_offset) +
                   CompileString{"."} +
                   CompileString<number_buffer_size>::from_number(bit_offset) +
                   CompileString{":bits<"} +
                   CompileString<number_buffer_size>::from_number(bit_width) +
                   CompileString{","} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate() +
                   CompileString{">"};
        } else {
            constexpr std::size_t offset = offset_of(member).bytes + OffsetAdj;

            return CompileString{",@"} +
                   CompileString<number_buffer_size>::from_number(offset) +
                   CompileString{":"} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate();
        }
    }

    /// Emit all direct fields of T with offset adjustment (comma-prefixed each).
    template <typename T, std::size_t OffsetAdj, std::size_t... Indices>
    consteval auto layout_direct_fields_prefixed(std::index_sequence<Indices...>) noexcept {
        if constexpr (sizeof...(Indices) == 0) {
            return CompileString{""};
        } else {
            return (layout_field_with_comma<T, Indices, OffsetAdj>() + ...);
        }
    }

    // Forward declaration for mutual recursion
    template <typename T, std::size_t OffsetAdj>
    consteval auto layout_all_prefixed() noexcept;

    /// Recursively flatten one non-virtual base's fields.
    template <typename T, std::size_t BaseIndex, std::size_t OffsetAdj>
    consteval auto layout_one_base_prefixed() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[BaseIndex];

        if constexpr (is_virtual(base_info)) {
            // v1 limitation: skip virtual bases during flattening.
            return CompileString{""};
        } else {
            using BaseType = [:type_of(base_info):];
            constexpr std::size_t abs_offset = offset_of(base_info).bytes + OffsetAdj;
            return layout_all_prefixed<BaseType, abs_offset>();
        }
    }

    /// Emit all bases' flattened fields (comma-prefixed each).
    template <typename T, std::size_t OffsetAdj, std::size_t... BaseIndices>
    consteval auto layout_bases_prefixed(std::index_sequence<BaseIndices...>) noexcept {
        if constexpr (sizeof...(BaseIndices) == 0) {
            return CompileString{""};
        } else {
            return (layout_one_base_prefixed<T, BaseIndices, OffsetAdj>() + ...);
        }
    }

    /// Collect ALL flattened fields (recurse into bases, then direct fields).
    template <typename T, std::size_t OffsetAdj>
    consteval auto layout_all_prefixed() noexcept {
        constexpr std::size_t base_count = get_base_count<T>();
        constexpr std::size_t field_count = get_member_count<T>();

        if constexpr (base_count == 0 && field_count == 0) {
            return CompileString{""};
        } else if constexpr (base_count == 0) {
            return layout_direct_fields_prefixed<T, OffsetAdj>(
                std::make_index_sequence<field_count>{});
        } else if constexpr (field_count == 0) {
            return layout_bases_prefixed<T, OffsetAdj>(
                std::make_index_sequence<base_count>{});
        } else {
            return layout_bases_prefixed<T, OffsetAdj>(
                       std::make_index_sequence<base_count>{}) +
                   layout_direct_fields_prefixed<T, OffsetAdj>(
                       std::make_index_sequence<field_count>{});
        }
    }

    /// Top-level entry: get flattened layout content, strip leading comma.
    template <typename T>
    consteval auto get_layout_content() noexcept {
        constexpr auto prefixed = layout_all_prefixed<T, 0>();
        return prefixed.skip_first();
    }

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_REFLECTION_HELPERS_HPP