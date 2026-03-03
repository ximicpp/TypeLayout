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

namespace boost {
namespace typelayout {

// =========================================================================
// layout_traits<T>
//
// Primary product:
//   signature          -- the full layout signature string (FixedString)
//
// Natural by-products (derived from scanning the signature, zero extra
// traversal cost):
//   has_pointer         -- contains ptr[, fnptr[, memptr[, ref[, or rref[
//   has_bit_field       -- contains bits<
//   has_opaque          -- contains an opaque-registered sub-type
//   is_platform_variant -- contains wchar[, f80[, or ptr[ (platform-dependent)
//   has_padding         -- sizeof(T) > sum of field sizes (detected via
//                          signature structure)
//
// Structural metadata (from reflection, zero cost):
//   field_count         -- number of non-static data members
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
    // wchar_t: 2 bytes on Windows, 4 bytes on Linux/macOS
    // long double (f80): 80-bit on x86 Linux, 64-bit on ARM/MSVC
    // Pointers (ptr, fnptr, memptr): size varies by bitness (32-bit vs 64-bit)
    // References (ref, rref): implemented as pointers, same platform variance
    return sig.contains(FixedString{"wchar["}) ||
           sig.contains(FixedString{"f80["}) ||
           sig.contains_token(FixedString{"ptr["}) ||
           sig.contains_token(FixedString{"fnptr["}) ||
           sig.contains_token(FixedString{"memptr["}) ||
           sig.contains_token(FixedString{"ref["}) ||
           sig.contains_token(FixedString{"rref["});
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

template <typename T>
consteval bool type_has_opaque() noexcept {
    if constexpr (has_opaque_signature<T>) {
        return true;
    } else if constexpr (std::is_class_v<T> && !std::is_union_v<T>) {
        constexpr std::size_t fc = get_member_count<T>();
        return any_member_has_opaque<T, 0, fc>();
    } else {
        return false;
    }
}

// Helper: recursively sum direct member sizes at compile time.
// Uses template recursion because P2996 splicing requires constexpr
// indices (for-loop variables are not constexpr).
template <typename T, std::size_t I, std::size_t N>
consteval std::size_t sum_member_sizes() noexcept {
    if constexpr (I >= N) {
        return 0;
    } else {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[I];
        if constexpr (is_bit_field(member)) {
            // Bit-fields do not contribute whole-byte sizes in the
            // simple summation model.  Skip them; the overall
            // sizeof(T) vs sum check will still catch padding.
            return sum_member_sizes<T, I + 1, N>();
        } else {
            using FieldType = [:type_of(member):];
            return sizeof(FieldType) + sum_member_sizes<T, I + 1, N>();
        }
    }
}

// Compute has_padding for type T using P2996 reflection.
//
// Returns true if sizeof(T) > sum of all direct member sizes,
// indicating that the compiler has inserted padding bytes.
template <typename T>
consteval bool compute_has_padding() noexcept {
    if constexpr (std::is_fundamental_v<T> || std::is_pointer_v<T> ||
                  std::is_enum_v<T> || std::is_member_pointer_v<T>) {
        return false;  // Scalar types have no padding
    } else if constexpr (std::is_empty_v<T>) {
        return false;  // Empty classes have no padding
    } else if constexpr (std::is_class_v<T> && !std::is_union_v<T>) {
        constexpr std::size_t fc = get_member_count<T>();
        if constexpr (fc == 0) {
            // Class with bases only, or truly empty with size > 0
            return sizeof(T) > 1;
        } else {
            constexpr std::size_t member_sum = sum_member_sizes<T, 0, fc>();
            return member_sum < sizeof(T);
        }
    } else {
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
        if constexpr (requires { TypeSignature<T>::pointer_free; }) {
            // Opaque type with explicit user assertion
            return !TypeSignature<T>::pointer_free;
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

    /// True if the type has padding bytes (sizeof > sum of member sizes).
    /// Padding bytes may contain uninitialized data, posing an information
    /// leakage risk in serialization scenarios.
    ///
    /// Uses P2996 reflection to sum all direct member sizes and compare
    /// against sizeof(T).  If the sum is less than sizeof(T), padding
    /// bytes exist (either inter-field alignment padding or trailing
    /// padding for struct alignment).
    static constexpr bool has_padding =
        detail::compute_has_padding<T>();

    // =================================================================
    // Structural metadata (from sizeof/alignof and reflection)
    // =================================================================

    /// Number of direct non-static data members.
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
