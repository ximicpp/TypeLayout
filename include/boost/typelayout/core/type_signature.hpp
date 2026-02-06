// Boost.TypeLayout
//
// Type Signature Specializations
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_CORE_TYPE_SIGNATURE_HPP
#define BOOST_TYPELAYOUT_CORE_TYPE_SIGNATURE_HPP

#include <boost/typelayout/core/config.hpp>
#include <boost/typelayout/core/compile_string.hpp>
#include <boost/typelayout/core/reflection_helpers.hpp>
#include <type_traits>

namespace boost {
namespace typelayout {

    template<size_t N>
    consteval auto format_size_align(const char (&name)[N], size_t size, size_t align) noexcept {
        return CompileString{name} + CompileString{"[s:"} +
               CompileString<32>::from_number(size) +
               CompileString{",a:"} +
               CompileString<32>::from_number(align) +
               CompileString{"]"};
    }

    // Mode-aware format_size_align (used for primitives - mode doesn't affect them)
    template<SignatureMode Mode, size_t N>
    consteval auto format_size_align_mode(const char (&name)[N], size_t size, size_t align) noexcept {
        return format_size_align(name, size, align);
    }

    template <typename T>
    inline constexpr bool is_fixed_width_integer_v = 
        std::is_same_v<T, int8_t> || std::is_same_v<T, uint8_t> ||
        std::is_same_v<T, int16_t> || std::is_same_v<T, uint16_t> ||
        std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> ||
        std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t>;

    // Platform type aliasing traits (replaces scattered #if macros)
    // macOS/Linux: int8_t = signed char, int64_t = long long
    // Windows: these are distinct types
    namespace detail {
        inline constexpr bool int8_is_signed_char = std::is_same_v<int8_t, signed char>;
        inline constexpr bool int64_is_long = std::is_same_v<int64_t, long>;
        inline constexpr bool int64_is_long_long = std::is_same_v<int64_t, long long>;
    }

    // Fixed-width integers (mode doesn't affect primitive types)
    template <SignatureMode Mode> struct TypeSignature<int8_t, Mode>   { static consteval auto calculate() noexcept { return CompileString{"i8[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint8_t, Mode>  { static consteval auto calculate() noexcept { return CompileString{"u8[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<int16_t, Mode>  { static consteval auto calculate() noexcept { return CompileString{"i16[s:2,a:2]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint16_t, Mode> { static consteval auto calculate() noexcept { return CompileString{"u16[s:2,a:2]"}; } };
    template <SignatureMode Mode> struct TypeSignature<int32_t, Mode>  { static consteval auto calculate() noexcept { return CompileString{"i32[s:4,a:4]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint32_t, Mode> { static consteval auto calculate() noexcept { return CompileString{"u32[s:4,a:4]"}; } };
    template <SignatureMode Mode> struct TypeSignature<int64_t, Mode>  { static consteval auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint64_t, Mode> { static consteval auto calculate() noexcept { return CompileString{"u64[s:8,a:8]"}; } };

    // Platform-specific: int8_t aliases signed char on macOS/Linux
#if !defined(__APPLE__) && !defined(__linux__)
    template <SignatureMode Mode> struct TypeSignature<signed char, Mode> { 
        static consteval auto calculate() noexcept { return CompileString{"i8[s:1,a:1]"}; } 
    };
    template <SignatureMode Mode> struct TypeSignature<unsigned char, Mode> { 
        static consteval auto calculate() noexcept { return CompileString{"u8[s:1,a:1]"}; } 
    };
#endif

    // long: 4 bytes on Windows/32-bit, 8 bytes on LP64 Unix
#if !defined(__linux__) || !defined(__LP64__)
    template <SignatureMode Mode> struct TypeSignature<long, Mode> { 
        static consteval auto calculate() noexcept { 
            if constexpr (sizeof(long) == 4) return CompileString{"i32[s:4,a:4]"};
            else return CompileString{"i64[s:8,a:8]"};
        } 
    };

    template <SignatureMode Mode> struct TypeSignature<unsigned long, Mode> { 
        static consteval auto calculate() noexcept { 
            if constexpr (sizeof(unsigned long) == 4) return CompileString{"u32[s:4,a:4]"};
            else return CompileString{"u64[s:8,a:8]"};
        } 
    };
#endif

    // long long
#if defined(__linux__) && defined(__LP64__)
    template <SignatureMode Mode> struct TypeSignature<long long, Mode> { 
        static consteval auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; } 
    };
    template <SignatureMode Mode> struct TypeSignature<unsigned long long, Mode> { 
        static consteval auto calculate() noexcept { return CompileString{"u64[s:8,a:8]"}; } 
    };
#elif !defined(__APPLE__) && !defined(_WIN64)
    template <SignatureMode Mode> struct TypeSignature<long long, Mode> { 
        static consteval auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; } 
    };
    template <SignatureMode Mode> struct TypeSignature<unsigned long long, Mode> { 
        static consteval auto calculate() noexcept { return CompileString{"u64[s:8,a:8]"}; } 
    };
#endif

    // Floating point
    template <SignatureMode Mode> struct TypeSignature<float, Mode>    { static consteval auto calculate() noexcept { return CompileString{"f32[s:4,a:4]"}; } };
    template <SignatureMode Mode> struct TypeSignature<double, Mode>   { static consteval auto calculate() noexcept { return CompileString{"f64[s:8,a:8]"}; } };
    template <SignatureMode Mode> struct TypeSignature<long double, Mode> { 
        static consteval auto calculate() noexcept { return format_size_align("f80", sizeof(long double), alignof(long double)); } 
    };

    // Characters
    template <SignatureMode Mode> struct TypeSignature<char, Mode>     { static consteval auto calculate() noexcept { return CompileString{"char[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<wchar_t, Mode>  { static consteval auto calculate() noexcept { return format_size_align("wchar", sizeof(wchar_t), alignof(wchar_t)); } };
    template <SignatureMode Mode> struct TypeSignature<char8_t, Mode>  { static consteval auto calculate() noexcept { return CompileString{"char8[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<char16_t, Mode> { static consteval auto calculate() noexcept { return CompileString{"char16[s:2,a:2]"}; } };
    template <SignatureMode Mode> struct TypeSignature<char32_t, Mode> { static consteval auto calculate() noexcept { return CompileString{"char32[s:4,a:4]"}; } };

    // Special types
    template <SignatureMode Mode> struct TypeSignature<bool, Mode>     { static consteval auto calculate() noexcept { return CompileString{"bool[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<std::nullptr_t, Mode> { static consteval auto calculate() noexcept { return format_size_align("nullptr", sizeof(std::nullptr_t), alignof(std::nullptr_t)); } };
    template <SignatureMode Mode> struct TypeSignature<std::byte, Mode> { static consteval auto calculate() noexcept { return CompileString{"byte[s:1,a:1]"}; } };

    // Function pointers
    template <typename R, typename... Args, SignatureMode Mode>
    struct TypeSignature<R(*)(Args...), Mode> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args...)), alignof(R(*)(Args...)));
        }
    };
    
    // Noexcept function pointer (R(*)(Args...) noexcept)
    template <typename R, typename... Args, SignatureMode Mode>
    struct TypeSignature<R(*)(Args...) noexcept, Mode> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args...) noexcept), alignof(R(*)(Args...) noexcept));
        }
    };
    
    // C-style variadic function pointer (R(*)(Args..., ...))
    template <typename R, typename... Args, SignatureMode Mode>
    struct TypeSignature<R(*)(Args..., ...), Mode> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args..., ...)), alignof(R(*)(Args..., ...)));
        }
    };

    // CV-qualified types: strip qualifiers and forward mode
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

    // Pointers and references
    template <typename T, SignatureMode Mode> 
    struct TypeSignature<T*, Mode> { 
        static consteval auto calculate() noexcept { return format_size_align("ptr", sizeof(T*), alignof(T*)); } 
    };
    template <SignatureMode Mode> 
    struct TypeSignature<void*, Mode> { 
        static consteval auto calculate() noexcept { return format_size_align("ptr", sizeof(void*), alignof(void*)); } 
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

    // Arrays
    template <typename T, SignatureMode Mode>
    struct TypeSignature<T[], Mode> {
        static consteval auto calculate() noexcept {
            static_assert(always_false<T>::value, "Unbounded array T[] has no defined size");
            return CompileString{""};
        }
    };
    template <typename T, size_t N, SignatureMode Mode>
    struct TypeSignature<T[N], Mode> {
        static consteval auto calculate() noexcept {
            if constexpr (std::is_same_v<T, char>) {
                return CompileString{"bytes[s:"} + CompileString<32>::from_number(N) + CompileString{",a:1]"};
            } else {
                return CompileString{"array[s:"} + CompileString<32>::from_number(sizeof(T[N])) +
                       CompileString{",a:"} + CompileString<32>::from_number(alignof(T[N])) +
                       CompileString{"]<"} + TypeSignature<T, Mode>::calculate() +
                       CompileString{","} + CompileString<32>::from_number(N) + CompileString{">"};
            }
        }
    };

    // =========================================================================
    // Generic: structs, classes, enums, unions
    // Primary template with mode parameter
    // =========================================================================
    template <typename T, SignatureMode Mode>
    struct TypeSignature {
        static consteval auto calculate() noexcept {
            if constexpr (std::is_enum_v<T>) {
                using U = std::underlying_type_t<T>;
                return CompileString{"enum[s:"} + CompileString<32>::from_number(sizeof(T)) +
                       CompileString{",a:"} + CompileString<32>::from_number(alignof(T)) +
                       CompileString{"]<"} + TypeSignature<U, Mode>::calculate() + CompileString{">"};
            }
            else if constexpr (std::is_union_v<T>) {
                return CompileString{"union[s:"} + CompileString<32>::from_number(sizeof(T)) +
                       CompileString{",a:"} + CompileString<32>::from_number(alignof(T)) +
                       CompileString{"]{"} + get_fields_signature<T, Mode>() + CompileString{"}"};
            }
            else if constexpr (std::is_class_v<T> && !std::is_array_v<T>) {
                constexpr bool poly = std::is_polymorphic_v<T>;
                constexpr bool base = has_bases<T>();
                auto prefix = [&]() {
                    if constexpr (poly && base) return CompileString{"class[s:"};
                    else if constexpr (poly) return CompileString{"class[s:"};
                    else if constexpr (base) return CompileString{"class[s:"};
                    else return CompileString{"struct[s:"};
                }();
                auto suffix = [&]() {
                    if constexpr (poly && base) return CompileString{",polymorphic,inherited]{"};
                    else if constexpr (poly) return CompileString{",polymorphic]{"};
                    else if constexpr (base) return CompileString{",inherited]{"};
                    else return CompileString{"]{"};
                }();
                return prefix + CompileString<32>::from_number(sizeof(T)) +
                       CompileString{",a:"} + CompileString<32>::from_number(alignof(T)) +
                       suffix + get_layout_content_signature<T, Mode>() + CompileString{"}"};
            }
            else if constexpr (std::is_void_v<T>) {
                static_assert(always_false<T>::value, "void has no layout; use void*");
                return CompileString{""};
            }
            else if constexpr (std::is_function_v<T>) {
                static_assert(always_false<T>::value, "function types have no size; use function pointer");
                return CompileString{""};
            }
            else {
                static_assert(always_false<T>::value, "unsupported type for layout signature");
                return CompileString{""};
            }
        }
    };

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_TYPE_SIGNATURE_HPP
