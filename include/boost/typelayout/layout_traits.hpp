// layout_traits<T> -- Unified compile-time type layout descriptor.
//
// Aggregates the layout signature with zero-cost by-product properties
// derived from the signature string.  All computations are constexpr/
// consteval and incur no runtime overhead.
//
// This is the recommended entry point for users who need both the
// signature and structural properties of a type.  Users who only need
// the raw signature string can use get_layout_signature<T>() directly.
//
// Requires P2996 (Bloomberg Clang) for struct/class types.
// Fundamental types and opaque-registered types work without P2996.
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

// =========================================================================
// layout_traits<T>
//
// Primary product:
//   signature          -- the full layout signature string (FixedString)
//
// Natural by-products (derived from scanning the signature):
//   has_pointer         -- scans for ptr[, fnptr[, etc.
//   has_bit_field       -- scans for bits<
//   is_platform_variant -- scans for wchar[, f80[
//
// Reflection-derived by-products (NOT from the signature string, but
// use the same recursive flattening model as the signature engine):
//   has_opaque          -- recursive type-level concept check via reflection
//   has_padding         -- byte coverage bitmap analysis via reflection
//                          (mirrors the signature engine's flattening to
//                          detect uncovered bytes in the layout)
//
// Structural metadata (from reflection, zero cost):
//   field_count         -- number of direct non-static data members (no bases)
//   total_size          -- sizeof(T)
//   alignment           -- alignof(T)
// =========================================================================

namespace detail {

// Signature-scanning helpers.  These operate on the FixedString produced
// by get_layout_signature<T>() and use token-boundary-aware matching
// to avoid false positives (e.g. "nullptr[" must not match "ptr[").

template <typename Sig>
consteval bool sig_has_pointer(const Sig& sig) noexcept {
    return sig.contains_token(FixedString{"ptr["}) ||
           sig.contains_token(FixedString{"fnptr["}) ||
           sig.contains_token(FixedString{"memptr["}) ||
           sig.contains_token(FixedString{"ref["}) ||
           sig.contains_token(FixedString{"rref["});
}

template <typename Sig>
consteval bool sig_has_bit_field(const Sig& sig) noexcept {
    return sig.contains(FixedString{"bits<"});
}

template <typename Sig>
consteval bool sig_has_platform_variant(const Sig& sig) noexcept {
    // Platform-variant primitive types only (NOT pointers/references).
    // Pointers and references are platform-variant in size (32 vs 64 bit),
    // but their more actionable risk is PointerRisk (dangling after memcpy).
    // sig_has_pointer() already detects those; we do not duplicate them here
    // to avoid masking the higher-priority PointerRisk classification.
    //
    // wchar_t: 2 bytes on Windows, 4 bytes on Linux/macOS
    // long double (f80): 80-bit on x86 Linux, 64-bit on ARM/MSVC
    return sig.contains(FixedString{"wchar["}) ||
           sig.contains(FixedString{"f80["});
}

// Detect opaque sub-types by looking for opaque-style markers.
// Opaque signatures use a "name[s:N,a:M]" pattern where `name` is a
// user-chosen label.  Since this pattern is identical to primitive type
// markers, we detect opaqueness at the type level via the concept check.
//
// Recursively inspects class members: if any member's type (or any
// member's member, transitively) is registered as opaque, the
// containing type is considered to have opaque sub-types.
template <typename T>
consteval bool type_has_opaque() noexcept;

// Helper: check whether any member at index I..N-1 of class T
// has an opaque sub-type (direct or nested).
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

// Helper: check whether any base class at index I..N-1 of class T
// is (or transitively contains) an opaque sub-type.
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
        // Strip all array dimensions and check the element type.
        // OpaqueType[3] and OpaqueType[3][4] both reach the base type.
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
//
// These helpers mirror the signature engine's flattening logic
// (signature_impl.hpp) but instead of building a string, they mark
// which bytes in [0, sizeof(T)) are covered by leaf data fields.
// Any uncovered byte is padding.

// Helper: mark a byte range [offset, offset+size) as covered.
template <std::size_t ArrSize>
consteval void mark_byte_range(std::array<bool, ArrSize>& covered,
                               std::size_t offset, std::size_t size) noexcept {
    for (std::size_t b = offset; b < offset + size && b < ArrSize; ++b)
        covered[b] = true;
}

// Forward declaration: mark coverage for all bases and members of T,
// with T placed at byte offset OffsetAdj in the outermost struct.
template <std::size_t ArrSize, typename T, std::size_t OffsetAdj>
consteval void mark_type_coverage(std::array<bool, ArrSize>& covered) noexcept;

// Mark coverage for member I of class T.
template <std::size_t ArrSize, typename T, std::size_t I, std::size_t N, std::size_t OffsetAdj>
consteval void mark_member_coverage(std::array<bool, ArrSize>& covered) noexcept {
    if constexpr (I < N) {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[I];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            // Bit-field: mark the full underlying storage unit.
            //
            // We use sizeof(FieldType) -- the size of the bit-field's
            // declared type -- rather than computing a sub-byte span from
            // bit_size_of().  This is correct because:
            //
            //   1. The compiler allocates a storage unit whose size equals
            //      sizeof(FieldType) for each bit-field (or group of
            //      adjacent bit-fields that share a unit).
            //   2. Multiple bit-fields may share the same storage unit at
            //      the same byte offset; mark_byte_range handles overlaps
            //      idempotently (bitmap OR).
            //   3. Using (bit_offset + bit_width + 7) / 8 instead would
            //      under-count coverage, incorrectly flagging the unused
            //      tail of the storage unit as padding.
            //
            // The runtime signature parser (sig_has_padding) applies the
            // same logic by extracting [s:N] from the embedded underlying
            // type descriptor inside the bits<...> entry.
            constexpr std::size_t off = offset_of(member).bytes + OffsetAdj;
            mark_byte_range(covered, off, sizeof(FieldType));
        } else if constexpr (std::is_class_v<FieldType> && !std::is_union_v<FieldType>
                             && !has_opaque_signature<FieldType>
                             && !std::is_empty_v<FieldType>) {
            // Non-opaque, non-empty class: recursively flatten
            // (same decision as signature engine).
            constexpr std::size_t field_offset = offset_of(member).bytes + OffsetAdj;
            mark_type_coverage<ArrSize, FieldType, field_offset>(covered);
        } else if constexpr (std::is_empty_v<FieldType>) {
            // Empty member (possibly [[no_unique_address]]): the compiler
            // may overlay it at the same offset as another field, occupying
            // 0 extra bytes.  sizeof(Empty) == 1 by the C++ standard, but
            // marking that byte would over-report coverage.  Skip it.
            // (no-op)
        } else {
            // Leaf node: primitive, union, enum, or opaque.
            constexpr std::size_t off = offset_of(member).bytes + OffsetAdj;
            mark_byte_range(covered, off, sizeof(FieldType));
        }
        mark_member_coverage<ArrSize, T, I + 1, N, OffsetAdj>(covered);
    }
}

// Mark coverage for base class I of class T.
template <std::size_t ArrSize, typename T, std::size_t I, std::size_t N, std::size_t OffsetAdj>
consteval void mark_base_coverage(std::array<bool, ArrSize>& covered) noexcept {
    if constexpr (I < N) {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[I];
        using BaseType = [:type_of(base_info):];

        if constexpr (std::is_empty_v<BaseType>) {
            // Empty base: EBO makes it occupy 0 bytes in the derived
            // class layout, so we mark nothing.  sizeof(Empty) == 1 by
            // the C++ standard, but that byte is shared with the first
            // data member when EBO is active.  Marking it would
            // over-report coverage and mask real padding at offset 0.
            // (no-op)
        } else if constexpr (has_opaque_signature<BaseType>) {
            // Opaque base: treat as leaf node sized sizeof(BaseType).
            constexpr std::size_t off = offset_of(base_info).bytes + OffsetAdj;
            mark_byte_range(covered, off, sizeof(BaseType));
        } else {
            // Non-empty, non-opaque base: recursively flatten.
            constexpr std::size_t base_offset = offset_of(base_info).bytes + OffsetAdj;
            mark_type_coverage<ArrSize, BaseType, base_offset>(covered);
        }
        mark_base_coverage<ArrSize, T, I + 1, N, OffsetAdj>(covered);
    }
}

// Process all bases and members of T at offset OffsetAdj.
template <std::size_t ArrSize, typename T, std::size_t OffsetAdj>
consteval void mark_type_coverage(std::array<bool, ArrSize>& covered) noexcept {
    constexpr std::size_t bc = get_base_count<T>();
    constexpr std::size_t fc = get_member_count<T>();
    mark_base_coverage<ArrSize, T, 0, bc, OffsetAdj>(covered);
    mark_member_coverage<ArrSize, T, 0, fc, OffsetAdj>(covered);
}

// Forward declaration needed for mutual recursion between
// compute_has_padding and any_member_array_elem_has_padding.
template <typename T>
consteval bool compute_has_padding() noexcept;

// Check whether any direct member of T that is an array type has an element
// type with internal padding.  This detects the case where the byte coverage
// bitmap (which treats arrays as atomic leaf nodes) would miss padding inside
// array elements, e.g. struct Foo { PaddedStruct arr[2]; }.
//
// The bitmap correctly identifies NO outer gap in Foo (the array field covers
// all sizeof(Foo) bytes), but the element type PaddedStruct itself has padding.
// This helper catches that by recursively calling compute_has_padding on the
// base element type of every array field.
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

// Check whether any base class of T (at index I..N-1) contains an array
// member whose element type has internal padding.  This complements
// any_member_array_elem_has_padding which only checks T's direct members.
//
// Without this, a pattern like:
//   struct Base { PaddedStruct arr[2]; };
//   struct Derived : Base { int x; };
// would be missed: the bitmap marks the inherited array as atomic (all
// bytes covered), and any_member_array_elem_has_padding only iterates
// Derived's direct members, skipping Base::arr.
//
// This helper recurses into each non-empty, non-opaque base class and
// checks its members (and its bases, transitively) for array-element
// padding via compute_has_padding on the base type itself.  Since
// compute_has_padding already calls both any_member_array_elem_has_padding
// and any_base_array_elem_has_padding, the full inheritance tree is covered.
template <typename T, std::size_t I, std::size_t N>
consteval bool any_base_array_elem_has_padding() noexcept {
    if constexpr (I >= N) {
        return false;
    } else {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[I];
        using BaseType = [:type_of(base_info):];
        if constexpr (!std::is_empty_v<BaseType> && !has_opaque_signature<BaseType>) {
            // Check the base's own members and bases for array-element padding.
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

// Compute has_padding for type T using byte coverage analysis.
//
// Creates a bool bitmap of sizeof(T) bytes, recursively marks every
// byte that is covered by a leaf data field (using the same flattening
// logic as the signature engine), and returns true if any byte is
// uncovered -- i.e. the compiler inserted alignment padding.
//
// This correctly handles EBO, [[no_unique_address]], bit-fields, and
// nested struct internal padding (which the old sizeof-summation
// approach could not).
//
// Array fields are treated as atomic by the bitmap (the full array byte
// range is marked covered), but any_member_array_elem_has_padding is
// called afterward to detect padding inside array element types.
template <typename T>
consteval bool compute_has_padding() noexcept {
    if constexpr (std::is_fundamental_v<T> || std::is_pointer_v<T> ||
                  std::is_enum_v<T> || std::is_member_pointer_v<T>) {
        return false;  // Scalar types have no padding
    } else if constexpr (std::is_empty_v<T>) {
        return false;  // Empty classes have no padding
    } else if constexpr (has_opaque_signature<T>) {
        // Opaque types are treated as atomic leaf nodes -- the signature
        // does not expose internal structure, so we cannot (and should
        // not) determine padding from the bitmap.  Return false to stay
        // consistent with sig_has_padding, which only detects padding
        // inside "record[...]" signatures.
        return false;
    } else if constexpr (std::is_class_v<T> && !std::is_union_v<T>) {
        constexpr std::size_t fc = get_member_count<T>();
        constexpr std::size_t bc = get_base_count<T>();
        if constexpr (fc == 0 && bc == 0) {
            // Class with only static members: the mandatory 1-byte
            // minimum for addressability is not considered padding.
            return sizeof(T) > 1;
        } else {
            std::array<bool, sizeof(T)> covered{};
            mark_type_coverage<sizeof(T), T, 0>(covered);
            for (std::size_t i = 0; i < sizeof(T); ++i) {
                if (!covered[i]) return true;
            }
            // The bitmap treats array fields as atomic leaf nodes, so it
            // cannot detect padding inside array element types.  Check
            // each array member's element type separately, including
            // arrays inherited from base classes.
            return any_member_array_elem_has_padding<T, 0, fc>() ||
                   any_base_array_elem_has_padding<T, 0, bc>();
        }
    } else {
        // Union types: all members share offset 0, so "padding" in a union
        // is semantically ambiguous — the unused bytes depend on which member
        // is active at runtime.  We return false to match sig_has_padding
        // (which only parses "record[s:..." blocks).  Users dealing with
        // information-leakage scenarios should manually audit union-containing
        // types.
        return false;
    }
}

} // namespace detail

template <typename T>
struct layout_traits {
    // =================================================================
    // Primary product: signature
    // =================================================================

    /// The full layout signature string, including architecture prefix.
    static constexpr auto signature = get_layout_signature<T>();

    // =================================================================
    // Natural by-products: derived from the signature
    // =================================================================

    /// True if the type contains pointer-like fields (ptr, fnptr, memptr,
    /// ref, rref).  Such types cannot be safely memcpy'd across process
    /// boundaries because pointer values are address-space-specific.
    ///
    /// For opaque types registered via TYPELAYOUT_REGISTER_OPAQUE, this
    /// is determined by the user's pointer_free assertion (inverted).
    /// For all other types, it is detected by scanning the signature.
    static constexpr bool has_pointer = []() consteval {
        if constexpr (requires { TypeSignature<std::remove_cv_t<T>>::pointer_free; }) {
            // Opaque type with explicit user assertion
            return !TypeSignature<std::remove_cv_t<T>>::pointer_free;
        } else {
            return detail::sig_has_pointer(signature);
        }
    }();

    /// True if the type contains bit-fields.  Bit-field layout is not
    /// standardized across compilers and is a portability risk.
    static constexpr bool has_bit_field =
        detail::sig_has_bit_field(signature);

    /// True if the type is registered as opaque (via TYPELAYOUT_OPAQUE_*).
    /// Opaque types have size/alignment identity but no internal structural
    /// identity -- the user is responsible for ensuring internal consistency.
    static constexpr bool has_opaque =
        detail::type_has_opaque<T>();

    /// True if the type's layout may differ across platforms due to
    /// platform-dependent types (wchar_t, long double, pointers).
    static constexpr bool is_platform_variant =
        detail::sig_has_platform_variant(signature);

    /// True if the type has padding bytes (uncovered bytes in the layout).
    /// Padding bytes may contain uninitialized data, posing an information
    /// leakage risk in serialization scenarios.
    ///
    /// Uses byte coverage analysis with P2996 reflection: recursively
    /// flattens the type (same model as the signature engine) and checks
    /// whether every byte in [0, sizeof(T)) is covered by a leaf field.
    /// Correctly handles EBO, [[no_unique_address]], bit-fields, and
    /// nested struct internal padding.
    static constexpr bool has_padding =
        detail::compute_has_padding<T>();

    // ----- Cross-validation: compile-time bitmap vs runtime sig parse -----
    // Ensures that the byte-coverage analysis (has_padding above, which
    // mirrors the signature engine's flattening) agrees with the runtime
    // signature parser (sig_has_padding).  Any discrepancy indicates a
    // bug in one of the two paths.
    //
    // Guard: only for record types (non-empty, non-union classes) where
    // sig_has_padding can actually parse the outermost record block.
    // For non-record types both paths trivially return false.
    //
    // check_padding_consistency also passes when sig_has_padding_impl was
    // truncated (>512 leaf fields) -- in that case the signature parser
    // returns a conservative true that may not match the bitmap result,
    // and we trust the bitmap (which has no field-count limit).
    static_assert(
        !(std::is_class_v<T> && !std::is_union_v<T> && !std::is_empty_v<T>) ||
        detail::check_padding_consistency(has_padding,
                                          std::string_view(signature)),
        "layout_traits cross-validation failure: compile-time has_padding "
        "disagrees with runtime sig_has_padding. This indicates a bug in "
        "the coverage bitmap or the signature parser.");

    // =================================================================
    // Structural metadata (from sizeof/alignof and reflection)
    // =================================================================

    /// Number of direct non-static data members (does NOT include members
    /// inherited from base classes).  For the total flattened field count,
    /// sum field_count across the inheritance hierarchy manually.
    static constexpr std::size_t field_count = []() consteval {
        if constexpr (std::is_class_v<T> || std::is_union_v<T>) {
            return get_member_count<T>();
        } else {
            return std::size_t{0};
        }
    }();

    /// Total size in bytes (sizeof(T)).
    static constexpr std::size_t total_size = sizeof(T);

    /// Alignment requirement in bytes (alignof(T)).
    static constexpr std::size_t alignment = alignof(T);

};

// =========================================================================
// signature_compare -- compare layout signatures of two types
// =========================================================================

template <typename T, typename U>
struct signature_compare {
    static constexpr bool value =
        layout_traits<T>::signature == layout_traits<U>::signature;
};

template <typename T, typename U>
inline constexpr bool signature_compare_v = signature_compare<T, U>::value;

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_LAYOUT_TRAITS_HPP
