// Boost.TypeLayout
//
// Utility Concepts: Serialization and Zero-Copy Transmittable Concepts
//
// These concepts are part of the UTILITY layer (Layer 2) and build upon
// the core layout signature engine for serialization safety checking.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_UTIL_CONCEPTS_HPP
#define BOOST_TYPELAYOUT_UTIL_CONCEPTS_HPP

#include <boost/typelayout/util/serialization_check.hpp>
#include <boost/typelayout/core/signature.hpp>
#include <boost/typelayout/core/concepts.hpp>
#include <concepts>
#include <type_traits>

namespace boost {
namespace typelayout {

/**
 * @defgroup serialization_concepts Serialization Concepts
 * @brief C++20 concepts for serialization safety checking.
 * @{
 */

// =========================================================================
// Serialization Concepts (Utility Layer)
// =========================================================================

/**
 * @brief Concept: Type is serializable for a given platform set.
 * 
 * A type satisfies this concept if it can be safely memcpy'd across 
 * process boundaries on the same platform configuration.
 * 
 * @tparam T The type to check
 * @tparam P Target platform set (default: current platform)
 * 
 * @par Requirements:
 * - Must be trivially copyable
 * - Must not contain pointers or references
 * - Must not be polymorphic (no virtual functions)
 * - Must not contain platform-dependent types (wchar_t, long double)
 * - Must not have runtime state (std::variant, std::optional)
 * - All members must recursively meet these requirements
 * 
 * @note Bit-fields are ALLOWED - the signature includes bit positions,
 * and signature comparison will detect any layout incompatibilities.
 * 
 * @par Example:
 * @code
 * template<Serializable T>
 * void send(const T& data);
 * 
 * struct Point { int x; int y; };
 * send(Point{10, 20}); // OK
 * @endcode
 */
template <typename T, PlatformSet P = PlatformSet::current()>
concept Serializable = is_serializable_v<T, P>;

/**
 * @brief Concept: Type is serializable for 64-bit little-endian platforms.
 * 
 * Convenience alias for the most common server platform (x86-64, ARM64 LE).
 * 
 * @tparam T The type to check
 */
template <typename T>
concept Serializable64LE = Serializable<T, PlatformSet::bits64_le()>;

/**
 * @brief Concept: Type is serializable for 32-bit little-endian platforms.
 * 
 * @tparam T The type to check
 */
template <typename T>
concept Serializable32LE = Serializable<T, PlatformSet::bits32_le()>;

// =========================================================================
// Zero-Copy Transmittable Concept
// =========================================================================

/// Concept: Type can be safely transmitted via zero-copy mechanisms
/// 
/// This is the strongest guarantee: the type can be safely memcpy'd between
/// different processes or machines with the same platform configuration.
///
/// Additional requirements beyond Serializable:
/// - Layout must be compatible (same size and alignment)
/// - Must be serializable
template <typename T, typename U, PlatformSet P = PlatformSet::current()>
concept ZeroCopyTransmittable = 
    Serializable<T, P> && 
    Serializable<U, P> &&
    LayoutCompatible<T, U>;

/// Concept: Same layout as target type and serializable
/// Useful for type-erasure scenarios where you receive data as bytes
template <typename T, typename Target, PlatformSet P = PlatformSet::current()>
concept ReceivableAs =
    Serializable<T, P> &&
    Serializable<Target, P> &&
    (sizeof(T) == sizeof(Target)) &&
    (alignof(T) >= alignof(Target));

// =========================================================================
// Combined Concepts for Common Use Cases
// =========================================================================

/// Concept: Type is safe for network transmission on 64-bit LE systems
template <typename T>
concept NetworkSafe = Serializable64LE<T>;

/// Concept: Type is safe for shared memory on current platform
template <typename T>
concept SharedMemorySafe = Serializable<T, PlatformSet::current()>;

/// Concept: Type has portable layout (matching between 32-bit and 64-bit)
/// Note: This is conservative - rejects types that might differ between platforms
template <typename T>
concept PortableLayout = 
    Serializable32LE<T> && 
    Serializable64LE<T>;

// =========================================================================
// Diagnostic Helpers
// =========================================================================

namespace detail {

/// Get a human-readable blocker reason
template <typename T, PlatformSet P = PlatformSet::current()>
consteval const char* get_blocker_reason() noexcept {
    constexpr auto blocker = serialization_blocker_v<T, P>;
    switch (blocker) {
        case SerializationBlocker::None: 
            return "none (type is serializable)";
        case SerializationBlocker::NotTriviallyCopyable:
            return "type is not trivially copyable";
        case SerializationBlocker::HasPointer:
            return "type contains a pointer";
        case SerializationBlocker::HasReference:
            return "type contains a reference";
        case SerializationBlocker::IsPolymorphic:
            return "type is polymorphic (has virtual functions)";
        case SerializationBlocker::HasPlatformDependentSize:
            return "type contains platform-dependent size (wchar_t, long double)";
        case SerializationBlocker::PlatformMismatch:
            return "current platform does not match target platform set";
        case SerializationBlocker::HasNonSerializableMember:
            return "type contains a non-serializable member";
        case SerializationBlocker::HasRuntimeState:
            return "type has runtime state (std::variant, std::optional)";
        default:
            return "unknown blocker";
    }
}

} // namespace detail

/// Get blocker reason for a type
template <typename T, PlatformSet P = PlatformSet::current()>
consteval const char* blocker_reason() noexcept {
    return detail::get_blocker_reason<T, P>();
}

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_UTIL_CONCEPTS_HPP
