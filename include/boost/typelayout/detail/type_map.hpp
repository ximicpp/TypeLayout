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

    template <SignatureMode Mode> struct TypeSignature<int8_t, Mode>   { static consteval auto calculate() noexcept { return FixedString{"i8[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint8_t, Mode>  { static consteval auto calculate() noexcept { return FixedString{"u8[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<int16_t, Mode>  { static consteval auto calculate() noexcept { return FixedString{"i16[s:2,a:2]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint16_t, Mode> { static consteval auto calculate() noexcept { return FixedString{"u16[s:2,a:2]"}; } };
    template <SignatureMode Mode> struct TypeSignature<int32_t, Mode>  { static consteval auto calculate() noexcept { return FixedString{"i32[s:4,a:4]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint32_t, Mode> { static consteval auto calculate() noexcept { return FixedString{"u32[s:4,a:4]"}; } };
    template <SignatureMode Mode> struct TypeSignature<int64_t, Mode>  { static consteval auto calculate() noexcept { return FixedString{"i64[s:8,a:8]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint64_t, Mode> { static consteval auto calculate() noexcept { return FixedString{"u64[s:8,a:8]"}; } };

    // =========================================================================
    // Fundamental types (only when distinct from fixed-width aliases)
    // =========================================================================

    template <SignatureMode Mode>
        requires (!std::is_same_v<signed char, int8_t>)
    struct TypeSignature<signed char, Mode> {
        static consteval auto calculate() noexcept { return FixedString{"i8[s:1,a:1]"}; }
    };

    template <SignatureMode Mode>
        requires (!std::is_same_v<unsigned char, uint8_t>)
    struct TypeSignature<unsigned char, Mode> {
        static consteval auto calculate() noexcept { return FixedString{"u8[s:1,a:1]"}; }
    };

    template <SignatureMode Mode>
        requires (!std::is_same_v<long, int32_t> && !std::is_same_v<long, int64_t>)
    struct TypeSignature<long, Mode> {
        static consteval auto calculate() noexcept {
            if constexpr (sizeof(long) == 4) return FixedString{"i32[s:4,a:4]"};
            else return FixedString{"i64[s:8,a:8]"};
        }
    };

    template <SignatureMode Mode>
        requires (!std::is_same_v<unsigned long, uint32_t> && !std::is_same_v<unsigned long, uint64_t>)
    struct TypeSignature<unsigned long, Mode> {
        static consteval auto calculate() noexcept {
            if constexpr (sizeof(unsigned long) == 4) return FixedString{"u32[s:4,a:4]"};
            else return FixedString{"u64[s:8,a:8]"};
        }
    };

    template <SignatureMode Mode>
        requires (!std::is_same_v<long long, int64_t>)
    struct TypeSignature<long long, Mode> {
        static consteval auto calculate() noexcept { return FixedString{"i64[s:8,a:8]"}; }
    };

    template <SignatureMode Mode>
        requires (!std::is_same_v<unsigned long long, uint64_t>)
    struct TypeSignature<unsigned long long, Mode> {
        static consteval auto calculate() noexcept { return FixedString{"u64[s:8,a:8]"}; }
    };

    // =========================================================================
    // Floating point
    // =========================================================================

    template <SignatureMode Mode> struct TypeSignature<float, Mode>    { static consteval auto calculate() noexcept { return FixedString{"f32[s:4,a:4]"}; } };
    template <SignatureMode Mode> struct TypeSignature<double, Mode>   { static consteval auto calculate() noexcept { return FixedString{"f64[s:8,a:8]"}; } };
    template <SignatureMode Mode> struct TypeSignature<long double, Mode> {
        static consteval auto calculate() noexcept { return format_size_align("f80", sizeof(long double), alignof(long double)); }
    };

    // =========================================================================
    // Character types
    // =========================================================================

    template <SignatureMode Mode> struct TypeSignature<char, Mode>     { static consteval auto calculate() noexcept { return FixedString{"char[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<wchar_t, Mode>  { static consteval auto calculate() noexcept { return format_size_align("wchar", sizeof(wchar_t), alignof(wchar_t)); } };
    template <SignatureMode Mode> struct TypeSignature<char8_t, Mode>  { static consteval auto calculate() noexcept { return FixedString{"char8[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<char16_t, Mode> { static consteval auto calculate() noexcept { return FixedString{"char16[s:2,a:2]"}; } };
    template <SignatureMode Mode> struct TypeSignature<char32_t, Mode> { static consteval auto calculate() noexcept { return FixedString{"char32[s:4,a:4]"}; } };

    // =========================================================================
    // Other fundamentals
    // =========================================================================

    template <SignatureMode Mode> struct TypeSignature<bool, Mode>     { static consteval auto calculate() noexcept { return FixedString{"bool[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<std::nullptr_t, Mode> { static consteval auto calculate() noexcept { return format_size_align("nullptr", sizeof(std::nullptr_t), alignof(std::nullptr_t)); } };
    template <SignatureMode Mode> struct TypeSignature<std::byte, Mode> { static consteval auto calculate() noexcept { return FixedString{"byte[s:1,a:1]"}; } };

    // =========================================================================
    // Function pointers
    // =========================================================================

    template <typename R, typename... Args, SignatureMode Mode>
    struct TypeSignature<R(*)(Args...), Mode> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args...)), alignof(R(*)(Args...)));
        }
    };

    template <typename R, typename... Args, SignatureMode Mode>
    struct TypeSignature<R(*)(Args...) noexcept, Mode> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args...) noexcept), alignof(R(*)(Args...) noexcept));
        }
    };

    template <typename R, typename... Args, SignatureMode Mode>
    struct TypeSignature<R(*)(Args..., ...), Mode> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args..., ...)), alignof(R(*)(Args..., ...)));
        }
    };

    template <typename R, typename... Args, SignatureMode Mode>
    struct TypeSignature<R(*)(Args..., ...) noexcept, Mode> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args..., ...) noexcept), alignof(R(*)(Args..., ...) noexcept));
        }
    };

    // =========================================================================
    // CV-qualified: strip and forward
    // =========================================================================

    template <typename T, SignatureMode Mode>
    struct TypeSignature<const T, Mode> {
        static consteval auto calculate() noexcept { return TypeSignature<T, Mode>::calculate(); }
    };
    template <typename T, SignatureMode Mode>
    struct TypeSignature<volatile T, Mode> {
        static consteval auto calculate() noexcept { return TypeSignature<T, Mode>::calculate(); }
    };
    template <typename T, SignatureMode Mode>
    struct TypeSignature<const volatile T, Mode> {
        static consteval auto calculate() noexcept { return TypeSignature<T, Mode>::calculate(); }
    };

    // =========================================================================
    // Pointers and references
    // =========================================================================

    template <typename T, SignatureMode Mode>
    struct TypeSignature<T*, Mode> {
        static consteval auto calculate() noexcept { return format_size_align("ptr", sizeof(T*), alignof(T*)); }
    };
    template <typename T, SignatureMode Mode>
    struct TypeSignature<T&, Mode> {
        static consteval auto calculate() noexcept { return format_size_align("ref", sizeof(T*), alignof(T*)); }
    };
    template <typename T, SignatureMode Mode>
    struct TypeSignature<T&&, Mode> {
        static consteval auto calculate() noexcept { return format_size_align("rref", sizeof(T*), alignof(T*)); }
    };
    template <typename T, typename C, SignatureMode Mode>
    struct TypeSignature<T C::*, Mode> {
        static consteval auto calculate() noexcept { return format_size_align("memptr", sizeof(T C::*), alignof(T C::*)); }
    };

    // =========================================================================
    // Arrays
    // =========================================================================

    template <typename T, SignatureMode Mode>
    struct TypeSignature<T[], Mode> {
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

    template <typename T, size_t N, SignatureMode Mode>
    struct TypeSignature<T[N], Mode> {
        static consteval auto calculate() noexcept {
            if constexpr (is_byte_element<T>()) {
                return FixedString{"bytes[s:"} + to_fixed_string(N) + FixedString{",a:1]"};
            } else {
                return FixedString{"array[s:"} + to_fixed_string(sizeof(T[N])) +
                       FixedString{",a:"} + to_fixed_string(alignof(T[N])) +
                       FixedString{"]<"} + TypeSignature<T, Mode>::calculate() +
                       FixedString{","} + to_fixed_string(N) + FixedString{">"};
            }
        }
    };

    // =========================================================================
    // Primary template: structs, classes, enums, unions
    // =========================================================================

    template <typename T, SignatureMode Mode>
    struct TypeSignature {
        static consteval auto calculate() noexcept {
            if constexpr (std::is_enum_v<T>) {
                using U = std::underlying_type_t<T>;
                if constexpr (Mode == SignatureMode::Definition) {
                    return FixedString{"enum<"} +
                           get_type_qualified_name<T>() +
                           FixedString{">[s:"} +
                           to_fixed_string(sizeof(T)) +
                           FixedString{",a:"} +
                           to_fixed_string(alignof(T)) +
                           FixedString{"]<"} + TypeSignature<U, Mode>::calculate() + FixedString{">"};
                } else {
                    return FixedString{"enum[s:"} +
                           to_fixed_string(sizeof(T)) +
                           FixedString{",a:"} +
                           to_fixed_string(alignof(T)) +
                           FixedString{"]<"} + TypeSignature<U, Mode>::calculate() + FixedString{">"};
                }
            }
            else if constexpr (std::is_union_v<T>) {
                if constexpr (Mode == SignatureMode::Definition) {
                    return FixedString{"union[s:"} + to_fixed_string(sizeof(T)) +
                           FixedString{",a:"} + to_fixed_string(alignof(T)) +
                           FixedString{"]{"} + definition_fields<T>() + FixedString{"}"};
                } else {
                    return FixedString{"union[s:"} + to_fixed_string(sizeof(T)) +
                           FixedString{",a:"} + to_fixed_string(alignof(T)) +
                           FixedString{"]{"} + get_layout_union_content<T>() + FixedString{"}"};
                }
            }
            else if constexpr (std::is_class_v<T> && !std::is_array_v<T>) {
                if constexpr (Mode == SignatureMode::Layout) {
                    constexpr bool poly = std::is_polymorphic_v<T>;
                    if constexpr (poly) {
                        // vptr occupies pointer_size bytes at an implementation-defined position
                        return FixedString{"record[s:"} +
                               to_fixed_string(sizeof(T)) +
                               FixedString{",a:"} +
                               to_fixed_string(alignof(T)) +
                               FixedString{",vptr]{"} +
                               get_layout_content<T>() +
                               FixedString{"}"};
                    } else {
                        return FixedString{"record[s:"} +
                               to_fixed_string(sizeof(T)) +
                               FixedString{",a:"} +
                               to_fixed_string(alignof(T)) +
                               FixedString{"]{"} +
                               get_layout_content<T>() +
                               FixedString{"}"};
                    }
                } else {
                    // Definition mode: "record" prefix, preserve tree, include names + polymorphic marker
                    constexpr bool poly = std::is_polymorphic_v<T>;
                    auto suffix = [&]() {
                        if constexpr (poly) return FixedString{",polymorphic]{"};
                        else return FixedString{"]{"};
                    }();
                    return FixedString{"record[s:"} +
                           to_fixed_string(sizeof(T)) +
                           FixedString{",a:"} +
                           to_fixed_string(alignof(T)) +
                           suffix + definition_content<T>() + FixedString{"}"};
                }
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
