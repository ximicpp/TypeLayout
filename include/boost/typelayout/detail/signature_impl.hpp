// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
//
// =========================================================================
// Signature Grammar (BNF)
// =========================================================================
//
// A layout signature is a self-describing string encoding the byte-level
// identity of a C++ type.  The grammar below defines the syntax.
//
//   full-signature ::= arch-prefix type-signature
//
//   arch-prefix    ::= '[' pointer-bits '-' endianness ']'
//   pointer-bits   ::= '32' | '64'
//   endianness     ::= 'le' | 'be'
//
//   type-signature ::= leaf-signature | record-signature | union-signature
//                     | enum-signature | array-signature | opaque-signature
//
//   leaf-signature ::= type-kind '[' params ']'
//   type-kind      ::= 'i8' | 'i16' | 'i32' | 'i64'
//                     | 'u8' | 'u16' | 'u32' | 'u64'
//                     | 'f32' | 'f64' | 'fld64' | 'fld80' | 'fld106' | 'fld128'
//                     | 'char' | 'wchar' | 'char8' | 'char16' | 'char32'
//                     | 'bool' | 'byte' | 'nullptr'
//                     | 'ptr' | 'fnptr' | 'memptr' | 'ref' | 'rref'
//
//   params         ::= param (',' param)* (',' 'vptr')?
//   param          ::= key ':' value
//   key            ::= 's' | 'a'
//   value          ::= DIGIT+
//
//   record-signature ::= 'record' '[' params ']' '{' member-list '}'
//   union-signature  ::= 'union'  '[' params ']' '{' member-list '}'
//   member-list      ::= '' | member (',' member)*
//   member           ::= '@' offset ':' type-signature
//                       | '@' offset '.' bit-offset ':' bitfield-entry
//   offset           ::= DIGIT+
//   bit-offset       ::= DIGIT+
//   bitfield-entry   ::= 'bits<' bit-width ',' leaf-signature '>'
//   bit-width        ::= DIGIT+
//
//   enum-signature   ::= 'enum' '[' params ']' '<' type-signature '>'
//   array-signature  ::= 'array' '[' params ']' '<' type-signature ',' count '>'
//                       | 'bytes' '[' params ']'
//   count            ::= DIGIT+
//
//   opaque-signature ::= 'O(' TAG '|' size '|' alignment ')'
//   TAG              ::= [^|)]+
//   size             ::= DIGIT+
//   alignment        ::= DIGIT+
//
// Notes:
//   - Empty classes embedded as base (EBO) or [[no_unique_address]] member
//     use s:0 in the host signature.  Standalone signatures use s:1.
//   - DIGIT ::= [0-9]
//
// =========================================================================

#ifndef BOOST_TYPELAYOUT_DETAIL_SIGNATURE_IMPL_HPP
#define BOOST_TYPELAYOUT_DETAIL_SIGNATURE_IMPL_HPP

#include <boost/typelayout/detail/reflect.hpp>

namespace boost {
namespace typelayout {
inline namespace v1 {
namespace detail {

    // Detect opaque TypeSignature specializations (e.g. TYPELAYOUT_REGISTER_OPAQUE).
    template <typename T>
    concept has_opaque_signature = requires {
        { TypeSignature<std::remove_cv_t<T>>::is_opaque } -> std::convertible_to<bool>;
    } && TypeSignature<std::remove_cv_t<T>>::is_opaque;

    // Patch an empty type's signature from s:1 to s:0 for EBO /
    // [[no_unique_address]] contexts where it occupies 0 bytes.
    //
    // Implementation note: this uses string surgery on the "[s:N" portion
    // of the signature.  The format-specific static_asserts below verify
    // that the expected structure ("TYPE[s:N,a:M]...") holds.  If the
    // signature format is ever changed (e.g., new parameter inserted
    // before "s:"), these asserts will fire at compile time.
    template <typename T>
    consteval auto embedded_empty_signature() noexcept {
        static constexpr auto full = TypeSignature<T>::calculate();
        constexpr auto str = std::string_view(full);
        constexpr auto s_pos = str.find("[s:");
        static_assert(s_pos != std::string_view::npos,
            "embedded_empty_signature: signature must contain '[s:' "
            "(format: TYPE[s:SIZE,a:ALIGN]{...})");
        constexpr auto comma_pos = str.find(',', s_pos + 3);
        static_assert(comma_pos != std::string_view::npos,
            "embedded_empty_signature: expected ',a:' after size "
            "(format: TYPE[s:SIZE,a:ALIGN]{...})");
        // Verify the size being replaced is "1" (empty types have sizeof == 1).
        static_assert(str[s_pos + 3] == '1' && str[s_pos + 4] == ',',
            "embedded_empty_signature: expected s:1 for empty type; "
            "got unexpected size value -- check if sizeof(T) != 1");
        return FixedString<s_pos>(str.substr(0, s_pos)) +
               FixedString{"[s:0"} +
               FixedString<str.size() - comma_pos>(str.substr(comma_pos));
    }

    template <typename T, std::size_t OffsetAdj>
    consteval auto layout_all_prefixed() noexcept;

    template<typename T, std::size_t Index, std::size_t OffsetAdj>
    consteval auto layout_field_with_comma() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];

        // Bit-field: emit byte.bit offset + width + storage type signature
        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            constexpr std::size_t byte_pos = bit_off.bytes + OffsetAdj;
            constexpr std::size_t bit_pos  = bit_off.bits;
            constexpr std::size_t bwidth   = bit_size_of(member);
            return FixedString{",@"} +
                   to_fixed_string<byte_pos>() +
                   FixedString{"."} +
                   to_fixed_string<bit_pos>() +
                   FixedString{":bits<"} +
                   to_fixed_string<bwidth>() +
                   FixedString{","} +
                   TypeSignature<FieldType>::calculate() +
                   FixedString{">"};
        // Non-empty class (non-opaque): flatten recursively into parent offset space
        } else if constexpr (std::is_class_v<FieldType> && !std::is_union_v<FieldType>
                             && !has_opaque_signature<FieldType>
                             && !std::is_empty_v<FieldType>) {
            constexpr std::size_t field_offset = offset_of(member).bytes + OffsetAdj;
            return layout_all_prefixed<FieldType, field_offset>();
        // Empty class (EBO / [[no_unique_address]]): emit s:0 signature
        } else if constexpr (std::is_empty_v<FieldType>
                             && std::is_class_v<FieldType>
                             && !has_opaque_signature<FieldType>) {
            constexpr std::size_t emb_off = offset_of(member).bytes + OffsetAdj;
            return FixedString{",@"} +
                   to_fixed_string<emb_off>() +
                   FixedString{":"} +
                   embedded_empty_signature<FieldType>();
        // Leaf (scalar, array, opaque, union): emit type signature directly
        } else {
            constexpr std::size_t leaf_off = offset_of(member).bytes + OffsetAdj;
            return FixedString{",@"} +
                   to_fixed_string<leaf_off>() +
                   FixedString{":"} +
                   TypeSignature<FieldType>::calculate();
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
        if constexpr (std::is_empty_v<BaseType>) {
            constexpr std::size_t base_emb_off = offset_of(base_info).bytes + OffsetAdj;
            return FixedString{",@"} +
                   to_fixed_string<base_emb_off>() +
                   FixedString{":"} +
                   embedded_empty_signature<BaseType>();
        } else if constexpr (has_opaque_signature<BaseType>) {
            constexpr std::size_t base_opq_off = offset_of(base_info).bytes + OffsetAdj;
            return FixedString{",@"} +
                   to_fixed_string<base_opq_off>() +
                   FixedString{":"} +
                   TypeSignature<BaseType>::calculate();
        } else {
            return layout_all_prefixed<BaseType, offset_of(base_info).bytes + OffsetAdj>();
        }
    }

    template <typename T, std::size_t OffsetAdj, std::size_t... Is>
    consteval auto layout_bases_prefixed(std::index_sequence<Is...>) noexcept {
        if constexpr (sizeof...(Is) == 0) return FixedString{""};
        else return (layout_one_base_prefixed<T, Is, OffsetAdj>() + ...);
    }

    template <typename T, std::size_t OffsetAdj>
    consteval auto layout_all_prefixed() noexcept {
        static_assert(!has_virtual_base<T>(),
            "TypeLayout: virtual inheritance is not supported (hidden "
            "vbptrs, compiler-specific layout, diamond double-counting).");
        constexpr std::size_t bc = get_base_count<T>();
        constexpr std::size_t fc = get_member_count<T>();
        if constexpr (bc == 0 && fc == 0) return FixedString{""};
        else if constexpr (bc == 0) return layout_direct_fields_prefixed<T, OffsetAdj>(std::make_index_sequence<fc>{});
        else if constexpr (fc == 0) return layout_bases_prefixed<T, OffsetAdj>(std::make_index_sequence<bc>{});
        else return layout_bases_prefixed<T, OffsetAdj>(std::make_index_sequence<bc>{}) +
                    layout_direct_fields_prefixed<T, OffsetAdj>(std::make_index_sequence<fc>{});
    }

    template <typename T>
    consteval auto get_layout_content() noexcept {
        return layout_all_prefixed<T, 0>().skip_first();
    }

    // Union layout helpers (no flattening).

    template<typename T, std::size_t Index>
    consteval auto layout_union_field() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            constexpr std::size_t ubyte_pos = bit_off.bytes;
            constexpr std::size_t ubit_pos  = bit_off.bits;
            constexpr std::size_t ubwidth   = bit_size_of(member);
            return FixedString{"@"} +
                   to_fixed_string<ubyte_pos>() +
                   FixedString{"."} +
                   to_fixed_string<ubit_pos>() +
                   FixedString{":bits<"} +
                   to_fixed_string<ubwidth>() +
                   FixedString{","} +
                   TypeSignature<FieldType>::calculate() +
                   FixedString{">"};
        } else {
            constexpr std::size_t uf_off = offset_of(member).bytes;
            return FixedString{"@"} +
                   to_fixed_string<uf_off>() +
                   FixedString{":"} +
                   TypeSignature<FieldType>::calculate();
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

} // namespace detail
} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_DETAIL_SIGNATURE_IMPL_HPP