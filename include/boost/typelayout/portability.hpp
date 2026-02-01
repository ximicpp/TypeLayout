// Boost.TypeLayout
//
// Serialization Safety Checking
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_PORTABILITY_HPP
#define BOOST_TYPELAYOUT_PORTABILITY_HPP

#include <boost/typelayout/detail/config.hpp>
#include <boost/typelayout/detail/reflection_helpers.hpp>

namespace boost {
namespace typelayout {

    // =========================================================================
    // Forward Declarations
    // =========================================================================
    
    /// Check if a type can be trivially serialized via memcpy and transmitted
    /// across process boundaries (no pointers, references, bit-fields, etc.)
    template <typename T>
    [[nodiscard]] consteval bool is_trivially_serializable() noexcept;
    
    /// Check if a type contains any bit-fields (direct or nested)
    template <typename T>
    [[nodiscard]] consteval bool has_bitfields() noexcept;

    // =========================================================================
    // Bit-field Detection
    // =========================================================================
    
    // Check if a single member is a bit-field
    template<typename T, std::size_t Index>
    consteval bool check_member_is_bitfield() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        return is_bit_field(member);
    }
    
    // Check if any member is a bit-field
    template<typename T, std::size_t... Indices>
    consteval bool check_any_member_is_bitfield(std::index_sequence<Indices...>) noexcept {
        return (check_member_is_bitfield<T, Indices>() || ...);
    }
    
    // Check if a nested type has bit-fields (for recursive checking)
    template<typename T, std::size_t Index>
    consteval bool check_member_has_bitfields() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];
        return has_bitfields<FieldType>();
    }
    
    // Check all members recursively for bit-fields
    template<typename T, std::size_t... Indices>
    consteval bool check_any_member_has_bitfields(std::index_sequence<Indices...>) noexcept {
        return (check_member_has_bitfields<T, Indices>() || ...);
    }
    
    // Check if a base class has bit-fields
    template<typename T, std::size_t Index>
    consteval bool check_base_has_bitfields() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[Index];
        using BaseType = [:type_of(base_info):];
        return has_bitfields<BaseType>();
    }
    
    // Check all base classes for bit-fields
    template<typename T, std::size_t... Indices>
    consteval bool check_any_base_has_bitfields(std::index_sequence<Indices...>) noexcept {
        return (check_base_has_bitfields<T, Indices>() || ...);
    }
    
    /// Check if a type contains any bit-fields (direct or nested)
    /// Bit-fields have implementation-defined layout and are NOT trivially serializable
    template <typename T>
    [[nodiscard]] consteval bool has_bitfields() noexcept {
        using CleanT = std::remove_cv_t<T>;
        
        // Arrays: check element type
        if constexpr (std::is_array_v<CleanT>) {
            using ElementType = std::remove_extent_t<CleanT>;
            return has_bitfields<ElementType>();
        }
        // Unions and structs: check members and bases
        else if constexpr (std::is_class_v<CleanT> || std::is_union_v<CleanT>) {
            constexpr std::size_t member_count = get_member_count<CleanT>();
            constexpr std::size_t base_count = std::is_union_v<CleanT> ? 0 : get_base_count<CleanT>();
            
            // Check direct bit-fields
            bool has_direct = false;
            if constexpr (member_count > 0) {
                has_direct = check_any_member_is_bitfield<CleanT>(
                    std::make_index_sequence<member_count>{});
            }
            if (has_direct) return true;
            
            // Check nested types for bit-fields
            bool has_nested = false;
            if constexpr (member_count > 0) {
                has_nested = check_any_member_has_bitfields<CleanT>(
                    std::make_index_sequence<member_count>{});
            }
            if (has_nested) return true;
            
            // Check base classes
            if constexpr (base_count > 0) {
                return check_any_base_has_bitfields<CleanT>(
                    std::make_index_sequence<base_count>{});
            }
            
            return false;
        }
        else {
            return false;  // Primitives, pointers, etc. have no bit-fields
        }
    }
    
    /// Helper variable template
    template <typename T>
    inline constexpr bool has_bitfields_v = has_bitfields<T>();

    // =========================================================================
    // Trivial Serialization Checking
    // =========================================================================

    // Check if a single member type is trivially serializable (recursive)
    template<typename T, std::size_t Index>
    consteval bool check_member_serializable() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];
        
        // Use is_trivially_serializable for recursive checking of nested structs
        return is_trivially_serializable<FieldType>();
    }

    // Check all members for serializability
    template<typename T, std::size_t... Indices>
    consteval bool check_all_members_serializable(std::index_sequence<Indices...>) noexcept {
        return (check_member_serializable<T, Indices>() && ...);
    }

    // Check if a single base class is trivially serializable (recursive)
    template<typename T, std::size_t Index>
    consteval bool check_base_serializable() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[Index];
        using BaseType = [:type_of(base_info):];
        
        // Recursively check the base class
        return is_trivially_serializable<BaseType>();
    }

    // Check all base classes for serializability
    template<typename T, std::size_t... Indices>
    consteval bool check_all_bases_serializable(std::index_sequence<Indices...>) noexcept {
        return (check_base_serializable<T, Indices>() && ...);
    }

    /// Check if a type can be trivially serialized via memcpy and transmitted
    /// across process boundaries.
    ///
    /// A type is trivially serializable if and only if:
    /// - It does not contain pointer types (T*, void*)
    /// - It does not contain reference types (T&, T&&)
    /// - It does not contain member pointers (T C::*)
    /// - It does not contain std::nullptr_t
    /// - It does not contain platform-dependent types (long, size_t, etc.)
    /// - It does not contain bit-fields
    /// - All nested members and base classes are also trivially serializable
    ///
    /// This is a utility check independent of the layout signature system.
    /// The signature system generates signatures for ANY type (including pointers);
    /// this function helps users filter types for cross-process scenarios.
    template <typename T>
    [[nodiscard]] consteval bool is_trivially_serializable() noexcept {
        // References are NEVER serializable (check before removing cv/ref)
        if constexpr (std::is_reference_v<T>) {
            return false;
        }
        
        // Strip CV qualifiers
        using CleanT = std::remove_cv_t<T>;
        
        // Pointers are NEVER serializable (different address spaces)
        if constexpr (std::is_pointer_v<CleanT>) {
            return false;
        }
        // Member pointers are also not serializable
        else if constexpr (std::is_member_pointer_v<CleanT>) {
            return false;
        }
        // Null pointer type is not serializable
        else if constexpr (std::is_null_pointer_v<CleanT>) {
            return false;
        }
        // Check if it's a platform-dependent primitive
        else if constexpr (is_platform_dependent_v<CleanT>) {
            return false;
        }
        // Check arrays recursively
        else if constexpr (std::is_array_v<CleanT>) {
            using ElementType = std::remove_extent_t<CleanT>;
            return is_trivially_serializable<ElementType>();
        }
        // Unions: we cannot know which member is active at runtime, but
        // we CAN check all members at compile time.
        // Conservative strategy: if ANY member is non-serializable, the union
        // is considered non-serializable (safer than missing a problem).
        else if constexpr (std::is_union_v<CleanT>) {
            // Bit-fields in unions are not serializable
            if (has_bitfields<CleanT>()) {
                return false;
            }
            constexpr std::size_t member_count = get_member_count<CleanT>();
            if constexpr (member_count == 0) {
                return true;
            } else {
                return check_all_members_serializable<CleanT>(
                    std::make_index_sequence<member_count>{});
            }
        }
        // Check structs/classes recursively (including base classes)
        else if constexpr (std::is_class_v<CleanT>) {
            // Bit-fields have implementation-defined layout - NOT serializable
            if (has_bitfields<CleanT>()) {
                return false;
            }
            
            // First check all base classes
            constexpr std::size_t base_count = get_base_count<CleanT>();
            bool bases_serializable = true;
            if constexpr (base_count > 0) {
                bases_serializable = check_all_bases_serializable<CleanT>(
                    std::make_index_sequence<base_count>{});
            }
            
            if (!bases_serializable) {
                return false;
            }
            
            // Then check all direct members
            constexpr std::size_t member_count = get_member_count<CleanT>();
            if constexpr (member_count == 0) {
                return true;
            } else {
                return check_all_members_serializable<CleanT>(
                    std::make_index_sequence<member_count>{});
            }
        }
        // All other types (primitives) are serializable
        else {
            return true;
        }
    }

    /// Helper variable template for is_trivially_serializable
    template <typename T>
    inline constexpr bool is_trivially_serializable_v = is_trivially_serializable<T>();

    // =========================================================================
    // Deprecated Aliases (for backward compatibility)
    // =========================================================================
    
    /// @deprecated Use is_trivially_serializable<T>() instead
    template <typename T>
    [[nodiscard]] [[deprecated("Use is_trivially_serializable<T>() instead")]]
    consteval bool is_portable() noexcept {
        return is_trivially_serializable<T>();
    }
    
    /// @deprecated Use is_trivially_serializable_v<T> instead
    template <typename T>
    [[deprecated("Use is_trivially_serializable_v<T> instead")]]
    inline constexpr bool is_portable_v = is_trivially_serializable_v<T>;

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_PORTABILITY_HPP