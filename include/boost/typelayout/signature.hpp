// Boost.TypeLayout
//
// Signature Generation Public API (Legacy compatibility header)
//
// DEPRECATED: Consider using <boost/typelayout/typelayout.hpp> for core,
// or <boost/typelayout/typelayout_all.hpp> for all features.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_SIGNATURE_HPP
#define BOOST_TYPELAYOUT_SIGNATURE_HPP

// Include the new core signature module
#include <boost/typelayout/core/signature.hpp>
#include <boost/typelayout/core/verification.hpp>
#include <boost/typelayout/core/concepts.hpp>

// Include utility for serialization checking
#include <boost/typelayout/util/serialization_check.hpp>

namespace boost {
namespace typelayout {

    // =========================================================================
    // Legacy API Compatibility
    // =========================================================================
    
    /// DEPRECATED: Use is_serializable_v<T> instead
    /// Check if type is portable (no pointers, references, platform-dependent types)
    template <typename T>
    [[nodiscard]] [[deprecated("Use is_serializable_v<T> instead")]]
    consteval bool is_portable() noexcept {
        return is_serializable_v<T>;
    }

    /// DEPRECATED: Use Serializable<T> concept instead
    template <typename T>
    [[deprecated("Use Serializable<T> concept instead")]]
    inline constexpr bool is_portable_v = is_serializable_v<T>;

} // namespace typelayout
} // namespace boost

/// Bind type to expected signature - fails at compile time if layout differs
/// The signature includes architecture prefix (e.g., [64-le])
#define TYPELAYOUT_BIND(Type, ExpectedSig) \
    static_assert(::boost::typelayout::get_layout_signature<Type>() == ExpectedSig, \
                  "Layout mismatch for " #Type)

#endif // BOOST_TYPELAYOUT_SIGNATURE_HPP