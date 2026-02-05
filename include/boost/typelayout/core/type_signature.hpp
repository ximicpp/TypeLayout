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

    // =========================================================================
    // Signature Formatting Helpers
    // =========================================================================

    // Format type signature with size and alignment: "name[s:SIZE,a:ALIGN]"
    template<size_t N>
    consteval auto format_size_align(const char (&name)[N], size_t size, size_t align) noexcept {
        return CompileString{name} + CompileString{"[s:"} +
               CompileString<32>::from_number(size) +
               CompileString{",a:"} +
               CompileString<32>::from_number(align) +
               CompileString{"]"};
    }

    // =========================================================================
    // Type Alias Detection Helpers
    // =========================================================================
    
    // Check if a type is the same as any fixed-width integer type
    template <typename T>
    inline constexpr bool is_fixed_width_integer_v = 
        std::is_same_v<T, int8_t> || std::is_same_v<T, uint8_t> ||
        std::is_same_v<T, int16_t> || std::is_same_v<T, uint16_t> ||
        std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> ||
        std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t>;

    // =========================================================================
    // Fixed-width Integer Types
    // =========================================================================
    
    // 8-bit types
    template <> struct TypeSignature<int8_t>   { static consteval auto calculate() noexcept { return CompileString{"i8[s:1,a:1]"}; } };
    template <> struct TypeSignature<uint8_t>  { static consteval auto calculate() noexcept { return CompileString{"u8[s:1,a:1]"}; } };
    
    // 16-bit types
    template <> struct TypeSignature<int16_t>  { static consteval auto calculate() noexcept { return CompileString{"i16[s:2,a:2]"}; } };
    template <> struct TypeSignature<uint16_t> { static consteval auto calculate() noexcept { return CompileString{"u16[s:2,a:2]"}; } };
    
    // 32-bit types
    template <> struct TypeSignature<int32_t>  { static consteval auto calculate() noexcept { return CompileString{"i32[s:4,a:4]"}; } };
    template <> struct TypeSignature<uint32_t> { static consteval auto calculate() noexcept { return CompileString{"u32[s:4,a:4]"}; } };
    
    // 64-bit types
    template <> struct TypeSignature<int64_t>  { static consteval auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; } };
    template <> struct TypeSignature<uint64_t> { static consteval auto calculate() noexcept { return CompileString{"u64[s:8,a:8]"}; } };

    // =========================================================================
    // Standard Integer Types (conditional based on type identity, not preprocessor)
    // =========================================================================
    
    // signed char - only if distinct from int8_t
    template <>
    requires (!std::is_same_v<signed char, int8_t>)
    struct TypeSignature<signed char> { 
        static consteval auto calculate() noexcept { return CompileString{"i8[s:1,a:1]"}; } 
    };

    // unsigned char - only if distinct from uint8_t
    template <>
    requires (!std::is_same_v<unsigned char, uint8_t>)
    struct TypeSignature<unsigned char> { 
        static consteval auto calculate() noexcept { return CompileString{"u8[s:1,a:1]"}; } 
    };

    // long - only if distinct from int32_t and int64_t
    template <>
    requires (!std::is_same_v<long, int32_t> && !std::is_same_v<long, int64_t>)
    struct TypeSignature<long> { 
        static consteval auto calculate() noexcept { 
            if constexpr (sizeof(long) == 4) {
                return CompileString{"i32[s:4,a:4]"};
            } else {
                return CompileString{"i64[s:8,a:8]"};
            }
        } 
    };

    // unsigned long - only if distinct from uint32_t and uint64_t
    template <>
    requires (!std::is_same_v<unsigned long, uint32_t> && !std::is_same_v<unsigned long, uint64_t>)
    struct TypeSignature<unsigned long> { 
        static consteval auto calculate() noexcept { 
            if constexpr (sizeof(unsigned long) == 4) {
                return CompileString{"u32[s:4,a:4]"};
            } else {
                return CompileString{"u64[s:8,a:8]"};
            }
        } 
    };

    // long long - only if distinct from int64_t
    template <>
    requires (!std::is_same_v<long long, int64_t>)
    struct TypeSignature<long long> { 
        static consteval auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; } 
    };

    // unsigned long long - only if distinct from uint64_t
    template <>
    requires (!std::is_same_v<unsigned long long, uint64_t>)
    struct TypeSignature<unsigned long long> { 
        static consteval auto calculate() noexcept { return CompileString{"u64[s:8,a:8]"}; } 
    };

    // =========================================================================
    // Floating Point Types
    // =========================================================================
    
    template <> struct TypeSignature<float>    { static consteval auto calculate() noexcept { return CompileString{"f32[s:4,a:4]"}; } };
    template <> struct TypeSignature<double>   { static consteval auto calculate() noexcept { return CompileString{"f64[s:8,a:8]"}; } };
    // long double - platform dependent, typically 8, 12, or 16 bytes
    template <> struct TypeSignature<long double> { 
        static consteval auto calculate() noexcept { 
            return format_size_align("f80", sizeof(long double), alignof(long double));
        } 
    };
    
    // =========================================================================
    // Character Types
    // =========================================================================
    
    template <> struct TypeSignature<char>     { static consteval auto calculate() noexcept { return CompileString{"char[s:1,a:1]"}; } };
    template <> struct TypeSignature<wchar_t>  { 
        static consteval auto calculate() noexcept { 
            // wchar_t: 2 bytes on Windows, 4 bytes on Linux/macOS
            return format_size_align("wchar", sizeof(wchar_t), alignof(wchar_t));
        } 
    };
    template <> struct TypeSignature<char8_t>  { static consteval auto calculate() noexcept { return CompileString{"char8[s:1,a:1]"}; } };
    template <> struct TypeSignature<char16_t> { static consteval auto calculate() noexcept { return CompileString{"char16[s:2,a:2]"}; } };
    template <> struct TypeSignature<char32_t> { static consteval auto calculate() noexcept { return CompileString{"char32[s:4,a:4]"}; } };
    
    // =========================================================================
    // Boolean and Special Types
    // =========================================================================
    
    template <> struct TypeSignature<bool>     { static consteval auto calculate() noexcept { return CompileString{"bool[s:1,a:1]"}; } };
    
    // nullptr_t - size is platform-dependent
    template <> struct TypeSignature<std::nullptr_t> { 
        static consteval auto calculate() noexcept { 
            return format_size_align("nullptr", sizeof(std::nullptr_t), alignof(std::nullptr_t));
        } 
    };

    // std::byte
    template <> struct TypeSignature<std::byte> { static consteval auto calculate() noexcept { return CompileString{"byte[s:1,a:1]"}; } };

    // =========================================================================
    // Function Pointer Types
    // =========================================================================
    
    // Generic function pointer (R(*)(Args...)) - size is platform-dependent
    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args...)> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args...)), alignof(R(*)(Args...)));
        }
    };
    
    // Noexcept function pointer (R(*)(Args...) noexcept)
    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args...) noexcept> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args...) noexcept), alignof(R(*)(Args...) noexcept));
        }
    };
    
    // C-style variadic function pointer (R(*)(Args..., ...))
    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args..., ...)> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args..., ...)), alignof(R(*)(Args..., ...)));
        }
    };

    // =========================================================================
    // CV-Qualified Types
    // =========================================================================

    // Const types - strip const and delegate
    template <typename T>
    struct TypeSignature<const T> {
        static consteval auto calculate() noexcept {
            return TypeSignature<T>::calculate();
        }
    };

    // Volatile types - strip volatile and delegate
    template <typename T>
    struct TypeSignature<volatile T> {
        static consteval auto calculate() noexcept {
            return TypeSignature<T>::calculate();
        }
    };

    // Const volatile types - strip both and delegate
    template <typename T>
    struct TypeSignature<const volatile T> {
        static consteval auto calculate() noexcept {
            return TypeSignature<T>::calculate();
        }
    };

    // =========================================================================
    // Pointer and Reference Types
    // =========================================================================

    // Pointer types - size is platform-dependent (4 bytes on 32-bit, 8 bytes on 64-bit)
    template <typename T> 
    struct TypeSignature<T*> { 
        static consteval auto calculate() noexcept { 
            return format_size_align("ptr", sizeof(T*), alignof(T*));
        } 
    };
    
    template <> 
    struct TypeSignature<void*> { 
        static consteval auto calculate() noexcept { 
            return format_size_align("ptr", sizeof(void*), alignof(void*));
        } 
    };

    // Reference types - treated as pointer-like (same memory footprint)
    template <typename T> 
    struct TypeSignature<T&> { 
        static consteval auto calculate() noexcept { 
            // References have same size as pointers
            return format_size_align("ref", sizeof(T*), alignof(T*));
        } 
    };
    
    // Rvalue reference types
    template <typename T> 
    struct TypeSignature<T&&> { 
        static consteval auto calculate() noexcept { 
            // Rvalue refs have same size as pointers
            return format_size_align("rref", sizeof(T*), alignof(T*));
        } 
    };

    // Member pointer types - size varies by platform and class type
    template <typename T, typename C>
    struct TypeSignature<T C::*> {
        static consteval auto calculate() noexcept {
            return format_size_align("memptr", sizeof(T C::*), alignof(T C::*));
        }
    };

    // =========================================================================
    // Array Types
    // =========================================================================
    
    // Unbounded array (T[]) - compile-time error with clear message
    template <typename T>
    struct TypeSignature<T[]> {
        static consteval auto calculate() noexcept {
            static_assert(always_false<T>::value,
                "TypeLayout Error: Unbounded array 'T[]' has no defined size. "
                "Use fixed-size array 'T[N]' instead.");
            return CompileString{""};
        }
    };

    // Bounded array (T[N]) - normal handling
    template <typename T, size_t N>
    struct TypeSignature<T[N]> {
        static consteval auto calculate() noexcept {
            if constexpr (std::is_same_v<T, char>) {
                // Character arrays treated as byte buffers
                return CompileString{"bytes[s:"} +
                       CompileString<32>::from_number(N) +
                       CompileString{",a:1]"};
            } else {
                // Regular arrays include element signature
                return CompileString{"array[s:"} +
                       CompileString<32>::from_number(sizeof(T[N])) +
                       CompileString{",a:"} +
                       CompileString<32>::from_number(alignof(T[N])) +
                       CompileString{"]<"} +
                       TypeSignature<T>::calculate() +
                       CompileString{","} +
                       CompileString<32>::from_number(N) +
                       CompileString{">"};
            }
        }
    };

    // =========================================================================
    // Generic Type Signature (structs/classes/enums/unions)
    // =========================================================================
    
    template <typename T>
    struct TypeSignature {
        static consteval auto calculate() noexcept {
            if constexpr (std::is_enum_v<T>) {
                // Enum type: include underlying type in signature
                using UnderlyingType = std::underlying_type_t<T>;
                return CompileString{"enum[s:"} +
                       CompileString<32>::from_number(sizeof(T)) +
                       CompileString{",a:"} +
                       CompileString<32>::from_number(alignof(T)) +
                       CompileString{"]<"} +
                       TypeSignature<UnderlyingType>::calculate() +
                       CompileString{">"};
            }
            else if constexpr (std::is_union_v<T>) {
                // Union type: include member information
                // All union members share offset 0 (overlapping storage)
                return CompileString{"union[s:"} +
                       CompileString<32>::from_number(sizeof(T)) +
                       CompileString{",a:"} +
                       CompileString<32>::from_number(alignof(T)) +
                       CompileString{"]{"} +
                       get_fields_signature<T>() +
                       CompileString{"}"};
            }
            else if constexpr (std::is_class_v<T> && !std::is_array_v<T>) {
                constexpr bool is_poly = std::is_polymorphic_v<T>;
                constexpr bool has_base = has_bases<T>();
                
                if constexpr (is_poly && has_base) {
                    // Polymorphic class with inheritance
                    return CompileString{"class[s:"} +
                           CompileString<32>::from_number(sizeof(T)) +
                           CompileString{",a:"} +
                           CompileString<32>::from_number(alignof(T)) +
                           CompileString{",polymorphic,inherited]{"} +
                           get_layout_content_signature<T>() +
                           CompileString{"}"};
                } else if constexpr (is_poly) {
                    // Polymorphic class without inheritance
                    return CompileString{"class[s:"} +
                           CompileString<32>::from_number(sizeof(T)) +
                           CompileString{",a:"} +
                           CompileString<32>::from_number(alignof(T)) +
                           CompileString{",polymorphic]{"} +
                           get_layout_content_signature<T>() +
                           CompileString{"}"};
                } else if constexpr (has_base) {
                    // Non-polymorphic class with inheritance
                    return CompileString{"class[s:"} +
                           CompileString<32>::from_number(sizeof(T)) +
                           CompileString{",a:"} +
                           CompileString<32>::from_number(alignof(T)) +
                           CompileString{",inherited]{"} +
                           get_layout_content_signature<T>() +
                           CompileString{"}"};
                } else {
                    // Regular struct/class without inheritance
                    return CompileString{"struct[s:"} +
                           CompileString<32>::from_number(sizeof(T)) +
                           CompileString{",a:"} +
                           CompileString<32>::from_number(alignof(T)) +
                           CompileString{"]{"} +
                           get_layout_content_signature<T>() +
                           CompileString{"}"};
                }
            }
            else if constexpr (std::is_void_v<T>) {
                static_assert(always_false<T>::value,
                    "TypeLayout Error: 'void' type has no memory layout. "
                    "Suggestion: Use 'void*' (pointer to void) instead if you need a generic pointer.");
                return CompileString{""};
            }
            else if constexpr (std::is_function_v<T>) {
                static_assert(always_false<T>::value,
                    "TypeLayout Error: Function types (not pointers) have no defined size. "
                    "Suggestion: Use a function pointer type like 'ReturnType(*)(Args...)' instead.");
                return CompileString{""};
            }
            else {
                static_assert(always_false<T>::value, 
                    "TypeLayout Error: This type is not supported for layout signature generation. "
                    "Supported types include: arithmetic types, pointers, references, arrays, "
                    "enums, unions, and standard layout class/struct types.");
                return CompileString{""};
            }
        }
    };

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_TYPE_SIGNATURE_HPP
