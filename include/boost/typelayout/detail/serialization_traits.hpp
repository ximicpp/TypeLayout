// Boost.TypeLayout
//
// Serialization Traits - Compile-time serialization compatibility checks
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_DETAIL_SERIALIZATION_TRAITS_HPP
#define BOOST_TYPELAYOUT_DETAIL_SERIALIZATION_TRAITS_HPP

#include <boost/typelayout/detail/config.hpp>
#include <boost/typelayout/detail/compile_string.hpp>
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

/// Platform set configuration - must be specified for serialization checks
struct PlatformSet {
    BitWidth bit_width;
    Endianness endianness;
    bool allow_platform_dependent_long;  // false = reject 'long', true = allow if size matches
    
    // Predefined platform sets
    static constexpr PlatformSet x64_le() noexcept {
        return PlatformSet{BitWidth::Bits64, Endianness::Little, false};
    }
    
    static constexpr PlatformSet x86_le() noexcept {
        return PlatformSet{BitWidth::Bits32, Endianness::Little, false};
    }
    
    static constexpr PlatformSet arm64_le() noexcept {
        return PlatformSet{BitWidth::Bits64, Endianness::Little, false};
    }
    
    static constexpr PlatformSet current() noexcept {
        constexpr BitWidth bw = (sizeof(void*) == 8) ? BitWidth::Bits64 : BitWidth::Bits32;
        constexpr Endianness en = 
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            Endianness::Big;
#else
            Endianness::Little;
#endif
        return PlatformSet{bw, en, true};  // Allow platform-dependent types on current platform
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
    None = 0,           // Type is serializable
    NotTriviallyCopyable = 1,
    HasPointer = 2,
    HasReference = 3,
    IsPolymorphic = 4,
    HasPlatformDependentSize = 5,
    PlatformMismatch = 6,
    HasNonSerializableMember = 7
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

/// Check if type has platform-dependent size
/// 'long' has different sizes on Windows (4 bytes) vs Linux (8 bytes)
template <typename T>
struct is_platform_dependent_size : std::false_type {};

template <>
struct is_platform_dependent_size<long> : std::true_type {};

template <>
struct is_platform_dependent_size<unsigned long> : std::true_type {};

template <typename T>
inline constexpr bool is_platform_dependent_size_v = is_platform_dependent_size<T>::value;

// =========================================================================
// Primary Serialization Check (Non-Reflection, Type-Level)
// =========================================================================

/// Base case: check fundamental serialization requirements
template <typename T, PlatformSet P>
struct basic_serialization_check {
    static consteval SerializationBlocker check() noexcept {
        // Check platform match first
        if constexpr (!PlatformSet::current_matches(P)) {
            return SerializationBlocker::PlatformMismatch;
        }
        // Check trivially copyable
        else if constexpr (!std::is_trivially_copyable_v<T>) {
            return SerializationBlocker::NotTriviallyCopyable;
        }
        // Check pointer types
        else if constexpr (is_pointer_type_v<T>) {
            return SerializationBlocker::HasPointer;
        }
        // Check reference types
        else if constexpr (is_reference_type_v<T>) {
            return SerializationBlocker::HasReference;
        }
        // Check polymorphic types
        else if constexpr (std::is_polymorphic_v<T>) {
            return SerializationBlocker::IsPolymorphic;
        }
        // Check platform-dependent sizes (if not allowed)
        else if constexpr (!P.allow_platform_dependent_long && is_platform_dependent_size_v<T>) {
            return SerializationBlocker::HasPlatformDependentSize;
        }
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

#endif // BOOST_TYPELAYOUT_DETAIL_SERIALIZATION_TRAITS_HPP