// Boost.TypeLayout
//
// Type Signature Specializations
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_DETAIL_TYPE_SIGNATURE_HPP
#define BOOST_TYPELAYOUT_DETAIL_TYPE_SIGNATURE_HPP

#include <boost/typelayout/detail/config.hpp>
#include <boost/typelayout/detail/compile_string.hpp>
#include <boost/typelayout/detail/reflection_helpers.hpp>
#include <memory>

namespace boost {
namespace typelayout {

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
    // Standard Integer Types (conditional to avoid redefinition)
    // =========================================================================
    
    // signed char / unsigned char - only define if not same as int8_t/uint8_t
    #if !defined(__GNUC__) && !defined(__clang__)
    template <> struct TypeSignature<signed char>   { static consteval auto calculate() noexcept { return CompileString{"i8[s:1,a:1]"}; } };
    template <> struct TypeSignature<unsigned char> { static consteval auto calculate() noexcept { return CompileString{"u8[s:1,a:1]"}; } };
    #endif
    
    // On Windows LLP64, long is 4 bytes (different from int64_t which is long long)
    // On Linux LP64, long is 8 bytes and same as int64_t
    #if defined(_WIN32) || defined(_WIN64)
    // Windows: long is 4 bytes, separate from int32_t (which is also 4 bytes but different type)
    template <> struct TypeSignature<long> { 
        static consteval auto calculate() noexcept { 
            return CompileString{"i32[s:4,a:4]"};  // LLP64 (Windows)
        } 
    };
    template <> struct TypeSignature<unsigned long> { 
        static consteval auto calculate() noexcept { 
            return CompileString{"u32[s:4,a:4]"};  // LLP64 (Windows)
        } 
    };
    #endif
    
    // long long - define only on Linux (LP64) where int64_t is 'long', not 'long long'
    // On macOS and Windows, int64_t is already 'long long', so skip to avoid redefinition
    #if defined(__linux__) && !defined(__APPLE__)
    template <> struct TypeSignature<long long>          { static consteval auto calculate() noexcept { return CompileString{"i64[s:8,a:8]"}; } };
    template <> struct TypeSignature<unsigned long long> { static consteval auto calculate() noexcept { return CompileString{"u64[s:8,a:8]"}; } };
    #endif

    // =========================================================================
    // Floating Point Types
    // =========================================================================
    
    template <> struct TypeSignature<float>    { static consteval auto calculate() noexcept { return CompileString{"f32[s:4,a:4]"}; } };
    template <> struct TypeSignature<double>   { static consteval auto calculate() noexcept { return CompileString{"f64[s:8,a:8]"}; } };
    // long double - platform dependent, typically 8, 12, or 16 bytes
    template <> struct TypeSignature<long double> { 
        static consteval auto calculate() noexcept { 
            return CompileString{"f80[s:"} +
                   CompileString<32>::from_number(sizeof(long double)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(long double)) +
                   CompileString{"]"};
        } 
    };
    
    // =========================================================================
    // Character Types
    // =========================================================================
    
    template <> struct TypeSignature<char>     { static consteval auto calculate() noexcept { return CompileString{"char[s:1,a:1]"}; } };
    template <> struct TypeSignature<wchar_t>  { 
        static consteval auto calculate() noexcept { 
            // wchar_t: 2 bytes on Windows, 4 bytes on Linux/macOS
            return CompileString{"wchar[s:"} +
                   CompileString<32>::from_number(sizeof(wchar_t)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(wchar_t)) +
                   CompileString{"]"};
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
            return CompileString{"nullptr[s:"} +
                   CompileString<32>::from_number(sizeof(std::nullptr_t)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(std::nullptr_t)) +
                   CompileString{"]"};
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
            return CompileString{"fnptr[s:"} +
                   CompileString<32>::from_number(sizeof(R(*)(Args...))) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(R(*)(Args...))) +
                   CompileString{"]"};
        }
    };
    
    // Noexcept function pointer (R(*)(Args...) noexcept)
    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args...) noexcept> {
        static consteval auto calculate() noexcept {
            return CompileString{"fnptr[s:"} +
                   CompileString<32>::from_number(sizeof(R(*)(Args...) noexcept)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(R(*)(Args...) noexcept)) +
                   CompileString{"]"};
        }
    };
    
    // C-style variadic function pointer (R(*)(Args..., ...))
    template <typename R, typename... Args>
    struct TypeSignature<R(*)(Args..., ...)> {
        static consteval auto calculate() noexcept {
            return CompileString{"fnptr[s:"} +
                   CompileString<32>::from_number(sizeof(R(*)(Args..., ...))) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(R(*)(Args..., ...))) +
                   CompileString{"]"};
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
            return CompileString{"ptr[s:"} +
                   CompileString<32>::from_number(sizeof(T*)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(T*)) +
                   CompileString{"]"};
        } 
    };
    
    template <> 
    struct TypeSignature<void*> { 
        static consteval auto calculate() noexcept { 
            return CompileString{"ptr[s:"} +
                   CompileString<32>::from_number(sizeof(void*)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(void*)) +
                   CompileString{"]"};
        } 
    };

    // Reference types - treated as pointer-like (same memory footprint)
    template <typename T> 
    struct TypeSignature<T&> { 
        static consteval auto calculate() noexcept { 
            return CompileString{"ref[s:"} +
                   CompileString<32>::from_number(sizeof(T*)) +  // References have same size as pointers
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(T*)) +
                   CompileString{"]"};
        } 
    };
    
    // Rvalue reference types
    template <typename T> 
    struct TypeSignature<T&&> { 
        static consteval auto calculate() noexcept { 
            return CompileString{"rref[s:"} +
                   CompileString<32>::from_number(sizeof(T*)) +  // Rvalue refs have same size as pointers
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(T*)) +
                   CompileString{"]"};
        } 
    };

    // Member pointer types - size varies by platform and class type
    template <typename T, typename C>
    struct TypeSignature<T C::*> {
        static consteval auto calculate() noexcept {
            return CompileString{"memptr[s:"} +
                   CompileString<32>::from_number(sizeof(T C::*)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(T C::*)) +
                   CompileString{"]"};
        }
    };

    // =========================================================================
    // Array Types
    // =========================================================================
    
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
                // Union type: show size and alignment, but cannot introspect members safely
                return CompileString{"union[s:"} +
                       CompileString<32>::from_number(sizeof(T)) +
                       CompileString{",a:"} +
                       CompileString<32>::from_number(alignof(T)) +
                       CompileString{"]"};
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
            else if constexpr (std::is_pointer_v<T>) {
                return TypeSignature<void*>::calculate();
            }
            else if constexpr (std::is_array_v<T>) {
                return TypeSignature<std::remove_extent_t<T>[]>::calculate();
            }
            else {
                static_assert(always_false<T>::value, 
                    "Type is not supported for layout signature generation");
                return CompileString{""};
            }
        }
    };

    // =========================================================================
    // Smart Pointer Specializations
    // =========================================================================

    // std::unique_ptr - treat as pointer
    template <typename T, typename Deleter>
    struct TypeSignature<std::unique_ptr<T, Deleter>> {
        static consteval auto calculate() noexcept {
            return CompileString{"unique_ptr[s:"} +
                   CompileString<32>::from_number(sizeof(std::unique_ptr<T, Deleter>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(std::unique_ptr<T, Deleter>)) +
                   CompileString{"]"};
        }
    };

    // std::shared_ptr - treat as pointer
    template <typename T>
    struct TypeSignature<std::shared_ptr<T>> {
        static consteval auto calculate() noexcept {
            return CompileString{"shared_ptr[s:"} +
                   CompileString<32>::from_number(sizeof(std::shared_ptr<T>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(std::shared_ptr<T>)) +
                   CompileString{"]"};
        }
    };

    // std::weak_ptr - treat as pointer
    template <typename T>
    struct TypeSignature<std::weak_ptr<T>> {
        static consteval auto calculate() noexcept {
            return CompileString{"weak_ptr[s:"} +
                   CompileString<32>::from_number(sizeof(std::weak_ptr<T>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(std::weak_ptr<T>)) +
                   CompileString{"]"};
        }
    };

} // namespace typelayout
} // namespace boost

// =========================================================================
// std::atomic Specializations (must be in separate section to include <atomic>)
// =========================================================================

#include <atomic>

namespace boost {
namespace typelayout {

    // std::atomic<T> - provides explicit signature to avoid reflecting internal
    // implementation details (which vary between libc++ and libstdc++)
    template <typename T>
    struct TypeSignature<std::atomic<T>> {
        static consteval auto calculate() noexcept {
            return CompileString{"atomic[s:"} +
                   CompileString<32>::from_number(sizeof(std::atomic<T>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(std::atomic<T>)) +
                   CompileString{"]<"} +
                   TypeSignature<T>::calculate() +
                   CompileString{">"};
        }
    };

} // namespace typelayout
} // namespace boost

// =========================================================================
// Boost.Interprocess offset_ptr specialization (if header included)
// =========================================================================

#ifdef BOOST_INTERPROCESS_OFFSET_PTR_HPP

namespace boost {
namespace typelayout {

    template <typename T, typename DifferenceType, typename OffsetType, std::size_t Alignment>
    struct TypeSignature<boost::interprocess::offset_ptr<T, DifferenceType, OffsetType, Alignment>> {
        static consteval auto calculate() noexcept {
            return CompileString{"offset_ptr[s:"} +
                   CompileString<32>::from_number(sizeof(boost::interprocess::offset_ptr<T, DifferenceType, OffsetType, Alignment>)) +
                   CompileString{",a:"} +
                   CompileString<32>::from_number(alignof(boost::interprocess::offset_ptr<T, DifferenceType, OffsetType, Alignment>)) +
                   CompileString{"]"};
        }
    };

} // namespace typelayout
} // namespace boost

#endif // BOOST_INTERPROCESS_OFFSET_PTR_HPP

#endif // BOOST_TYPELAYOUT_DETAIL_TYPE_SIGNATURE_HPP
