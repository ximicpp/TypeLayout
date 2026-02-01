// Boost.TypeLayout
//
// Serialization Signature - Layer 2 signature generation for serialization compatibility
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_DETAIL_SERIALIZATION_SIGNATURE_HPP
#define BOOST_TYPELAYOUT_DETAIL_SERIALIZATION_SIGNATURE_HPP

#include <boost/typelayout/detail/config.hpp>
#include <boost/typelayout/detail/serialization_traits.hpp>
#include <boost/typelayout/detail/compile_string.hpp>
#include <experimental/meta>
#include <type_traits>
#include <cstdint>

namespace boost {
namespace typelayout {
namespace detail {

// =========================================================================
// Forward Declarations
// =========================================================================

template <typename T, PlatformSet P>
struct is_serializable;

// =========================================================================
// Get member count (same pattern as reflection_helpers.hpp)
// =========================================================================

template <typename T>
consteval std::size_t get_serialization_member_count() noexcept {
    using namespace std::meta;
    return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
}

// =========================================================================
// Per-Member Serialization Check (Uses P2996)
// =========================================================================

/// Check a single member at Index for serializability  
template <typename T, PlatformSet P, std::size_t Index>
consteval SerializationBlocker check_member_at_index() noexcept {
    using namespace std::meta;
    constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
    using MemberType = [:type_of(member):];
    using DecayedType = std::remove_cv_t<MemberType>;
    
    // Step 1: Check basic serialization requirements for this type
    constexpr auto basic = basic_serialization_check<DecayedType, P>::value;
    if constexpr (basic != SerializationBlocker::None) {
        return basic;
    }
    
    // Step 2: For class types, recursively check members
    if constexpr (std::is_class_v<DecayedType> && 
                  !std::is_fundamental_v<DecayedType> && 
                  !std::is_enum_v<DecayedType>) {
        return is_serializable<DecayedType, P>::blocker;
    }
    
    return SerializationBlocker::None;
}

// =========================================================================
// Recursive Member Checking (Template Recursion)
// =========================================================================

/// Recursive template to check all members
template <typename T, PlatformSet P, std::size_t Count, std::size_t Index = 0>
struct check_members_recursive {
    static consteval SerializationBlocker check() noexcept {
        constexpr auto current = check_member_at_index<T, P, Index>();
        if constexpr (current != SerializationBlocker::None) {
            return current;
        } else {
            return check_members_recursive<T, P, Count, Index + 1>::check();
        }
    }
    static constexpr SerializationBlocker value = check();
};

/// Base case: all members checked
template <typename T, PlatformSet P, std::size_t Count>
struct check_members_recursive<T, P, Count, Count> {
    static consteval SerializationBlocker check() noexcept {
        return SerializationBlocker::None;
    }
    static constexpr SerializationBlocker value = SerializationBlocker::None;
};

/// Entry point: check all members of a class (uses compile-time constant for count)
template <typename T, PlatformSet P>
struct check_all_members_helper {
    static constexpr std::size_t count = get_serialization_member_count<T>();
    static constexpr SerializationBlocker value = 
        check_members_recursive<T, P, count>::value;
};

// =========================================================================
// Array Element Check
// =========================================================================

template <typename T, PlatformSet P>
consteval SerializationBlocker check_array_element() noexcept {
    if constexpr (std::is_array_v<T>) {
        using ElementType = std::remove_all_extents_t<T>;
        constexpr auto basic = basic_serialization_check<ElementType, P>::value;
        if constexpr (basic != SerializationBlocker::None) {
            return basic;
        }
        if constexpr (std::is_class_v<ElementType>) {
            return is_serializable<ElementType, P>::blocker;
        }
    }
    return SerializationBlocker::None;
}

// =========================================================================
// Primary Serializability Trait
// =========================================================================

template <typename T, PlatformSet P>
struct is_serializable {
private:
    static consteval SerializationBlocker compute_blocker() noexcept {
        using DecayedT = std::remove_cv_t<T>;
        
        // Step 1: Basic type-level checks (pointer, ref, polymorphic, trivially copyable)
        constexpr auto basic = basic_serialization_check<DecayedT, P>::value;
        if constexpr (basic != SerializationBlocker::None) {
            return basic;
        }
        
        // Step 2: For arrays, check element type
        if constexpr (std::is_array_v<DecayedT>) {
            constexpr auto arr_check = check_array_element<DecayedT, P>();
            if constexpr (arr_check != SerializationBlocker::None) {
                return arr_check;
            }
        }
        
        // Step 3: For class types, recursively check members
        if constexpr (std::is_class_v<DecayedT>) {
            return check_all_members_helper<DecayedT, P>::value;
        }
        
        return SerializationBlocker::None;
    }
    
public:
    static constexpr SerializationBlocker blocker = compute_blocker();
    static constexpr bool value = (blocker == SerializationBlocker::None);
};

template <typename T, PlatformSet P>
inline constexpr bool is_serializable_v = is_serializable<T, P>::value;

// =========================================================================
// Serialization Signature String Generation
// =========================================================================

/// Generate a diagnostic string for the serialization status
template <typename T, PlatformSet P>
consteval auto serialization_status_string() noexcept {
    constexpr auto blocker = is_serializable<T, P>::blocker;
    return blocker_to_string<blocker>();
}

/// Generate full serialization signature with platform prefix
template <typename T, PlatformSet P>
consteval auto make_serialization_signature() noexcept {
    constexpr auto platform = platform_prefix_string<P>();
    constexpr auto status = serialization_status_string<T, P>();
    return platform + status;
}

} // namespace detail

// =========================================================================
// Public API
// =========================================================================

/// Check if type T is serializable for platform set P
template <typename T, PlatformSet P = PlatformSet::current()>
inline constexpr bool is_serializable_v = detail::is_serializable_v<T, P>;

/// Get the serialization blocker for type T
template <typename T, PlatformSet P = PlatformSet::current()>
inline constexpr SerializationBlocker serialization_blocker_v = 
    detail::is_serializable<T, P>::blocker;

/// Generate serialization signature string
template <typename T, PlatformSet P = PlatformSet::current()>
consteval auto serialization_signature() noexcept {
    return detail::make_serialization_signature<T, P>();
}

/// Check serialization compatibility between two types for platform set P
template <typename T, typename U, PlatformSet P = PlatformSet::current()>
consteval bool check_serialization_compatible() noexcept {
    // Both types must be serializable
    if constexpr (!is_serializable_v<T, P> || !is_serializable_v<U, P>) {
        return false;
    }
    
    // For serialization compatibility, we require:
    // 1. Same size
    // 2. Same alignment (for safety)
    return sizeof(T) == sizeof(U) && alignof(T) == alignof(U);
}

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_DETAIL_SERIALIZATION_SIGNATURE_HPP