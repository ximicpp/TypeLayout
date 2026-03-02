// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
//
// TypeSignature specializations for fundamental types, pointers, arrays,
// CV-qualified types, and the primary template catch-all for structs,
// classes, enums, and unions.

#ifndef BOOST_TYPELAYOUT_DETAIL_TYPE_MAP_HPP
#define BOOST_TYPELAYOUT_DETAIL_TYPE_MAP_HPP

#include <boost/typelayout/detail/signature_impl.hpp>

namespace boost {
namespace typelayout {

    // =========================================================================
    // Helper: format "name[s:SIZE,a:ALIGN]"
    // =========================================================================

    template<size_t N>
    consteval auto format_size_align(const char (&name)[N], size_t size, size_t align) noexcept {
        return FixedString{name} + FixedString{"[s:"} +
               to_fixed_string(size) +
               FixedString{",a:"} +
               to_fixed_string(align) +
               FixedString{"]"};
    }

    // =========================================================================
    // Fixed-width integers
    // =========================================================================

    template <> struct TypeSignature<int8_t>   { static consteval auto calculate() noexcept { return FixedString{"i8[s:1,a:1]"}; } };
    template <> struct TypeSignature<uint8_t>  { static consteval auto calculate() noexcept { return FixedString{"u8[s:1,a:1]"}; } };
    template <> struct TypeSignature<int16_t>  { static consteval auto calculate() noexcept { return FixedString{"i16[s:2,a:2]"}; } };
    template <> struct TypeSignature<uint16_t> { static consteval auto calculate() noexcept { return FixedString{"u16[s:2,a:2]"}; } };
    template <> struct TypeSignature<int32_t>  { static consteval auto calculate() noexcept { return FixedString{"i32[s:4,a:4]"}; } };
    template <> struct TypeSignature<uint32_t> { static consteval auto calculate() noexcept { return FixedString{"u32[s:4,a:4]"}; } };
    template <> struct TypeSignature<int64_t>  { static consteval auto calculate() noexcept { return FixedString{"i64[s:8,a:8]"}; } };
    template <> struct TypeSignature<uint64_t> { static consteval auto calculate() noexcept { return FixedString{"u64[s:8,a:8]"}; } };

    // =========================================================================
    // Fundamental types (only when distinct from fixed-width aliases)
    //
    // C++ allows signed char / long / long long to be the same type as
    // int8_t / int32_t / int64_t on some platforms.  We must not provide
    // duplicate specializations in that case, so we use compile-time
    // type identity checks guarded by `if constexpr` inside a helper,
    // and then conditionally define the specialization via a constexpr
    // bool + requires on a *partial* specialization workaround.
    //
    // Because `template<> requires(...)` is ill-formed on an explicit
    // (full) specialization, we use a static dispatching approach
    // instead: the primary template's catch-all branch already handles
    // `long`, `long long`, etc. if they are not the same type as a
    // fixed-width alias, so we provide explicit specializations only
    // for the cases where the type IS distinct.
    // =========================================================================

    // -- signed char: on most platforms signed char != int8_t
    //    but on some they alias. Guard with a macro-free constexpr check.
    //    We use a wrapper helper to avoid ill-formed full-specialization +
    //    requires.

    // Inline constexpr bool helpers for type distinctness checks
    inline constexpr bool signed_char_is_int8    = std::is_same_v<signed char, int8_t>;
    inline constexpr bool unsigned_char_is_uint8  = std::is_same_v<unsigned char, uint8_t>;
    inline constexpr bool long_is_fixed           = std::is_same_v<long, int32_t> || std::is_same_v<long, int64_t>;
    inline constexpr bool ulong_is_fixed          = std::is_same_v<unsigned long, uint32_t> || std::is_same_v<unsigned long, uint64_t>;
    inline constexpr bool llong_is_int64          = std::is_same_v<long long, int64_t>;
    inline constexpr bool ullong_is_uint64        = std::is_same_v<unsigned long long, uint64_t>;

    // Helper: map any fundamental integer to its layout signature string.
    // This is used by the primary template catch-all to handle types like
    // long / long long that may or may not alias fixed-width types.
    template <typename T>
    consteval auto fundamental_int_signature() noexcept {
        if constexpr (std::is_same_v<T, signed char> && !signed_char_is_int8) {
            return FixedString{"i8[s:1,a:1]"};
        } else if constexpr (std::is_same_v<T, unsigned char> && !unsigned_char_is_uint8) {
            return FixedString{"u8[s:1,a:1]"};
        } else if constexpr (std::is_same_v<T, long> && !long_is_fixed) {
            if constexpr (sizeof(long) == 4) return FixedString{"i32[s:4,a:4]"};
            else return FixedString{"i64[s:8,a:8]"};
        } else if constexpr (std::is_same_v<T, unsigned long> && !ulong_is_fixed) {
            if constexpr (sizeof(unsigned long) == 4) return FixedString{"u32[s:4,a:4]"};
            else return FixedString{"u64[s:8,a:8]"};
        } else if constexpr (std::is_same_v<T, long long> && !llong_is_int64) {
            return FixedString{"i64[s:8,a:8]"};
        } else if constexpr (std::is_same_v<T, unsigned long long> && !ullong_is_uint64) {
            return FixedString{"u64[s:8,a:8]"};
        } else {
            static_assert(always_false<T>::value, "not a distinct fundamental integer");
            return FixedString{""};
        }
    }

    // Helper: check if T is a fundamental integer that is distinct from
    // all fixed-width aliases and needs special handling.
    template <typename T>
    inline constexpr bool is_distinct_fundamental_int_v =
        (std::is_same_v<T, signed char> && !signed_char_is_int8) ||
        (std::is_same_v<T, unsigned char> && !unsigned_char_is_uint8) ||
        (std::is_same_v<T, long> && !long_is_fixed) ||
        (std::is_same_v<T, unsigned long> && !ulong_is_fixed) ||
        (std::is_same_v<T, long long> && !llong_is_int64) ||
        (std::is_same_v<T, unsigned long long> && !ullong_is_uint64);

    // =========================================================================
    // Floating point
    // =========================================================================

    template <> struct TypeSignature<float>    { static consteval auto calculate() noexcept { return FixedString{"f32[s:4,a:4]"}; } };
    template <> struct TypeSignature<double>   { static consteval auto calculate() noexcept { return FixedString{"f64[s:8,a:8]"}; } };
    template <> struct TypeSignature<long double> {
        static consteval auto calculate() noexcept { return format_size_align("f80", sizeof(long double), alignof(long double)); }
    };

    // =========================================================================
    // Character types
    // =========================================================================

    template <> struct TypeSignature<char>     { static consteval auto calculate() noexcept { return FixedString{"char[s:1,a:1]"}; } };
    template <> struct TypeSignature<wchar_t>  { static consteval auto calculate() noexcept { return format_size_align("wchar", sizeof(wchar_t), alignof(wchar_t)); } };
    template <> struct TypeSignature<char8_t>  { static consteval auto calculate() noexcept { return FixedString{"char8[s:1,a:1]"}; } };
    template <> struct TypeSignature<char16_t> { static consteval auto calculate() noexcept { return FixedString{"char16[s:2,a:2]"}; } };
    template <> struct TypeSignature<char32_t> { static consteval auto calculate() noexcept { return FixedString{"char32[s:4,a:4]"}; } };

    // =========================================================================
    // Other fundamentals
    // =========================================================================

    template <> struct TypeSignature<bool>     { static consteval auto calculate() noexcept { return FixedString{"bool[s:1,a:1]"}; } };
    template <> struct TypeSignature<std::nullptr_t> { static consteval auto calculate() noexcept { return format_size_align("nullptr", sizeof(std::nullptr_t), alignof(std::nullptr_t)); } };
    template <> struct TypeSignature<std::byte> { static consteval auto calculate() noexcept { return FixedString{"byte[s:1,a:1]"}; } };

    // =========================================================================
    // Function pointers
    // =========================================================================

    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args...)> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args...)), alignof(R(*)(Args...)));
        }
    };

    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args...) noexcept> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args...) noexcept), alignof(R(*)(Args...) noexcept));
        }
    };

    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args..., ...)> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args..., ...)), alignof(R(*)(Args..., ...)));
        }
    };

    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args..., ...) noexcept> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args..., ...) noexcept), alignof(R(*)(Args..., ...) noexcept));
        }
    };

    // =========================================================================
    // CV-qualified: strip and forward
    // =========================================================================

    template <typename T>
    struct TypeSignature<const T> {
        static consteval auto calculate() noexcept { return TypeSignature<T>::calculate(); }
    };
    template <typename T>
    struct TypeSignature<volatile T> {
        static consteval auto calculate() noexcept { return TypeSignature<T>::calculate(); }
    };
    template <typename T>
    struct TypeSignature<const volatile T> {
        static consteval auto calculate() noexcept { return TypeSignature<T>::calculate(); }
    };

    // =========================================================================
    // Pointers and references
    // =========================================================================

    template <typename T>
    struct TypeSignature<T*> {
        static consteval auto calculate() noexcept { return format_size_align("ptr", sizeof(T*), alignof(T*)); }
    };
    template <typename T>
    struct TypeSignature<T&> {
        // sizeof(T&) == sizeof(T*) on all known platforms; use T& for formal consistency.
        static consteval auto calculate() noexcept { return format_size_align("ref", sizeof(T&), alignof(T&)); }
    };
    template <typename T>
    struct TypeSignature<T&&> {
        static consteval auto calculate() noexcept { return format_size_align("rref", sizeof(T&&), alignof(T&&)); }
    };
    template <typename T, typename C>
    struct TypeSignature<T C::*> {
        static consteval auto calculate() noexcept { return format_size_align("memptr", sizeof(T C::*), alignof(T C::*)); }
    };

    // =========================================================================
    // Arrays
    // =========================================================================

    template <typename T>
    struct TypeSignature<T[]> {
        static consteval auto calculate() noexcept {
            static_assert(always_false<T>::value, "Unbounded array T[] has no defined size");
            return FixedString{""};
        }
    };

    template <typename T>
    [[nodiscard]] consteval bool is_byte_element() noexcept {
        return std::is_same_v<T, char> || std::is_same_v<T, signed char> ||
               std::is_same_v<T, unsigned char> || std::is_same_v<T, int8_t> ||
               std::is_same_v<T, uint8_t> || std::is_same_v<T, std::byte> ||
               std::is_same_v<T, char8_t>;
    }

    template <typename T, size_t N>
    struct TypeSignature<T[N]> {
        static consteval auto calculate() noexcept {
            if constexpr (is_byte_element<T>()) {
                return FixedString{"bytes[s:"} + to_fixed_string(N) + FixedString{",a:1]"};
            } else {
                return FixedString{"array[s:"} + to_fixed_string(sizeof(T[N])) +
                       FixedString{",a:"} + to_fixed_string(alignof(T[N])) +
                       FixedString{"]<"} + TypeSignature<T>::calculate() +
                       FixedString{","} + to_fixed_string(N) + FixedString{">"};
            }
        }
    };

    // =========================================================================
    // Primary template: structs, classes, enums, unions
    // =========================================================================

    template <typename T>
    struct TypeSignature {
        static consteval auto calculate() noexcept {
            if constexpr (is_distinct_fundamental_int_v<T>) {
                return fundamental_int_signature<T>();
            }
            else if constexpr (std::is_enum_v<T>) {
                using U = std::underlying_type_t<T>;
                return FixedString{"enum[s:"} +
                       to_fixed_string(sizeof(T)) +
                       FixedString{",a:"} +
                       to_fixed_string(alignof(T)) +
                       FixedString{"]<"} + TypeSignature<U>::calculate() + FixedString{">"};
            }
            else if constexpr (std::is_union_v<T>) {
                return FixedString{"union[s:"} + to_fixed_string(sizeof(T)) +
                       FixedString{",a:"} + to_fixed_string(alignof(T)) +
                       FixedString{"]{"} + get_layout_union_content<T>() + FixedString{"}"};
            }
            else if constexpr (std::is_class_v<T> && !std::is_array_v<T>) {
                return FixedString{"record[s:"} +
                       to_fixed_string(sizeof(T)) +
                       FixedString{",a:"} +
                       to_fixed_string(alignof(T)) +
                       FixedString{"]{"} +
                       get_layout_content<T>() +
                       FixedString{"}"};
            }
            else if constexpr (std::is_void_v<T>) {
                static_assert(always_false<T>::value, "void has no layout; use void*");
                return FixedString{""};
            }
            else if constexpr (std::is_function_v<T>) {
                static_assert(always_false<T>::value, "function types have no size; use function pointer");
                return FixedString{""};
            }
            else {
                static_assert(always_false<T>::value, "unsupported type for layout signature");
                return FixedString{""};
            }
        }
    };

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_DETAIL_TYPE_MAP_HPP