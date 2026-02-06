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

// Internal include guard: allows reflection_helpers.hpp to verify it's included correctly
#define BOOST_TYPELAYOUT_INTERNAL_INCLUDE_
#include <boost/typelayout/core/reflection_helpers.hpp>
#undef BOOST_TYPELAYOUT_INTERNAL_INCLUDE_
#include <type_traits>

namespace boost {
namespace typelayout {

    template<size_t N>
    consteval auto format_size_align(const char (&name)[N], size_t size, size_t align) noexcept {
        return CompileString{name} + CompileString{"[s:"} +
               CompileString<number_buffer_size>::from_number(size) +
               CompileString{",a:"} +
               CompileString<number_buffer_size>::from_number(align) +
               CompileString{"]"};
    }

    // =============================================================================
    // Fixed-width integers (canonical, always defined)
    // =============================================================================
    template <SignatureMode Mode> struct TypeSignature<int8_t, Mode>   { static consteval auto calculate() noexcept { return CompileString{"i8[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint8_t, Mode>  { static consteval auto calculate() noexcept { return CompileString{"u8[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<int16_t, Mode>  { static consteval auto calculate() noexcept { return CompileString{"i16[s:2,a:2]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint16_t, Mode> { static consteval auto calculate() noexcept { return CompileString{"u16[s:2,a:2]"}; } };
    template <SignatureMode Mode> struct TypeSignature<int32_t, Mode>  { static consteval auto calculate() noexcept { return CompileString{"i32[s:4,a:4]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint32_t, Mode> { static consteval auto calculate() noexcept { return CompileString{"u32[s:4,a:4]"}; } };
    template <SignatureMode Mode> struct TypeSignature<int64_t, Mode>  { static consteval auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint64_t, Mode> { static consteval auto calculate() noexcept { return CompileString{"u64[s:8,a:8]"}; } };

    // =============================================================================
    // Fundamental types: use requires clauses to avoid duplicate specializations
    // Only specialize when the type is distinct from fixed-width integers
    // =============================================================================

    // signed char / unsigned char: only if distinct from int8_t/uint8_t
    template <SignatureMode Mode>
        requires (!std::is_same_v<signed char, int8_t>)
    struct TypeSignature<signed char, Mode> {
        static consteval auto calculate() noexcept { return CompileString{"i8[s:1,a:1]"}; }
    };

    template <SignatureMode Mode>
        requires (!std::is_same_v<unsigned char, uint8_t>)
    struct TypeSignature<unsigned char, Mode> {
        static consteval auto calculate() noexcept { return CompileString{"u8[s:1,a:1]"}; }
    };

    // long / unsigned long: only if distinct from int32_t AND int64_t
    template <SignatureMode Mode>
        requires (!std::is_same_v<long, int32_t> && !std::is_same_v<long, int64_t>)
    struct TypeSignature<long, Mode> {
        static consteval auto calculate() noexcept {
            if constexpr (sizeof(long) == 4) return CompileString{"i32[s:4,a:4]"};
            else return CompileString{"i64[s:8,a:8]"};
        }
    };

    template <SignatureMode Mode>
        requires (!std::is_same_v<unsigned long, uint32_t> && !std::is_same_v<unsigned long, uint64_t>)
    struct TypeSignature<unsigned long, Mode> {
        static consteval auto calculate() noexcept {
            if constexpr (sizeof(unsigned long) == 4) return CompileString{"u32[s:4,a:4]"};
            else return CompileString{"u64[s:8,a:8]"};
        }
    };

    // long long / unsigned long long: only if distinct from int64_t/uint64_t
    template <SignatureMode Mode>
        requires (!std::is_same_v<long long, int64_t>)
    struct TypeSignature<long long, Mode> {
        static consteval auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; }
    };

    template <SignatureMode Mode>
        requires (!std::is_same_v<unsigned long long, uint64_t>)
    struct TypeSignature<unsigned long long, Mode> {
        static consteval auto calculate() noexcept { return CompileString{"u64[s:8,a:8]"}; }
    };

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

    // Pointers and references (void* covered by T* specialization)
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

    // Arrays
    template <typename T, SignatureMode Mode>
    struct TypeSignature<T[], Mode> {
        static consteval auto calculate() noexcept {
            static_assert(always_false<T>::value, "Unbounded array T[] has no defined size");
            return CompileString{""};
        }
    };

    // Helper: detect single-byte array types that should be treated as bytes[]
    template <typename T>
    inline constexpr bool is_byte_element_v =
        std::is_same_v<T, char> || std::is_same_v<T, signed char> ||
        std::is_same_v<T, unsigned char> || std::is_same_v<T, int8_t> ||
        std::is_same_v<T, uint8_t> || std::is_same_v<T, std::byte> ||
        std::is_same_v<T, char8_t>;

    template <typename T, size_t N, SignatureMode Mode>
    struct TypeSignature<T[N], Mode> {
        static consteval auto calculate() noexcept {
            if constexpr (is_byte_element_v<T>) {
                // Physical and Structural modes: normalize all single-byte arrays to bytes[]
                // This ensures char[N], int8_t[N], uint8_t[N], byte[N], char8_t[N] have identical signatures
                // guaranteeing: identical ABI layout → identical signature
                if constexpr (Mode != SignatureMode::Annotated) {
                    return CompileString{"bytes[s:"} + CompileString<number_buffer_size>::from_number(N) + CompileString{",a:1]"};
                }
                // Annotated mode: preserve element type for debugging/diagnostics
                else {
                    return CompileString{""} + TypeSignature<T, Mode>::calculate() + 
                           CompileString{"["} + CompileString<number_buffer_size>::from_number(N) + CompileString{"]"};
                }
            }
            // General case: full type information for non-byte arrays
            else {
                return CompileString{"array[s:"} + CompileString<number_buffer_size>::from_number(sizeof(T[N])) +
                       CompileString{",a:"} + CompileString<number_buffer_size>::from_number(alignof(T[N])) +
                       CompileString{"]<"} + TypeSignature<T, Mode>::calculate() +
                       CompileString{","} + CompileString<number_buffer_size>::from_number(N) + CompileString{">"};
            }
        }
    };

    // =========================================================================
    // Generic: structs, classes, enums, unions
    // Primary template with mode parameter
    // =========================================================================
    //
    // DESIGN DECISION: struct vs class distinction
    //
    // Signatures encode "ABI identity" rather than just "byte layout". This means:
    // - Polymorphic types use "class" prefix with "polymorphic" flag (vtable affects ABI)
    // - Types with bases use "class" prefix with "inherited" flag (affects ABI on some platforms)
    // - Plain aggregates use "struct" prefix
    //
    // Consequence: A flat struct and an inherited class with identical byte layouts
    // will have DIFFERENT signatures. This is intentional—it's conservative ABI safety.
    // See doc/design/abi-identity.md for detailed rationale.
    //
    // =========================================================================
    template <typename T, SignatureMode Mode>
    struct TypeSignature {
        static consteval auto calculate() noexcept {
            if constexpr (std::is_enum_v<T>) {
                using U = std::underlying_type_t<T>;
                return CompileString{"enum[s:"} + CompileString<number_buffer_size>::from_number(sizeof(T)) +
                       CompileString{",a:"} + CompileString<number_buffer_size>::from_number(alignof(T)) +
                       CompileString{"]<"} + TypeSignature<U, Mode>::calculate() + CompileString{">"};
            }
            else if constexpr (std::is_union_v<T>) {
                return CompileString{"union[s:"} + CompileString<number_buffer_size>::from_number(sizeof(T)) +
                       CompileString{",a:"} + CompileString<number_buffer_size>::from_number(alignof(T)) +
                       CompileString{"]{"} + get_fields_signature<T, Mode>() + CompileString{"}"};
            }
            else if constexpr (std::is_class_v<T> && !std::is_array_v<T>) {
                if constexpr (Mode == SignatureMode::Physical) {
                    // Physical mode: uniform "record" prefix, no polymorphic/inherited markers,
                    // flatten inheritance hierarchy
                    return CompileString{"record[s:"} +
                           CompileString<number_buffer_size>::from_number(sizeof(T)) +
                           CompileString{",a:"} +
                           CompileString<number_buffer_size>::from_number(alignof(T)) +
                           CompileString{"]{"} +
                           get_physical_content<T>() +
                           CompileString{"}"};
                } else {
                    // Structural and Annotated: existing behavior with struct/class distinction
                    constexpr bool poly = std::is_polymorphic_v<T>;
                    constexpr bool base = has_bases<T>();
                    // Simplified: polymorphic or inherited types use "class", otherwise "struct"
                    auto prefix = [&]() {
                        if constexpr (poly || base) return CompileString{"class[s:"};
                        else return CompileString{"struct[s:"};
                    }();
                    auto suffix = [&]() {
                        if constexpr (poly && base) return CompileString{",polymorphic,inherited]{"};
                        else if constexpr (poly) return CompileString{",polymorphic]{"};
                        else if constexpr (base) return CompileString{",inherited]{"};
                        else return CompileString{"]{"};
                    }();
                    return prefix + CompileString<number_buffer_size>::from_number(sizeof(T)) +
                           CompileString{",a:"} + CompileString<number_buffer_size>::from_number(alignof(T)) +
                           suffix + get_layout_content_signature<T, Mode>() + CompileString{"}"};
                }
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
