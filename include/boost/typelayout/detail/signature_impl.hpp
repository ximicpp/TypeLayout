// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
//
// Signature computation engines: Definition mode (tree with names),
// Layout mode (flattened byte identity), and union layout helpers.

#ifndef BOOST_TYPELAYOUT_DETAIL_SIGNATURE_IMPL_HPP
#define BOOST_TYPELAYOUT_DETAIL_SIGNATURE_IMPL_HPP

#include <boost/typelayout/detail/reflect.hpp>

namespace boost {
namespace typelayout {

    // =========================================================================
    // Definition Signature Engine
    // =========================================================================

    // --- Fields ---

    template<typename T, std::size_t Index>
    consteval auto definition_field_signature() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            return FixedString{"@"} +
                   to_fixed_string(bit_off.bytes) +
                   FixedString{"."} +
                   to_fixed_string(bit_off.bits) +
                   FixedString{"["} + get_member_name<member, Index>() +
                   FixedString{"]:bits<"} +
                   to_fixed_string(bit_size_of(member)) +
                   FixedString{","} +
                   TypeSignature<FieldType, SignatureMode::Definition>::calculate() +
                   FixedString{">"};
        } else {
            return FixedString{"@"} +
                   to_fixed_string(offset_of(member).bytes) +
                   FixedString{"["} + get_member_name<member, Index>() +
                   FixedString{"]:"} +
                   TypeSignature<FieldType, SignatureMode::Definition>::calculate();
        }
    }

    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto definition_field_with_comma() noexcept {
        if constexpr (IsFirst)
            return definition_field_signature<T, Index>();
        else
            return FixedString{","} + definition_field_signature<T, Index>();
    }

    template<typename T, std::size_t... Is>
    consteval auto concatenate_definition_fields(std::index_sequence<Is...>) noexcept {
        return (definition_field_with_comma<T, Is, (Is == 0)>() + ...);
    }

    template <typename T>
    consteval auto definition_fields() noexcept {
        constexpr std::size_t count = get_member_count<T>();
        if constexpr (count == 0)
            return FixedString{""};
        else
            return concatenate_definition_fields<T>(std::make_index_sequence<count>{});
    }

    // --- Bases ---

    template<typename T, std::size_t Index>
    consteval auto definition_base_signature() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[Index];
        using BaseType = [:type_of(base_info):];

        if constexpr (is_virtual(base_info))
            return FixedString{"~vbase<"} + get_base_name<base_info>() + FixedString{">:"} +
                   TypeSignature<BaseType, SignatureMode::Definition>::calculate();
        else
            return FixedString{"~base<"} + get_base_name<base_info>() + FixedString{">:"} +
                   TypeSignature<BaseType, SignatureMode::Definition>::calculate();
    }

    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto definition_base_with_comma() noexcept {
        if constexpr (IsFirst)
            return definition_base_signature<T, Index>();
        else
            return FixedString{","} + definition_base_signature<T, Index>();
    }

    template<typename T, std::size_t... Is>
    consteval auto concatenate_definition_bases(std::index_sequence<Is...>) noexcept {
        return (definition_base_with_comma<T, Is, (Is == 0)>() + ...);
    }

    template <typename T>
    consteval auto definition_bases() noexcept {
        constexpr std::size_t count = get_base_count<T>();
        if constexpr (count == 0)
            return FixedString{""};
        else
            return concatenate_definition_bases<T>(std::make_index_sequence<count>{});
    }

    // --- Combined ---

    template <typename T>
    consteval auto definition_content() noexcept {
        constexpr std::size_t bc = get_base_count<T>();
        constexpr std::size_t fc = get_member_count<T>();
        if constexpr (bc == 0 && fc == 0) return FixedString{""};
        else if constexpr (bc == 0) return definition_fields<T>();
        else if constexpr (fc == 0) return definition_bases<T>();
        else return definition_bases<T>() + FixedString{","} + definition_fields<T>();
    }

    // =========================================================================
    // Layout Signature Engine
    // =========================================================================

    // Detect whether a type has a custom (non-default) TypeSignature
    // specialization, such as those generated by TYPELAYOUT_OPAQUE_* macros.
    // Opaque specializations define `static constexpr bool is_opaque = true`.
    template <typename T, SignatureMode Mode>
    concept has_opaque_signature = requires {
        { TypeSignature<T, Mode>::is_opaque } -> std::convertible_to<bool>;
    } && TypeSignature<T, Mode>::is_opaque;

    // Every helper returns a comma-PREFIXED string. The top-level function
    // strips the leading comma via skip_first().

    template <typename T, std::size_t OffsetAdj>
    consteval auto layout_all_prefixed() noexcept;

    template<typename T, std::size_t Index, std::size_t OffsetAdj>
    consteval auto layout_field_with_comma() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            return FixedString{",@"} +
                   to_fixed_string(bit_off.bytes + OffsetAdj) +
                   FixedString{"."} +
                   to_fixed_string(bit_off.bits) +
                   FixedString{":bits<"} +
                   to_fixed_string(bit_size_of(member)) +
                   FixedString{","} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate() +
                   FixedString{">"};
        } else if constexpr (std::is_class_v<FieldType> && !std::is_union_v<FieldType>
                             && !has_opaque_signature<FieldType, SignatureMode::Layout>) {
            // Non-opaque class: recursively flatten into parent layout.
            constexpr std::size_t field_offset = offset_of(member).bytes + OffsetAdj;
            return layout_all_prefixed<FieldType, field_offset>();
        } else {
            // Primitive, union, enum, opaque class, or any type with a
            // custom TypeSignature specialization: emit as a leaf node.
            return FixedString{",@"} +
                   to_fixed_string(offset_of(member).bytes + OffsetAdj) +
                   FixedString{":"} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate();
        }
    }

    template <typename T, std::size_t OffsetAdj, std::size_t... Is>
    consteval auto layout_direct_fields_prefixed(std::index_sequence<Is...>) noexcept {
        if constexpr (sizeof...(Is) == 0) return FixedString{""};
        else return (layout_field_with_comma<T, Is, OffsetAdj>() + ...);
    }

    template <typename T, std::size_t BaseIndex, std::size_t OffsetAdj>
    consteval auto layout_one_base_prefixed() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[BaseIndex];
        using BaseType = [:type_of(base_info):];
        if constexpr (has_opaque_signature<BaseType, SignatureMode::Layout>) {
            // Opaque base: emit as leaf node at base offset, not flattened.
            return FixedString{",@"} +
                   to_fixed_string(offset_of(base_info).bytes + OffsetAdj) +
                   FixedString{":"} +
                   TypeSignature<BaseType, SignatureMode::Layout>::calculate();
        } else {
            return layout_all_prefixed<BaseType, offset_of(base_info).bytes + OffsetAdj>();
        }
    }

    template <typename T, std::size_t OffsetAdj, std::size_t... Is>
    consteval auto layout_bases_prefixed(std::index_sequence<Is...>) noexcept {
        if constexpr (sizeof...(Is) == 0) return FixedString{""};
        else return (layout_one_base_prefixed<T, Is, OffsetAdj>() + ...);
    }

    // Synthesize a comma-prefixed vptr field if T introduces polymorphism.
    // vptr is physically a pointer; we encode it as ptr[s:N,a:N] so that
    // classify_safety's existing "ptr[" pattern detects it automatically,
    // even when the polymorphic type is flattened into a parent record.
    template <typename T, std::size_t OffsetAdj>
    consteval auto maybe_vptr_prefixed() noexcept {
        if constexpr (introduces_vptr<T>()) {
            return FixedString{",@"} + to_fixed_string(OffsetAdj) +
                   FixedString{":ptr[s:"} +
                   to_fixed_string(sizeof(void*)) +
                   FixedString{",a:"} +
                   to_fixed_string(alignof(void*)) +
                   FixedString{"]"};
        } else {
            return FixedString{""};
        }
    }

    template <typename T, std::size_t OffsetAdj>
    consteval auto layout_all_prefixed() noexcept {
        constexpr std::size_t bc = get_base_count<T>();
        constexpr std::size_t fc = get_member_count<T>();
        auto vptr = maybe_vptr_prefixed<T, OffsetAdj>();
        if constexpr (bc == 0 && fc == 0) return vptr;
        else if constexpr (bc == 0) return vptr + layout_direct_fields_prefixed<T, OffsetAdj>(std::make_index_sequence<fc>{});
        else if constexpr (fc == 0) return vptr + layout_bases_prefixed<T, OffsetAdj>(std::make_index_sequence<bc>{});
        else return vptr + layout_bases_prefixed<T, OffsetAdj>(std::make_index_sequence<bc>{}) +
                    layout_direct_fields_prefixed<T, OffsetAdj>(std::make_index_sequence<fc>{});
    }

    template <typename T>
    consteval auto get_layout_content() noexcept {
        return layout_all_prefixed<T, 0>().skip_first();
    }

    // =========================================================================
    // Union Layout Helpers (no flattening)
    // =========================================================================

    template<typename T, std::size_t Index>
    consteval auto layout_union_field() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            return FixedString{"@"} +
                   to_fixed_string(bit_off.bytes) +
                   FixedString{"."} +
                   to_fixed_string(bit_off.bits) +
                   FixedString{":bits<"} +
                   to_fixed_string(bit_size_of(member)) +
                   FixedString{","} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate() +
                   FixedString{">"};
        } else {
            return FixedString{"@"} +
                   to_fixed_string(offset_of(member).bytes) +
                   FixedString{":"} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate();
        }
    }

    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto layout_union_field_with_comma() noexcept {
        if constexpr (IsFirst)
            return layout_union_field<T, Index>();
        else
            return FixedString{","} + layout_union_field<T, Index>();
    }

    template<typename T, std::size_t... Is>
    consteval auto concatenate_layout_union_fields(std::index_sequence<Is...>) noexcept {
        return (layout_union_field_with_comma<T, Is, (Is == 0)>() + ...);
    }

    template <typename T>
    consteval auto get_layout_union_content() noexcept {
        constexpr std::size_t count = get_member_count<T>();
        if constexpr (count == 0)
            return FixedString{""};
        else
            return concatenate_layout_union_fields<T>(std::make_index_sequence<count>{});
    }

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_DETAIL_SIGNATURE_IMPL_HPP
