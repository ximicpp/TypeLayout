// Boost.TypeLayout - Hash Utilities (FNV-1a, DJB2)
//
// Provides compile-time hash functions for layout signatures.
// This is a utility module, not part of the core signature generation.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_UTILS_HASH_HPP
#define BOOST_TYPELAYOUT_UTILS_HASH_HPP

#include <boost/typelayout/core/config.hpp>

namespace boost {
namespace typelayout {

// =============================================================================
// FNV-1a Hash Constants
// =============================================================================

inline constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ull;
inline constexpr uint64_t FNV_PRIME = 1099511628211ull;

// =============================================================================
// String-based hash functions (for signature string hashing)
// =============================================================================

/**
 * @brief FNV-1a hash of a string buffer.
 * Used to compute layout hashes from signature strings.
 */
[[nodiscard]] consteval uint64_t fnv1a_hash(const char* str, size_t len) noexcept {
    uint64_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(str[i]));
        hash *= FNV_PRIME;
    }
    return hash;
}

/**
 * @brief DJB2 hash of a string buffer.
 * Alternative hash algorithm, useful for hash table applications.
 */
[[nodiscard]] consteval uint64_t djb2_hash(const char* str, size_t len) noexcept {
    uint64_t hash = 5381;
    for (size_t i = 0; i < len; ++i)
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(str[i]);
    return hash;
}

// =============================================================================
// Incremental Hash State (for streaming layout data)
// =============================================================================

/**
 * @brief FNV-1a incremental hash state for compile-time streaming.
 * 
 * Allows building a hash incrementally by feeding bytes/values one at a time.
 * Useful for scenarios where building a complete string first is impractical.
 * 
 * @note Currently not used in production due to P2996 toolchain limitations,
 * but preserved for future optimization when the toolchain improves.
 */
struct FNV1aState {
    uint64_t hash = FNV_OFFSET_BASIS;
    
    constexpr void update_byte(uint8_t b) noexcept {
        hash ^= static_cast<uint64_t>(b);
        hash *= FNV_PRIME;
    }
    
    constexpr void update_u64(uint64_t value) noexcept {
        for (int i = 0; i < 8; ++i) {
            update_byte(static_cast<uint8_t>(value & 0xFF));
            value >>= 8;
        }
    }
    
    constexpr void update_size(size_t value) noexcept {
        if constexpr (sizeof(size_t) == 8) {
            update_u64(value);
        } else {
            for (int i = 0; i < 4; ++i) {
                update_byte(static_cast<uint8_t>(value & 0xFF));
                value >>= 8;
            }
        }
    }
    
    constexpr void update_tag(uint8_t tag) noexcept {
        update_byte(tag);
    }
    
    constexpr void combine(uint64_t other_hash) noexcept {
        update_u64(other_hash);
    }
    
    [[nodiscard]] constexpr uint64_t finalize() const noexcept {
        return hash;
    }
};

// =============================================================================
// Type Category Tags (for incremental hashing)
// =============================================================================

/**
 * @brief Binary tags representing type categories for hash computation.
 * 
 * These tags provide a compact binary representation of type categories,
 * useful for incremental hashing without string construction.
 * 
 * @note Reserved for future use when P2996 toolchain supports more efficient
 * constexpr evaluation.
 */
namespace hash_tags {
    // Architecture identifiers
    inline constexpr uint8_t ARCH_64_LE = 0x01;
    inline constexpr uint8_t ARCH_64_BE = 0x02;
    inline constexpr uint8_t ARCH_32_LE = 0x03;
    inline constexpr uint8_t ARCH_32_BE = 0x04;
    
    // Primitive types
    inline constexpr uint8_t TYPE_I8   = 0x10;
    inline constexpr uint8_t TYPE_U8   = 0x11;
    inline constexpr uint8_t TYPE_I16  = 0x12;
    inline constexpr uint8_t TYPE_U16  = 0x13;
    inline constexpr uint8_t TYPE_I32  = 0x14;
    inline constexpr uint8_t TYPE_U32  = 0x15;
    inline constexpr uint8_t TYPE_I64  = 0x16;
    inline constexpr uint8_t TYPE_U64  = 0x17;
    inline constexpr uint8_t TYPE_F32  = 0x18;
    inline constexpr uint8_t TYPE_F64  = 0x19;
    inline constexpr uint8_t TYPE_F80  = 0x1A;
    inline constexpr uint8_t TYPE_BOOL = 0x1B;
    inline constexpr uint8_t TYPE_CHAR = 0x1C;
    
    // Pointer and reference types
    inline constexpr uint8_t TYPE_PTR    = 0x30;
    inline constexpr uint8_t TYPE_REF    = 0x31;
    inline constexpr uint8_t TYPE_ARRAY  = 0x35;
    inline constexpr uint8_t TYPE_BYTES  = 0x36;
    
    // Composite types
    inline constexpr uint8_t TYPE_STRUCT = 0x40;
    inline constexpr uint8_t TYPE_CLASS  = 0x41;
    inline constexpr uint8_t TYPE_UNION  = 0x42;
    inline constexpr uint8_t TYPE_ENUM   = 0x43;
    
    // Flags and modifiers
    inline constexpr uint8_t FLAG_POLY   = 0x80;
    inline constexpr uint8_t FLAG_BASE   = 0x81;
    inline constexpr uint8_t FLAG_VBASE  = 0x82;
    inline constexpr uint8_t FLAG_BITS   = 0x83;
    
    // Member markers
    inline constexpr uint8_t MEMBER      = 0xF0;
    inline constexpr uint8_t BASE        = 0xF1;
}

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_UTILS_HASH_HPP
