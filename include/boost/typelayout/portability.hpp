// Boost.TypeLayout
//
// Portability Checking
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

    // Forward declarations
    template <typename T>
    [[nodiscard]] consteval bool is_portable() noexcept;
    
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
    /// Bit-fields have implementation-defined layout and are NOT portable
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
    // Portability Checking
    // =========================================================================

    // Check if a single member type is portable (recursive)
    template<typename T, std::size_t Index>
    consteval bool check_member_portable() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];
        
        // Use is_portable for recursive checking of nested structs
        return is_portable<FieldType>();
    }

    // Check all members for portability
    template<typename T, std::size_t... Indices>
    consteval bool check_all_members_portable(std::index_sequence<Indices...>) noexcept {
        return (check_member_portable<T, Indices>() && ...);
    }

    // Check if a single base class is portable (recursive)
    template<typename T, std::size_t Index>
    consteval bool check_base_portable() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[Index];
        using BaseType = [:type_of(base_info):];
        
        // Recursively check the base class
        return is_portable<BaseType>();
    }

    // Check all base classes for portability
    template<typename T, std::size_t... Indices>
    consteval bool check_all_bases_portable(std::index_sequence<Indices...>) noexcept {
        return (check_base_portable<T, Indices>() && ...);
    }

    /// Check if a type is portable (no platform-dependent members, no bit-fields,
    /// no pointers, no references)
    /// Recursively checks nested structs AND base classes
    /// Bit-fields are NOT portable: layout is implementation-defined (C++11 §9.6)
    /// Pointers are NOT portable: different address spaces across processes
    /// References are NOT portable: cannot be serialized/transmitted
    template <typename T>
    [[nodiscard]] consteval bool is_portable() noexcept {
        // References are NEVER portable (check before removing cv/ref)
        if constexpr (std::is_reference_v<T>) {
            return false;
        }
        
        // Strip CV qualifiers
        using CleanT = std::remove_cv_t<T>;
        
        // Pointers are NEVER portable (different address spaces)
        if constexpr (std::is_pointer_v<CleanT>) {
            return false;
        }
        // Member pointers are also not portable
        else if constexpr (std::is_member_pointer_v<CleanT>) {
            return false;
        }
        // Null pointer type is not portable
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
            return is_portable<ElementType>();
        }
        // Unions: we cannot know which member is active at runtime, but
        // we CAN check all members for portability at compile time.
        // Conservative strategy: if ANY member is non-portable, the union
        // is considered non-portable (safer than missing a problem).
        else if constexpr (std::is_union_v<CleanT>) {
            // Bit-fields in unions are not portable
            if (has_bitfields<CleanT>()) {
                return false;
            }
            constexpr std::size_t member_count = get_member_count<CleanT>();
            if constexpr (member_count == 0) {
                return true;
            } else {
                return check_all_members_portable<CleanT>(
                    std::make_index_sequence<member_count>{});
            }
        }
        // Check structs/classes recursively (including base classes)
        else if constexpr (std::is_class_v<CleanT>) {
            // Bit-fields have implementation-defined layout - NOT portable
            if (has_bitfields<CleanT>()) {
                return false;
            }
            
            // First check all base classes
            constexpr std::size_t base_count = get_base_count<CleanT>();
            bool bases_portable = true;
            if constexpr (base_count > 0) {
                bases_portable = check_all_bases_portable<CleanT>(
                    std::make_index_sequence<base_count>{});
            }
            
            if (!bases_portable) {
                return false;
            }
            
            // Then check all direct members
            constexpr std::size_t member_count = get_member_count<CleanT>();
            if constexpr (member_count == 0) {
                return true;
            } else {
                return check_all_members_portable<CleanT>(
                    std::make_index_sequence<member_count>{});
            }
        }
        // All other types (primitives, pointers, etc.) are portable
        else {
            return true;
        }
    }

    /// Helper variable template for is_portable
    template <typename T>
    inline constexpr bool is_portable_v = is_portable<T>();

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_PORTABILITY_HPP
