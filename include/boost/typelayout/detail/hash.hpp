// Boost.TypeLayout
//
// Hash Algorithms (FNV-1a, DJB2)
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_DETAIL_HASH_HPP
#define BOOST_TYPELAYOUT_DETAIL_HASH_HPP

#include <boost/typelayout/detail/config.hpp>

namespace boost {
namespace typelayout {

    // =========================================================================
    // FNV-1a 64-bit Hash
    // =========================================================================

    // FNV-1a 64-bit hash
    [[nodiscard]] consteval uint64_t fnv1a_hash(const char* str, size_t len) noexcept {
        constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ull;
        constexpr uint64_t FNV_PRIME = 1099511628211ull;
        
        uint64_t hash = FNV_OFFSET_BASIS;
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(str[i]));
            hash *= FNV_PRIME;
        }
        return hash;
    }

    // =========================================================================
    // DJB2 64-bit Hash
    // =========================================================================

    // DJB2 64-bit hash (different algorithm for dual-hash verification)
    [[nodiscard]] consteval uint64_t djb2_hash(const char* str, size_t len) noexcept {
        uint64_t hash = 5381;
        for (size_t i = 0; i < len; ++i) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(str[i]); // hash * 33 + c
        }
        return hash;
    }

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_DETAIL_HASH_HPP
