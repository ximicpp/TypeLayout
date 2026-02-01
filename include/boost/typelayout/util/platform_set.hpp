// Boost.TypeLayout
//
// Utility: Platform Set Definition
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_UTIL_PLATFORM_SET_HPP
#define BOOST_TYPELAYOUT_UTIL_PLATFORM_SET_HPP

#include <boost/typelayout/core/config.hpp>
#include <boost/typelayout/core/compile_string.hpp>
#include <type_traits>
#include <cstdint>

namespace boost {
namespace typelayout {

// =========================================================================
// Platform Set Definition
// =========================================================================

/// Endianness enumeration
enum class Endianness : uint8_t {
    Little,
    Big
};

/// Bit width enumeration  
enum class BitWidth : uint8_t {
    Bits32 = 32,
    Bits64 = 64
};

/// Platform set configuration for serialization compatibility
/// Defines the target platform characteristics for memcpy-safe data transfer
struct PlatformSet {
    BitWidth bit_width;
    Endianness endianness;
    
    // Predefined platform sets by bitwidth + endianness
    static constexpr PlatformSet bits64_le() noexcept {
        return PlatformSet{BitWidth::Bits64, Endianness::Little};
    }
    
    static constexpr PlatformSet bits64_be() noexcept {
        return PlatformSet{BitWidth::Bits64, Endianness::Big};
    }
    
    static constexpr PlatformSet bits32_le() noexcept {
        return PlatformSet{BitWidth::Bits32, Endianness::Little};
    }
    
    static constexpr PlatformSet bits32_be() noexcept {
        return PlatformSet{BitWidth::Bits32, Endianness::Big};
    }
    
    // Current platform detection
    static constexpr PlatformSet current() noexcept {
        constexpr BitWidth bw = (sizeof(void*) == 8) ? BitWidth::Bits64 : BitWidth::Bits32;
        constexpr Endianness en = 
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            Endianness::Big;
#else
            Endianness::Little;
#endif
        return PlatformSet{bw, en};
    }
    
    // Check if current build platform matches this platform set
    static consteval bool current_matches(PlatformSet target) noexcept {
        auto curr = current();
        return curr.bit_width == target.bit_width && 
               curr.endianness == target.endianness;
    }
};

// =========================================================================
// Serialization Blocker Reasons
// =========================================================================

/// Reasons why a type cannot be serialized
enum class SerializationBlocker : uint8_t {
    None = 0,               // Type is serializable
    NotTriviallyCopyable = 1,
    HasPointer = 2,
    HasReference = 3,
    IsPolymorphic = 4,
    HasPlatformDependentSize = 5,  // long, etc.
    PlatformMismatch = 6,
    HasNonSerializableMember = 7,
    HasBitField = 8         // bit-fields have implementation-defined layout
};

/// Convert blocker to fixed-size string representation
template <SerializationBlocker B>
consteval auto blocker_to_string() noexcept {
    if constexpr (B == SerializationBlocker::None) {
        return CompileString{"serial"};
    } else if constexpr (B == SerializationBlocker::NotTriviallyCopyable) {
        return CompileString{"!serial:trivial"};
    } else if constexpr (B == SerializationBlocker::HasPointer) {
        return CompileString{"!serial:ptr"};
    } else if constexpr (B == SerializationBlocker::HasReference) {
        return CompileString{"!serial:ref"};
    } else if constexpr (B == SerializationBlocker::IsPolymorphic) {
        return CompileString{"!serial:poly"};
    } else if constexpr (B == SerializationBlocker::HasPlatformDependentSize) {
        return CompileString{"!serial:platform"};
    } else if constexpr (B == SerializationBlocker::PlatformMismatch) {
        return CompileString{"!serial:mismatch"};
    } else if constexpr (B == SerializationBlocker::HasNonSerializableMember) {
        return CompileString{"!serial:member"};
    } else if constexpr (B == SerializationBlocker::HasBitField) {
        return CompileString{"!serial:bitfield"};
    } else {
        return CompileString{"!serial:unknown"};
    }
}

// =========================================================================
// Type Traits for Serialization
// =========================================================================

/// Check if type is a pointer (excluding member pointers for now)
template <typename T>
struct is_pointer_type : std::bool_constant<
    std::is_pointer_v<T> || 
    std::is_member_pointer_v<T> ||
    std::is_null_pointer_v<T>
> {};

template <typename T>
inline constexpr bool is_pointer_type_v = is_pointer_type<T>::value;

/// Check if type is a reference
template <typename T>
struct is_reference_type : std::bool_constant<
    std::is_reference_v<T>
> {};

template <typename T>
inline constexpr bool is_reference_type_v = is_reference_type<T>::value;

/// Check if type has platform-dependent size (narrow: long only)
/// 'long' has different sizes on Windows (4 bytes) vs Linux (8 bytes) on 64-bit
template <typename T>
struct is_platform_dependent_size : std::false_type {};

template <>
struct is_platform_dependent_size<long> : std::true_type {};

template <>
struct is_platform_dependent_size<unsigned long> : std::true_type {};

template <typename T>
inline constexpr bool is_platform_dependent_size_v = is_platform_dependent_size<T>::value;

// =========================================================================
// Platform Dependency Check (Broad: includes wchar_t, long double, pointers)
// =========================================================================

namespace detail {

/// Helper to detect if T is exactly long (not an alias like int64_t)
/// On LP64 (Linux 64-bit), long and int64_t are the same type
/// We use a name-based approach: if sizeof matches but type is not explicitly "long"
template <typename T>
struct is_exactly_long_type : std::false_type {};

// Specialize for long only
template <>
struct is_exactly_long_type<long> : std::true_type {};

template <>
struct is_exactly_long_type<unsigned long> : std::true_type {};

// Helper to check if type is a fundamental platform-dependent type
// Note: on LP64 systems, int64_t IS long, so we can't distinguish them
// Instead we just mark wchar_t and long double as platform-dependent fundamentals
template <typename T>
struct is_fundamental_platform_dependent : std::false_type {};

template <>
struct is_fundamental_platform_dependent<wchar_t> : std::true_type {};

template <>
struct is_fundamental_platform_dependent<long double> : std::true_type {};

} // namespace detail

/// Check if type is platform-dependent (broad check for portability)
/// Includes: wchar_t, long double, pointers, references
/// NOTE: long/unsigned long are NOT flagged here to avoid false positives with int64_t on LP64
/// Use is_platform_dependent_size_v to specifically check for long types
template <typename T>
struct is_platform_dependent : std::false_type {};

// wchar_t (different sizes on Windows vs Linux)
template <>
struct is_platform_dependent<wchar_t> : std::true_type {};

// long double (different sizes on different platforms)
template <>
struct is_platform_dependent<long double> : std::true_type {};

// Pointers
template <typename T>
struct is_platform_dependent<T*> : std::true_type {};

// Member pointers
template <typename T, typename C>
struct is_platform_dependent<T C::*> : std::true_type {};

// Function pointers
template <typename R, typename... Args>
struct is_platform_dependent<R(*)(Args...)> : std::true_type {};

// References (contain addresses internally)
template <typename T>
struct is_platform_dependent<T&> : std::true_type {};

template <typename T>
struct is_platform_dependent<T&&> : std::true_type {};

// CV-qualified versions - strip cv and check base type
template <typename T>
    requires (!std::is_same_v<T, std::remove_cv_t<T>>)
struct is_platform_dependent<T> : is_platform_dependent<std::remove_cv_t<T>> {};

// Array types - check element type
template <typename T, std::size_t N>
struct is_platform_dependent<T[N]> : is_platform_dependent<T> {};

// Unbounded arrays
template <typename T>
struct is_platform_dependent<T[]> : is_platform_dependent<T> {};

template <typename T>
inline constexpr bool is_platform_dependent_v = is_platform_dependent<T>::value;

// =========================================================================
// Primary Serialization Check (Non-Reflection, Type-Level)
// =========================================================================

/// Base case: check fundamental serialization requirements
template <typename T, PlatformSet P>
struct basic_serialization_check {
    static consteval SerializationBlocker check() noexcept {
        using DecayedT = std::remove_cv_t<T>;
        
        // Check platform match first
        if constexpr (!PlatformSet::current_matches(P)) {
            return SerializationBlocker::PlatformMismatch;
        }
        // Check trivially copyable
        else if constexpr (!std::is_trivially_copyable_v<DecayedT>) {
            return SerializationBlocker::NotTriviallyCopyable;
        }
        // Check pointer types
        else if constexpr (is_pointer_type_v<DecayedT>) {
            return SerializationBlocker::HasPointer;
        }
        // Check reference types
        else if constexpr (is_reference_type_v<DecayedT>) {
            return SerializationBlocker::HasReference;
        }
        // Check polymorphic types
        else if constexpr (std::is_polymorphic_v<DecayedT>) {
            return SerializationBlocker::IsPolymorphic;
        }
        // Reject wchar_t (different size on Windows vs Linux)
        // Note: wchar_t is 2 bytes on Windows, 4 bytes on Linux
        else if constexpr (std::is_same_v<DecayedT, wchar_t>) {
            return SerializationBlocker::HasPlatformDependentSize;
        }
        // Reject long double (different size/representation on different platforms)
        // Note: long double is 8/10/12/16 bytes depending on platform and ABI
        else if constexpr (std::is_same_v<DecayedT, long double>) {
            return SerializationBlocker::HasPlatformDependentSize;
        }
        // Note: We do NOT reject 'long' here because:
        // 1. On LP64 systems (Linux 64-bit), int64_t is a typedef for long
        // 2. Rejecting long would incorrectly reject int64_t
        // 3. For true cross-platform serialization, users should use fixed-width types
        // 4. is_platform_dependent_size_v<long> can be used for explicit warnings
        else {
            return SerializationBlocker::None;
        }
    }
    
    static constexpr SerializationBlocker value = check();
};

/// Check if a fundamental type is serializable for platform set P
template <typename T, PlatformSet P>
inline constexpr bool is_basic_serializable_v = 
    (basic_serialization_check<T, P>::value == SerializationBlocker::None);

// =========================================================================
// Platform Set String Generation
// =========================================================================

/// Generate platform prefix string for signature
template <PlatformSet P>
consteval auto platform_prefix_string() noexcept {
    if constexpr (P.bit_width == BitWidth::Bits64 && P.endianness == Endianness::Little) {
        return CompileString{"[64-le]"};
    } else if constexpr (P.bit_width == BitWidth::Bits64 && P.endianness == Endianness::Big) {
        return CompileString{"[64-be]"};
    } else if constexpr (P.bit_width == BitWidth::Bits32 && P.endianness == Endianness::Little) {
        return CompileString{"[32-le]"};
    } else {
        return CompileString{"[32-be]"};
    }
}

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_UTIL_PLATFORM_SET_HPP
