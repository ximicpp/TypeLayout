// layout_traits<T> -- Compile-time type layout descriptor.
// Aggregates signature + structural properties (has_pointer, has_padding, etc.).
// Requires P2996 for struct/class types.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_LAYOUT_TRAITS_HPP
#define BOOST_TYPELAYOUT_LAYOUT_TRAITS_HPP

#include <boost/typelayout/signature.hpp>
#include <boost/typelayout/detail/reflect.hpp>
#include <boost/typelayout/detail/sig_parser.hpp>
#include <array>

namespace boost {
namespace typelayout {
inline namespace v1 {

// =========================================================================
// layout_traits<T>
// =========================================================================

namespace detail {

// Signature-scanning helpers (token-boundary-aware to avoid false positives).

template <typename Sig>
consteval bool sig_has_pointer(const Sig& sig) noexcept {
    return sig.contains_token(FixedString{"ptr["}) ||
           sig.contains_token(FixedString{"fnptr["}) ||
           sig.contains_token(FixedString{"memptr["}) ||
           sig.contains_token(FixedString{"ref["}) ||
           sig.contains_token(FixedString{"rref["});
}

// Recursively check whether T (or any nested member/base) is opaque.
template <typename T>
consteval bool type_has_opaque() noexcept;

template <typename T, std::size_t I, std::size_t N>
consteval bool any_member_has_opaque() noexcept {
    if constexpr (I >= N) {
        return false;
    } else {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[I];
        using FieldType = [:type_of(member):];
        if constexpr (type_has_opaque<FieldType>()) {
            return true;
        } else {
            return any_member_has_opaque<T, I + 1, N>();
        }
    }
}

template <typename T, std::size_t I, std::size_t N>
consteval bool any_base_has_opaque() noexcept {
    if constexpr (I >= N) {
        return false;
    } else {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[I];
        using BaseType = [:type_of(base_info):];
        if constexpr (type_has_opaque<BaseType>()) {
            return true;
        } else {
            return any_base_has_opaque<T, I + 1, N>();
        }
    }
}

template <typename T>
consteval bool type_has_opaque() noexcept {
    if constexpr (has_opaque_signature<T>) {
        return true;
    } else if constexpr (std::is_array_v<T>) {
        return type_has_opaque<std::remove_all_extents_t<T>>();
    } else if constexpr (std::is_class_v<T> && !std::is_union_v<T>) {
        constexpr std::size_t fc = get_member_count<T>();
        constexpr std::size_t bc = get_base_count<T>();
        return any_base_has_opaque<T, 0, bc>() ||
               any_member_has_opaque<T, 0, fc>();
    } else {
        return false;
    }
}

// ---- Byte coverage bitmap for has_padding detection ----
// Mirrors the signature engine's flattening to mark which bytes are
// covered by leaf data fields.  Any uncovered byte is padding.

template <std::size_t ArrSize>
consteval void mark_byte_range(std::array<bool, ArrSize>& covered,
                               std::size_t offset, std::size_t size) noexcept {
    for (std::size_t b = offset; b < offset + size && b < ArrSize; ++b)
        covered[b] = true;
}

template <std::size_t ArrSize, typename T, std::size_t OffsetAdj>
consteval void mark_type_coverage(std::array<bool, ArrSize>& covered) noexcept;

template <std::size_t ArrSize, typename T, std::size_t I, std::size_t N, std::size_t OffsetAdj>
consteval void mark_member_coverage(std::array<bool, ArrSize>& covered) noexcept {
    if constexpr (I < N) {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[I];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            // Mark the full storage unit (sizeof(FieldType)), not sub-byte span.
            constexpr std::size_t off = offset_of(member).bytes + OffsetAdj;
            mark_byte_range(covered, off, sizeof(FieldType));
        } else if constexpr (std::is_class_v<FieldType> && !std::is_union_v<FieldType>
                             && !has_opaque_signature<FieldType>
                             && !std::is_empty_v<FieldType>) {
            constexpr std::size_t field_offset = offset_of(member).bytes + OffsetAdj;
            mark_type_coverage<ArrSize, FieldType, field_offset>(covered);
        } else if constexpr (std::is_empty_v<FieldType>) {
            // Empty member: 0 bytes in host layout, skip.
        } else {
            constexpr std::size_t off = offset_of(member).bytes + OffsetAdj;
            mark_byte_range(covered, off, sizeof(FieldType));
        }
        mark_member_coverage<ArrSize, T, I + 1, N, OffsetAdj>(covered);
    }
}

template <std::size_t ArrSize, typename T, std::size_t I, std::size_t N, std::size_t OffsetAdj>
consteval void mark_base_coverage(std::array<bool, ArrSize>& covered) noexcept {
    if constexpr (I < N) {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[I];
        using BaseType = [:type_of(base_info):];

        if constexpr (std::is_empty_v<BaseType>) {
            // Empty base (EBO): 0 bytes, skip.
        } else if constexpr (has_opaque_signature<BaseType>) {
            constexpr std::size_t off = offset_of(base_info).bytes + OffsetAdj;
            mark_byte_range(covered, off, sizeof(BaseType));
        } else {
            constexpr std::size_t base_offset = offset_of(base_info).bytes + OffsetAdj;
            mark_type_coverage<ArrSize, BaseType, base_offset>(covered);
        }
        mark_base_coverage<ArrSize, T, I + 1, N, OffsetAdj>(covered);
    }
}

template <std::size_t ArrSize, typename T, std::size_t OffsetAdj>
consteval void mark_type_coverage(std::array<bool, ArrSize>& covered) noexcept {
    constexpr std::size_t bc = get_base_count<T>();
    constexpr std::size_t fc = get_member_count<T>();
    mark_base_coverage<ArrSize, T, 0, bc, OffsetAdj>(covered);
    mark_member_coverage<ArrSize, T, 0, fc, OffsetAdj>(covered);
}

template <typename T>
consteval bool compute_has_padding() noexcept;

// Check whether any array-typed member of T has an element type with padding.
// The bitmap treats arrays as atomic, so internal element padding is checked here.
template <typename T, std::size_t I, std::size_t N>
consteval bool any_member_array_elem_has_padding() noexcept {
    if constexpr (I >= N) {
        return false;
    } else {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[I];
        using FieldType = [:type_of(member):];
        if constexpr (std::is_array_v<FieldType>) {
            using ElemType = std::remove_all_extents_t<FieldType>;
            if constexpr (compute_has_padding<ElemType>()) {
                return true;
            }
        }
        return any_member_array_elem_has_padding<T, I + 1, N>();
    }
}

// Same check for array members inherited from base classes.
template <typename T, std::size_t I, std::size_t N>
consteval bool any_base_array_elem_has_padding() noexcept {
    if constexpr (I >= N) {
        return false;
    } else {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[I];
        using BaseType = [:type_of(base_info):];
        if constexpr (!std::is_empty_v<BaseType> && !has_opaque_signature<BaseType>) {
            constexpr std::size_t base_fc = get_member_count<BaseType>();
            constexpr std::size_t base_bc = get_base_count<BaseType>();
            if constexpr (any_member_array_elem_has_padding<BaseType, 0, base_fc>() ||
                          any_base_array_elem_has_padding<BaseType, 0, base_bc>()) {
                return true;
            }
        }
        return any_base_array_elem_has_padding<T, I + 1, N>();
    }
}

// Compute has_padding via byte coverage bitmap analysis.
// Handles EBO, [[no_unique_address]], bit-fields, nested structs.
template <typename T>
consteval bool compute_has_padding() noexcept {
    if constexpr (std::is_fundamental_v<T> || std::is_pointer_v<T> ||
                  std::is_enum_v<T> || std::is_member_pointer_v<T>) {
        return false;
    } else if constexpr (std::is_empty_v<T>) {
        return false;
    } else if constexpr (has_opaque_signature<T>) {
        return false;  // Opaque: no internal structure exposed
    } else if constexpr (std::is_class_v<T> && !std::is_union_v<T>) {
        constexpr std::size_t fc = get_member_count<T>();
        constexpr std::size_t bc = get_base_count<T>();
        if constexpr (fc == 0 && bc == 0) {
            return sizeof(T) > 1;  // Static-only class
        } else {
            std::array<bool, sizeof(T)> covered{};
            mark_type_coverage<sizeof(T), T, 0>(covered);
            for (std::size_t i = 0; i < sizeof(T); ++i) {
                if (!covered[i]) return true;
            }
            return any_member_array_elem_has_padding<T, 0, fc>() ||
                   any_base_array_elem_has_padding<T, 0, bc>();
        }
    } else {
        return false;  // Unions: padding is semantically ambiguous
    }
}

} // namespace detail

namespace detail {

template <typename T>
struct layout_traits {
    static constexpr auto signature = get_layout_signature<T>();

    static constexpr bool has_pointer = []() consteval {
        if constexpr (requires { TypeSignature<std::remove_cv_t<T>>::pointer_free; }) {
            // Opaque type with explicit user assertion
            return !TypeSignature<std::remove_cv_t<T>>::pointer_free;
        } else {
            return detail::sig_has_pointer(signature);
        }
    }();

    static constexpr bool has_opaque =
        detail::type_has_opaque<T>();

    static constexpr bool has_padding =
        detail::compute_has_padding<T>();

    // Cross-validation: bitmap has_padding must agree with sig_has_padding.
    // Skip for opaque types and types containing opaque members (has_opaque).
    // When any nested member is opaque, the ENTIRE struct's cross-validation
    // is skipped because the bitmap treats opaque fields as atomic blobs while
    // the signature may embed sub-structure (e.g. container element types via
    // TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE) that contain padding markers.
    static_assert(
        has_opaque_signature<T> || has_opaque ||
        !(std::is_class_v<T> && !std::is_union_v<T> && !std::is_empty_v<T>) ||
        detail::check_padding_consistency(has_padding,
                                          std::string_view(signature)),
        "layout_traits cross-validation failure: compile-time has_padding "
        "disagrees with runtime sig_has_padding. This indicates a bug in "
        "the coverage bitmap or the signature parser.");

    static constexpr std::size_t field_count = []() consteval {
        if constexpr (std::is_class_v<T> || std::is_union_v<T>) {
            return get_member_count<T>();
        } else {
            return std::size_t{0};
        }
    }();

    static constexpr std::size_t total_size = sizeof(T);
    static constexpr std::size_t alignment = alignof(T);

};

} // namespace detail

} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_LAYOUT_TRAITS_HPP
