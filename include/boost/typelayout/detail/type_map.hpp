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
inline namespace v1 {

namespace detail {

    // =========================================================================
    // Helper: format "name[s:SIZE,a:ALIGN]"
    // =========================================================================

    template<size_t Size, size_t Align, size_t N>
    consteval auto format_size_align(const char (&name)[N]) noexcept {
        return FixedString{name} + FixedString{"[s:"} +
               to_fixed_string<Size>() +
               FixedString{",a:"} +
               to_fixed_string<Align>() +
               FixedString{"]"};
    }

    // =========================================================================
    // Fundamental types (only when distinct from fixed-width aliases)
    //
    // Types like long / long long may alias int32_t / int64_t.  The primary
    // template catch-all dispatches through fundamental_int_signature() for
    // types that are distinct from the fixed-width aliases above.
    // =========================================================================

    inline constexpr bool signed_char_is_int8    = std::is_same_v<signed char, int8_t>;
    inline constexpr bool unsigned_char_is_uint8  = std::is_same_v<unsigned char, uint8_t>;
    inline constexpr bool long_is_fixed           = std::is_same_v<long, int32_t> || std::is_same_v<long, int64_t>;
    inline constexpr bool ulong_is_fixed          = std::is_same_v<unsigned long, uint32_t> || std::is_same_v<unsigned long, uint64_t>;
    inline constexpr bool llong_is_int64          = std::is_same_v<long long, int64_t>;
    inline constexpr bool ullong_is_uint64        = std::is_same_v<unsigned long long, uint64_t>;

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

    template <typename T>
    inline constexpr bool is_distinct_fundamental_int_v =
        (std::is_same_v<T, signed char> && !signed_char_is_int8) ||
        (std::is_same_v<T, unsigned char> && !unsigned_char_is_uint8) ||
        (std::is_same_v<T, long> && !long_is_fixed) ||
        (std::is_same_v<T, unsigned long> && !ulong_is_fixed) ||
        (std::is_same_v<T, long long> && !llong_is_int64) ||
        (std::is_same_v<T, unsigned long long> && !ullong_is_uint64);

    // Byte-element types (size=1, align=1): arrays collapsed to "bytes[s:N,a:1]".
    template <typename T>
    [[nodiscard]] consteval bool is_byte_element() noexcept {
        return std::is_same_v<T, char> || std::is_same_v<T, signed char> ||
               std::is_same_v<T, unsigned char> || std::is_same_v<T, int8_t> ||
               std::is_same_v<T, uint8_t> || std::is_same_v<T, std::byte> ||
               std::is_same_v<T, char8_t>;
    }

    template <typename T>
    struct forward_signature {
        static consteval auto calculate() noexcept {
            return TypeSignature<T>::calculate();
        }
    };

    template <typename T>
    consteval auto fnptr_signature() noexcept {
        return format_size_align<sizeof(T), alignof(T)>("fnptr");
    }

} // namespace detail

#define BOOST_TYPELAYOUT_LITERAL_SIGNATURE(Type, Sig)                           \
    template <> struct TypeSignature<Type> {                                    \
        static consteval auto calculate() noexcept { return FixedString{Sig}; } \
    };

#define BOOST_TYPELAYOUT_FORMATTED_SIGNATURE(Type, Name)                                \
    template <> struct TypeSignature<Type> {                                             \
        static consteval auto calculate() noexcept {                                     \
            return detail::format_size_align<sizeof(Type), alignof(Type)>(Name);         \
        }                                                                                \
    };

    // =========================================================================
    // Fixed-width integers
    // =========================================================================

    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(int8_t, "i8[s:1,a:1]")
    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(uint8_t, "u8[s:1,a:1]")
    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(int16_t, "i16[s:2,a:2]")
    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(uint16_t, "u16[s:2,a:2]")
    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(int32_t, "i32[s:4,a:4]")
    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(uint32_t, "u32[s:4,a:4]")
    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(int64_t, "i64[s:8,a:8]")
    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(uint64_t, "u64[s:8,a:8]")

    // =========================================================================
    // Floating point
    // =========================================================================

    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(float, "f32[s:4,a:4]")
    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(double, "f64[s:8,a:8]")
    BOOST_TYPELAYOUT_FORMATTED_SIGNATURE(long double, BOOST_TYPELAYOUT_LONG_DOUBLE_TAG)

    // =========================================================================
    // Character types
    // =========================================================================

    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(char, "char[s:1,a:1]")
    BOOST_TYPELAYOUT_FORMATTED_SIGNATURE(wchar_t, "wchar")
    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(char8_t, "char8[s:1,a:1]")
    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(char16_t, "char16[s:2,a:2]")
    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(char32_t, "char32[s:4,a:4]")

    // =========================================================================
    // Other fundamentals
    // =========================================================================

    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(bool, "bool[s:1,a:1]")
    BOOST_TYPELAYOUT_FORMATTED_SIGNATURE(std::nullptr_t, "nullptr")
    BOOST_TYPELAYOUT_LITERAL_SIGNATURE(std::byte, "byte[s:1,a:1]")

    // =========================================================================
    // Function pointers
    // =========================================================================

    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args...)> {
        static consteval auto calculate() noexcept {
            return detail::fnptr_signature<R(*)(Args...)>();
        }
    };

    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args...) noexcept> {
        static consteval auto calculate() noexcept {
            return detail::fnptr_signature<R(*)(Args...) noexcept>();
        }
    };

    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args..., ...)> {
        static consteval auto calculate() noexcept {
            return detail::fnptr_signature<R(*)(Args..., ...)>();
        }
    };

    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args..., ...) noexcept> {
        static consteval auto calculate() noexcept {
            return detail::fnptr_signature<R(*)(Args..., ...) noexcept>();
        }
    };

    // =========================================================================
    // CV-qualified: strip and forward
    // =========================================================================

    template <typename T>
    struct TypeSignature<const T> : detail::forward_signature<T> {};
    template <typename T>
    struct TypeSignature<volatile T> : detail::forward_signature<T> {};
    template <typename T>
    struct TypeSignature<const volatile T> : detail::forward_signature<T> {};

    // =========================================================================
    // Pointers and references
    // =========================================================================

    template <typename T>
    struct TypeSignature<T*> {
        static consteval auto calculate() noexcept { return detail::format_size_align<sizeof(T*), alignof(T*)>("ptr"); }
    };
    template <typename T>
    struct TypeSignature<T&> {
        // References are stored as pointers; use sizeof(T*) for layout identity.
        static consteval auto calculate() noexcept { return detail::format_size_align<sizeof(T*), alignof(T*)>("ref"); }
    };
    template <typename T>
    struct TypeSignature<T&&> {
        static consteval auto calculate() noexcept { return detail::format_size_align<sizeof(T*), alignof(T*)>("rref"); }
    };
    template <typename T, typename C>
    struct TypeSignature<T C::*> {
        static consteval auto calculate() noexcept { return detail::format_size_align<sizeof(T C::*), alignof(T C::*)>("memptr"); }
    };

#undef BOOST_TYPELAYOUT_FORMATTED_SIGNATURE
#undef BOOST_TYPELAYOUT_LITERAL_SIGNATURE

    // =========================================================================
    // Arrays
    // =========================================================================

    template <typename T>
    struct TypeSignature<T[]> {
        static consteval auto calculate() noexcept {
            static_assert(detail::always_false<T>::value, "Unbounded array T[] has no defined size");
            return FixedString{""};
        }
    };

    template <typename T, size_t N>
    struct TypeSignature<T[N]> {
        static consteval auto calculate() noexcept {
            if constexpr (detail::is_byte_element<T>()) {
                return FixedString{"bytes[s:"} + to_fixed_string<N>() + FixedString{",a:1]"};
            } else {
                return FixedString{"array[s:"} + to_fixed_string<sizeof(T[N])>() +
                       FixedString{",a:"} + to_fixed_string<alignof(T[N])>() +
                       FixedString{"]<"} + TypeSignature<T>::calculate() +
                       FixedString{","} + to_fixed_string<N>() + FixedString{">"};
            }
        }
    };

    // =========================================================================
    // Primary template: structs, classes, enums, unions
    // =========================================================================

    template <typename T>
    struct TypeSignature {
        static consteval auto calculate() noexcept {
            if constexpr (detail::is_distinct_fundamental_int_v<T>) {
                return detail::fundamental_int_signature<T>();
            }
            else if constexpr (std::is_enum_v<T>) {
                using U = std::underlying_type_t<T>;
                return FixedString{"enum[s:"} +
                       to_fixed_string<sizeof(T)>() +
                       FixedString{",a:"} +
                       to_fixed_string<alignof(T)>() +
                       FixedString{"]<"} + TypeSignature<U>::calculate() + FixedString{">"};
            }
            else if constexpr (std::is_union_v<T>) {
                return FixedString{"union[s:"} + to_fixed_string<sizeof(T)>() +
                       FixedString{",a:"} + to_fixed_string<alignof(T)>() +
                       FixedString{"]{"} + detail::get_layout_union_content<T>() + FixedString{"}"};
            }
            else if constexpr (std::is_class_v<T>) {
                if constexpr (std::is_polymorphic_v<T>) {
                    // Polymorphic types have a hidden vptr.  Mark it as a flag
                    // in the record params (not as a field, since P2996 does not
                    // expose its offset).  sig_has_pointer scans for "vptr" token.
                    return FixedString{"record[s:"} +
                           to_fixed_string<sizeof(T)>() +
                           FixedString{",a:"} +
                           to_fixed_string<alignof(T)>() +
                           FixedString{",vptr]{"} +
                           detail::get_layout_content<T>() +
                           FixedString{"}"};
                } else {
                    return FixedString{"record[s:"} +
                           to_fixed_string<sizeof(T)>() +
                           FixedString{",a:"} +
                           to_fixed_string<alignof(T)>() +
                           FixedString{"]{"} +
                           detail::get_layout_content<T>() +
                           FixedString{"}"};
                }
            }
            else if constexpr (std::is_void_v<T>) {
                static_assert(detail::always_false<T>::value, "void has no layout; use void*");
                return FixedString{""};
            }
            else if constexpr (std::is_function_v<T>) {
                static_assert(detail::always_false<T>::value, "function types have no size; use function pointer");
                return FixedString{""};
            }
            else {
                static_assert(detail::always_false<T>::value, "unsupported type for layout signature");
                return FixedString{""};
            }
        }
    };

} // inline namespace v1
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_DETAIL_TYPE_MAP_HPP
