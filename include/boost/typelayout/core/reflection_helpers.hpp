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
    template <typename T, SignatureMode Mode = SignatureMode::Structural>
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

    // Helper to generate field name or anonymous placeholder (Annotated mode only)
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

    // Build signature for a single field with mode awareness
    template<typename T, std::size_t Index, SignatureMode Mode>
    consteval auto get_field_signature() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            constexpr std::size_t byte_offset = bit_off.bytes;
            constexpr std::size_t bit_offset = bit_off.bits;
            constexpr std::size_t bit_width = bit_size_of(member);
            
            if constexpr (Mode == SignatureMode::Annotated) {
                // Annotated: @BYTE.BIT[name]:bits<WIDTH,TYPE>
                return CompileString{"@"} +
                       CompileString<number_buffer_size>::from_number(byte_offset) +
                       CompileString{"."} +
                       CompileString<number_buffer_size>::from_number(bit_offset) +
                       CompileString{"["} +
                       get_member_name<member, Index>() +
                       CompileString{"]:bits<"} +
                       CompileString<number_buffer_size>::from_number(bit_width) +
                       CompileString{","} +
                       TypeSignature<FieldType, Mode>::calculate() +
                       CompileString{">"};
            } else {
                // Structural: @BYTE.BIT:bits<WIDTH,TYPE> (no name)
                return CompileString{"@"} +
                       CompileString<number_buffer_size>::from_number(byte_offset) +
                       CompileString{"."} +
                       CompileString<number_buffer_size>::from_number(bit_offset) +
                       CompileString{":bits<"} +
                       CompileString<number_buffer_size>::from_number(bit_width) +
                       CompileString{","} +
                       TypeSignature<FieldType, Mode>::calculate() +
                       CompileString{">"};
            }
        } else {
            constexpr std::size_t offset = offset_of(member).bytes;
            
            if constexpr (Mode == SignatureMode::Annotated) {
                // Annotated: @OFFSET[name]:TYPE
                return CompileString{"@"} +
                       CompileString<number_buffer_size>::from_number(offset) +
                       CompileString{"["} +
                       get_member_name<member, Index>() +
                       CompileString{"]:"} +
                       TypeSignature<FieldType, Mode>::calculate();
            } else {
                // Structural: @OFFSET:TYPE (no name)
                return CompileString{"@"} +
                       CompileString<number_buffer_size>::from_number(offset) +
                       CompileString{":"} +
                       TypeSignature<FieldType, Mode>::calculate();
            }
        }
    }

    // Build field signature with comma prefix (except first)
    template<typename T, std::size_t Index, bool IsFirst, SignatureMode Mode>
    consteval auto build_field_with_comma() noexcept {
        if constexpr (IsFirst) {
            return get_field_signature<T, Index, Mode>();
        } else {
            return CompileString{","} + get_field_signature<T, Index, Mode>();
        }
    }

    // Concatenate all field signatures
    template<typename T, SignatureMode Mode, std::size_t... Indices>
    consteval auto concatenate_field_signatures(std::index_sequence<Indices...>) noexcept {
        return (build_field_with_comma<T, Indices, (Indices == 0), Mode>() + ...);
    }

    // Get all fields signature for a struct/class
    template <typename T, SignatureMode Mode = SignatureMode::Structural>
    consteval auto get_fields_signature() noexcept {
        constexpr std::size_t count = get_member_count<T>();
        if constexpr (count == 0) {
            return CompileString{""};
        } else {
            return concatenate_field_signatures<T, Mode>(std::make_index_sequence<count>{});
        }
    }

    // =========================================================================
    // Base Class Signature Generation
    // =========================================================================
    //
    // DESIGN DECISION: Why encode base classes in signatures?
    //
    // Even non-polymorphic, non-virtual inheritance affects ABI identity:
    // - Construction/destruction semantics differ
    // - Slicing behavior differs
    // - Some platform ABIs treat inherited types differently (e.g., parameter passing)
    //
    // Consequence: Derived{int x; double y;} and Flat{int x; double y;} with
    // identical byte layouts will have DIFFERENT signatures. This is intentional.
    //
    // The ~base: and ~vbase: prefixes mark base class subobjects without being
    // member "names" — they're structural markers in Structural mode.
    //
    // See doc/design/abi-identity.md for detailed rationale.
    // =========================================================================

    template<typename T, std::size_t Index, SignatureMode Mode>
    consteval auto get_base_signature() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[Index];
        
        using BaseType = [:type_of(base_info):];
        constexpr std::size_t base_offset = offset_of(base_info).bytes;
        constexpr bool is_virtual_base = is_virtual(base_info);

        if constexpr (Mode == SignatureMode::Annotated) {
            // Annotated mode: include [base] or [vbase] marker
            if constexpr (is_virtual_base) {
                return CompileString{"@"} +
                       CompileString<number_buffer_size>::from_number(base_offset) +
                       CompileString{"[vbase]:"} +
                       TypeSignature<BaseType, Mode>::calculate();
            } else {
                return CompileString{"@"} +
                       CompileString<number_buffer_size>::from_number(base_offset) +
                       CompileString{"[base]:"} +
                       TypeSignature<BaseType, Mode>::calculate();
            }
        } else {
            // Structural mode: no marker, just offset and type
            // Use ~vbase or ~base to indicate base class without being a "name"
            if constexpr (is_virtual_base) {
                return CompileString{"@"} +
                       CompileString<number_buffer_size>::from_number(base_offset) +
                       CompileString{"~vbase:"} +
                       TypeSignature<BaseType, Mode>::calculate();
            } else {
                return CompileString{"@"} +
                       CompileString<number_buffer_size>::from_number(base_offset) +
                       CompileString{"~base:"} +
                       TypeSignature<BaseType, Mode>::calculate();
            }
        }
    }

    template<typename T, std::size_t Index, bool IsFirst, SignatureMode Mode>
    consteval auto build_base_with_comma() noexcept {
        if constexpr (IsFirst) {
            return get_base_signature<T, Index, Mode>();
        } else {
            return CompileString{","} + get_base_signature<T, Index, Mode>();
        }
    }

    template<typename T, SignatureMode Mode, std::size_t... Indices>
    consteval auto concatenate_base_signatures(std::index_sequence<Indices...>) noexcept {
        return (build_base_with_comma<T, Indices, (Indices == 0), Mode>() + ...);
    }

    template <typename T, SignatureMode Mode = SignatureMode::Structural>
    consteval auto get_bases_signature() noexcept {
        constexpr std::size_t count = get_base_count<T>();
        if constexpr (count == 0) {
            return CompileString{""};
        } else {
            return concatenate_base_signatures<T, Mode>(std::make_index_sequence<count>{});
        }
    }

    // =========================================================================
    // Combined Layout Content Signature
    // =========================================================================

    /**
     * Get layout content signature.
     * 
     * NOTE: We use the index_sequence-based implementation (v1) because the
     * template-for based optimization (v2) doesn't work with current P2996
     * toolchain. The issue is that nonstatic_data_members_of() returns a
     * std::vector with heap allocation, which cannot be used as a constexpr
     * range for template for expansion.
     * 
     * This results in O(n) calls to nonstatic_data_members_of() for n members,
     * which hits constexpr step limits for large structs (>40-50 members).
     * This is a known toolchain limitation, not a library design issue.
     */
    template <typename T, SignatureMode Mode = SignatureMode::Structural>
    consteval auto get_layout_content_signature() noexcept {
        constexpr std::size_t base_count = get_base_count<T>();
        constexpr std::size_t field_count = get_member_count<T>();
        
        if constexpr (base_count == 0 && field_count == 0) {
            return CompileString{""};
        } else if constexpr (base_count == 0) {
            return get_fields_signature<T, Mode>();
        } else if constexpr (field_count == 0) {
            return get_bases_signature<T, Mode>();
        } else {
            return get_bases_signature<T, Mode>() + CompileString{","} + get_fields_signature<T, Mode>();
        }
    }

    // =========================================================================
    // Physical Mode: Inheritance Flattening
    // =========================================================================
    //
    // Physical mode flattens non-virtual base class sub-objects so that
    // a derived type and a flat struct with identical byte layout produce
    // identical signatures.
    //
    // Strategy: Every helper returns a comma-PREFIXED string (e.g. ",@0:i32[s:4,a:4]").
    // The top-level function strips the leading comma at the end.
    // Empty results (from empty bases) are empty strings, which concatenate harmlessly.
    //
    // Limitation (v1): Virtual bases are skipped during recursive flattening
    // because their offsets within intermediate bases depend on the most-derived
    // type and cannot be reliably adjusted. The parent type's size/alignment
    // still accounts for virtual base sub-objects. This is acceptable because
    // types with virtual inheritance are rarely used in data-exchange scenarios.
    // =========================================================================

    /// Emit a single direct (non-bitfield) field of T with absolute offset adjustment.
    /// Always returns a comma-prefixed string: ",@OFFSET:TYPE"
    template<typename T, std::size_t Index, std::size_t OffsetAdj>
    consteval auto physical_field_with_comma() noexcept {
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
                   TypeSignature<FieldType, SignatureMode::Physical>::calculate() +
                   CompileString{">"};
        } else {
            constexpr std::size_t offset = offset_of(member).bytes + OffsetAdj;

            return CompileString{",@"} +
                   CompileString<number_buffer_size>::from_number(offset) +
                   CompileString{":"} +
                   TypeSignature<FieldType, SignatureMode::Physical>::calculate();
        }
    }

    /// Emit all direct fields of T with offset adjustment (comma-prefixed each).
    template <typename T, std::size_t OffsetAdj, std::size_t... Indices>
    consteval auto physical_direct_fields_prefixed(std::index_sequence<Indices...>) noexcept {
        if constexpr (sizeof...(Indices) == 0) {
            return CompileString{""};
        } else {
            return (physical_field_with_comma<T, Indices, OffsetAdj>() + ...);
        }
    }

    // Forward declaration for mutual recursion
    template <typename T, std::size_t OffsetAdj>
    consteval auto physical_all_prefixed() noexcept;

    /// Recursively flatten one non-virtual base's fields.
    template <typename T, std::size_t BaseIndex, std::size_t OffsetAdj>
    consteval auto physical_one_base_prefixed() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[BaseIndex];

        if constexpr (is_virtual(base_info)) {
            // v1 limitation: skip virtual bases during flattening.
            // Their fields are not included in the physical flat view.
            // The type's size/alignment still accounts for them.
            return CompileString{""};
        } else {
            using BaseType = [:type_of(base_info):];
            constexpr std::size_t abs_offset = offset_of(base_info).bytes + OffsetAdj;
            return physical_all_prefixed<BaseType, abs_offset>();
        }
    }

    /// Emit all bases' flattened fields (comma-prefixed each).
    template <typename T, std::size_t OffsetAdj, std::size_t... BaseIndices>
    consteval auto physical_bases_prefixed(std::index_sequence<BaseIndices...>) noexcept {
        if constexpr (sizeof...(BaseIndices) == 0) {
            return CompileString{""};
        } else {
            return (physical_one_base_prefixed<T, BaseIndices, OffsetAdj>() + ...);
        }
    }

    /// Collect ALL flattened fields (recurse into bases, then direct fields).
    /// Every field is comma-prefixed. Result may start with ',' or be empty.
    template <typename T, std::size_t OffsetAdj>
    consteval auto physical_all_prefixed() noexcept {
        constexpr std::size_t base_count = get_base_count<T>();
        constexpr std::size_t field_count = get_member_count<T>();

        if constexpr (base_count == 0 && field_count == 0) {
            return CompileString{""};
        } else if constexpr (base_count == 0) {
            return physical_direct_fields_prefixed<T, OffsetAdj>(
                std::make_index_sequence<field_count>{});
        } else if constexpr (field_count == 0) {
            return physical_bases_prefixed<T, OffsetAdj>(
                std::make_index_sequence<base_count>{});
        } else {
            return physical_bases_prefixed<T, OffsetAdj>(
                       std::make_index_sequence<base_count>{}) +
                   physical_direct_fields_prefixed<T, OffsetAdj>(
                       std::make_index_sequence<field_count>{});
        }
    }

    /// Top-level entry: get flattened physical content, strip leading comma.
    template <typename T>
    consteval auto get_physical_content() noexcept {
        constexpr auto prefixed = physical_all_prefixed<T, 0>();
        return prefixed.skip_first();
    }

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_REFLECTION_HELPERS_HPP
