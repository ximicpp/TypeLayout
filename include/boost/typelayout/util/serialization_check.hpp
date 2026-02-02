// Boost.TypeLayout
//
// Utility: Serialization Check - Layer 2 serialization compatibility checking
//
// This is a UTILITY module built on top of core layout signature functionality.
// It demonstrates how to use layout signatures for practical serialization safety.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_UTIL_SERIALIZATION_CHECK_HPP
#define BOOST_TYPELAYOUT_UTIL_SERIALIZATION_CHECK_HPP

#include <boost/typelayout/util/platform_set.hpp>
#include <boost/typelayout/core/config.hpp>
#include <experimental/meta>
#include <type_traits>
#include <cstdint>
#include <variant>
#include <optional>

namespace boost {
namespace typelayout {
namespace detail {

// =========================================================================
// Forward Declarations
// =========================================================================

template <typename T, PlatformSet P>
struct is_serializable;

// =========================================================================
// Base Class Iteration (for inheritance support)
// =========================================================================

template <typename T>
consteval std::size_t get_base_count() noexcept {
    using namespace std::meta;
    return bases_of(^^T, access_context::unchecked()).size();
}

template <typename T, std::size_t Index>
consteval auto get_base_type_at() noexcept {
    using namespace std::meta;
    constexpr auto base = bases_of(^^T, access_context::unchecked())[Index];
    return type_of(base);
}

// =========================================================================
// Get member count (same pattern as reflection_helpers.hpp)
// =========================================================================

template <typename T>
consteval std::size_t get_serialization_member_count() noexcept {
    using namespace std::meta;
    return nonstatic_data_members_of(^^T, access_context::unchecked()).size();
}

// =========================================================================
// Bit-field Detection (Uses P2996)
// =========================================================================

/// Check if a single member is a bit-field
template <typename T, std::size_t Index>
consteval bool check_member_is_bitfield() noexcept {
    using namespace std::meta;
    constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
    return is_bit_field(member);
}

// Forward declaration for recursion
template <typename T>
consteval bool has_any_bitfield() noexcept;

/// Recursive check for base classes bit-fields
template <typename T, std::size_t BaseCount, std::size_t Index = 0>
struct check_base_bitfields_recursive {
    static consteval bool check() noexcept {
        using namespace std::meta;
        constexpr auto base = bases_of(^^T, access_context::unchecked())[Index];
        using BaseType = [:type_of(base):];
        
        if constexpr (has_any_bitfield<BaseType>()) {
            return true;
        }
        return check_base_bitfields_recursive<T, BaseCount, Index + 1>::check();
    }
    static constexpr bool value = check();
};

template <typename T, std::size_t BaseCount>
struct check_base_bitfields_recursive<T, BaseCount, BaseCount> {
    static consteval bool check() noexcept { return false; }
    static constexpr bool value = false;
};

/// Recursive check for bit-fields in members
template <typename T, std::size_t Count, std::size_t Index = 0>
struct check_bitfields_recursive {
    static consteval bool check() noexcept {
        if constexpr (check_member_is_bitfield<T, Index>()) {
            return true;
        } else {
            using namespace std::meta;
            constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
            using MemberType = [:type_of(member):];
            using DecayedType = std::remove_cv_t<MemberType>;
            
            // Handle array members
            if constexpr (std::is_array_v<DecayedType>) {
                using ElementType = std::remove_all_extents_t<DecayedType>;
                if constexpr (has_any_bitfield<ElementType>()) {
                    return true;
                }
            }
            // Handle nested class/struct members
            else if constexpr (std::is_class_v<DecayedType>) {
                if constexpr (has_any_bitfield<DecayedType>()) {
                    return true;
                }
            }
            return check_bitfields_recursive<T, Count, Index + 1>::check();
        }
    }
    static constexpr bool value = check();
};

template <typename T, std::size_t Count>
struct check_bitfields_recursive<T, Count, Count> {
    static consteval bool check() noexcept { return false; }
    static constexpr bool value = false;
};

template <typename T>
consteval bool has_any_bitfield() noexcept {
    using DecayedT = std::remove_cv_t<T>;
    
    // Handle arrays: check element type
    if constexpr (std::is_array_v<DecayedT>) {
        using ElementType = std::remove_all_extents_t<DecayedT>;
        return has_any_bitfield<ElementType>();
    }
    // Handle class/struct types
    else if constexpr (std::is_class_v<DecayedT> || std::is_union_v<DecayedT>) {
        // First check base classes for bit-fields
        constexpr std::size_t base_count = get_base_count<DecayedT>();
        if constexpr (base_count > 0) {
            if constexpr (check_base_bitfields_recursive<DecayedT, base_count>::value) {
                return true;
            }
        }
        
        // Then check own members
        constexpr std::size_t count = get_serialization_member_count<DecayedT>();
        if constexpr (count > 0) {
            return check_bitfields_recursive<DecayedT, count>::value;
        }
    }
    return false;
}

// =========================================================================
// Per-Member Serialization Check (Uses P2996)
// =========================================================================

template <typename T, PlatformSet P, std::size_t Index>
consteval SerializationBlocker check_member_at_index() noexcept {
    using namespace std::meta;
    constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
    using MemberType = [:type_of(member):];
    using DecayedType = std::remove_cv_t<MemberType>;
    
    // Handle arrays: check element type
    if constexpr (std::is_array_v<DecayedType>) {
        using ElementType = std::remove_all_extents_t<DecayedType>;
        
        constexpr auto elem_basic = basic_serialization_check<ElementType, P>::value;
        if constexpr (elem_basic != SerializationBlocker::None) {
            return elem_basic;
        }
        
        // Recursively check element type if it's a class
        if constexpr (std::is_class_v<ElementType> && 
                      !std::is_fundamental_v<ElementType> && 
                      !std::is_enum_v<ElementType>) {
            return is_serializable<ElementType, P>::blocker;
        }
        
        return SerializationBlocker::None;
    }
    
    // Check basic serialization requirements
    constexpr auto basic = basic_serialization_check<DecayedType, P>::value;
    if constexpr (basic != SerializationBlocker::None) {
        return basic;
    }
    
    // Recursively check nested class/struct types
    if constexpr (std::is_class_v<DecayedType> && 
                  !std::is_fundamental_v<DecayedType> && 
                  !std::is_enum_v<DecayedType>) {
        return is_serializable<DecayedType, P>::blocker;
    }
    
    // Recursively check nested union types
    if constexpr (std::is_union_v<DecayedType>) {
        return is_serializable<DecayedType, P>::blocker;
    }
    
    return SerializationBlocker::None;
}

// =========================================================================
// Recursive Member Checking
// =========================================================================

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

template <typename T, PlatformSet P, std::size_t Count>
struct check_members_recursive<T, P, Count, Count> {
    static consteval SerializationBlocker check() noexcept {
        return SerializationBlocker::None;
    }
    static constexpr SerializationBlocker value = SerializationBlocker::None;
};

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
// Base Class Serialization Check
// =========================================================================

template <typename T, PlatformSet P, std::size_t BaseCount, std::size_t Index = 0>
struct check_base_serializable_recursive {
    static consteval SerializationBlocker check() noexcept {
        using namespace std::meta;
        constexpr auto base = bases_of(^^T, access_context::unchecked())[Index];
        using BaseType = [:type_of(base):];
        
        constexpr auto base_blocker = is_serializable<BaseType, P>::blocker;
        if constexpr (base_blocker != SerializationBlocker::None) {
            return base_blocker;
        }
        return check_base_serializable_recursive<T, P, BaseCount, Index + 1>::check();
    }
    static constexpr SerializationBlocker value = check();
};

template <typename T, PlatformSet P, std::size_t BaseCount>
struct check_base_serializable_recursive<T, P, BaseCount, BaseCount> {
    static consteval SerializationBlocker check() noexcept {
        return SerializationBlocker::None;
    }
    static constexpr SerializationBlocker value = SerializationBlocker::None;
};

// =========================================================================
// Primary Serializability Trait
// =========================================================================

// =========================================================================
// Runtime State Type Detection
// =========================================================================

// Forward declare is_runtime_state_type for use in compute_blocker
template <typename T>
struct is_runtime_state_type : std::false_type {};

// std::variant has runtime state (_index determines which member is active)
// memcpy cannot correctly preserve object lifetime semantics
template <typename... Types>
struct is_runtime_state_type<std::variant<Types...>> : std::true_type {};

// std::optional has runtime state (_engaged flag determines if value is valid)
// memcpy cannot correctly preserve object lifetime semantics  
template <typename T>
struct is_runtime_state_type<std::optional<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_runtime_state_type_v = is_runtime_state_type<T>::value;

// =========================================================================
// Primary Serializability Trait
// =========================================================================

template <typename T, PlatformSet P>
struct is_serializable {
private:
    static consteval SerializationBlocker compute_blocker() noexcept {
        using DecayedT = std::remove_cv_t<T>;
        
        constexpr auto basic = basic_serialization_check<DecayedT, P>::value;
        if constexpr (basic != SerializationBlocker::None) {
            return basic;
        }
        
        // Check for runtime state types (std::variant, std::optional)
        // These types have runtime state that cannot be safely memcpy'd
        if constexpr (is_runtime_state_type_v<DecayedT>) {
            return SerializationBlocker::HasRuntimeState;
        }
        
        // Note: Bit-fields are ALLOWED for serialization
        // The signature includes bit positions, and signature comparison
        // will detect any layout incompatibilities between platforms
        
        if constexpr (std::is_array_v<DecayedT>) {
            constexpr auto arr_check = check_array_element<DecayedT, P>();
            if constexpr (arr_check != SerializationBlocker::None) {
                return arr_check;
            }
        }
        
        // Handle unions (std::is_union_v is true, std::is_class_v is false)
        if constexpr (std::is_union_v<DecayedT>) {
            // Unions have no bases, only check members
            return check_all_members_helper<DecayedT, P>::value;
        }
        
        if constexpr (std::is_class_v<DecayedT>) {
            // First check base classes
            constexpr std::size_t base_count = get_base_count<DecayedT>();
            if constexpr (base_count > 0) {
                constexpr auto base_blocker = check_base_serializable_recursive<DecayedT, P, base_count>::value;
                if constexpr (base_blocker != SerializationBlocker::None) {
                    return base_blocker;
                }
            }
            
            // Then check own members
            return check_all_members_helper<DecayedT, P>::value;
        }
        
        return SerializationBlocker::None;
    }
    
public:
    static constexpr SerializationBlocker blocker = compute_blocker();
    static constexpr bool value = (blocker == SerializationBlocker::None);
};

template <typename T, PlatformSet P>
inline constexpr bool is_serializable_impl_v = is_serializable<T, P>::value;

// =========================================================================
// Serialization Status String Generation
// =========================================================================

template <typename T, PlatformSet P>
consteval auto serialization_status_string() noexcept {
    constexpr auto blocker = is_serializable<T, P>::blocker;
    return blocker_to_string<blocker>();
}

template <typename T, PlatformSet P>
consteval auto make_serialization_status() noexcept {
    constexpr auto platform = platform_prefix_string<P>();
    constexpr auto status = serialization_status_string<T, P>();
    return platform + status;
}

} // namespace detail

// =========================================================================
// Public API (Utility)
// =========================================================================

/// Check if type T is serializable for platform set P
template <typename T, PlatformSet P = PlatformSet::current()>
inline constexpr bool is_serializable_v = detail::is_serializable_impl_v<T, P>;

/// Get the serialization blocker for type T
template <typename T, PlatformSet P = PlatformSet::current()>
inline constexpr SerializationBlocker serialization_blocker_v = 
    detail::is_serializable<T, P>::blocker;

/// Generate serialization status string (NOT a full layout signature)
/// Returns status like "[64-le]serial" or "[64-le]!serial:ptr"
template <typename T, PlatformSet P = PlatformSet::current()>
consteval auto serialization_status() noexcept {
    return detail::make_serialization_status<T, P>();
}

/// Check serialization compatibility between two types for platform set P
template <typename T, typename U, PlatformSet P = PlatformSet::current()>
consteval bool check_serialization_compatible() noexcept {
    if constexpr (!is_serializable_v<T, P> || !is_serializable_v<U, P>) {
        return false;
    }
    return sizeof(T) == sizeof(U) && alignof(T) == alignof(U);
}

/// Check if a type contains any bit-fields (utility function)
template <typename T>
consteval bool has_bitfields() noexcept {
    return detail::has_any_bitfield<T>();
}

/// Variable template for has_bitfields
template <typename T>
inline constexpr bool has_bitfields_v = detail::has_any_bitfield<T>();

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_UTIL_SERIALIZATION_CHECK_HPP
