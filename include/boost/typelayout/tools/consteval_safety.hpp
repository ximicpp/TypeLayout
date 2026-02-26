// Compile-time (consteval) safety classifier for cross-platform type portability.
// Unlike classify_safety() in compat_check.hpp which scans signature strings at
// runtime, this works directly on types at compile time via P2996 reflection.
//
// Requires P2996 (Bloomberg Clang). For C++17-compatible runtime classification,
// use classify_safety(std::string_view) from compat_check.hpp instead.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_CONSTEVAL_SAFETY_HPP
#define BOOST_TYPELAYOUT_TOOLS_CONSTEVAL_SAFETY_HPP

#include <boost/typelayout/detail/reflect.hpp>
#include <boost/typelayout/tools/compat_check.hpp>
#include <type_traits>
#include <cstdint>

namespace boost {
namespace typelayout {
namespace compat {

// =========================================================================
// Platform-dependent integer detection
// =========================================================================
// sizeof(long) differs between LP64 (8) and LLP64 (4).
// long long is always 8 bytes on all supported platforms, but 'long' is
// the only fundamental integer whose size is ABI-dependent.
//
// NOTE: We do NOT reject long long / unsigned long long because they are
// always 8 bytes, and int64_t/uint64_t are often typedefs for them.

template<typename T>
struct is_platform_dependent_integer : std::false_type {};

template<> struct is_platform_dependent_integer<long> : std::true_type {};
template<> struct is_platform_dependent_integer<unsigned long> : std::true_type {};

template<typename T>
inline constexpr bool is_platform_dependent_integer_v =
    is_platform_dependent_integer<T>::value;


// =========================================================================
// Fixed-width integer whitelist
// =========================================================================
// On LP64 (Linux/macOS), uint64_t is a typedef for `unsigned long`.
// On LLP64 (Windows), uint64_t is a typedef for `unsigned long long`.
// Since C++ treats typedefs as the same type, we must whitelist the
// fixed-width typedefs to avoid rejecting `uint64_t` on LP64.
//
// Note: this means that on LP64, a bare `unsigned long` will also pass
// (because it IS `uint64_t`). This is a fundamental C++ limitation.

template<typename T>
consteval bool is_fixed_width_integer() {
    using U = std::remove_cv_t<T>;
    return std::is_same_v<U, int8_t>   || std::is_same_v<U, uint8_t>  ||
           std::is_same_v<U, int16_t>  || std::is_same_v<U, uint16_t> ||
           std::is_same_v<U, int32_t>  || std::is_same_v<U, uint32_t> ||
           std::is_same_v<U, int64_t>  || std::is_same_v<U, uint64_t>;
}


// =========================================================================
// Default safety policy (no extra constraints)
// =========================================================================
// A Policy is a struct with a static consteval method:
//   static consteval SafetyLevel check(SafetyLevel engine_result)
// and optionally:
//   template<typename T>
//   static consteval int type_override()
//     — return -1 (no override) or a valid SafetyLevel cast to int
//
// The default policy passes through the engine result unchanged.

struct DefaultSafetyPolicy {
    /// Called after the engine classifies a leaf type. The policy may
    /// escalate the level but should not downgrade it.
    static consteval SafetyLevel check(SafetyLevel engine_result) {
        return engine_result;
    }

    /// Per-type override hook. Return a negative value to indicate
    /// "no override, let the engine decide."  Return a valid SafetyLevel
    /// (cast to int) to short-circuit the engine for this specific type.
    template<typename T>
    static consteval int type_override() {
        return -1;  // no override
    }
};


// =========================================================================
// consteval_classify_safety<T, Policy>  —  the core engine
// =========================================================================
// Returns the worst (highest enum value) SafetyLevel found in the type tree.
//
// Classification rules:
//   Risk:    pointer, reference, member-pointer, function type,
//            bit-field, platform-dependent integer (long, unsigned long),
//            wchar_t, long double, unfixed enum
//   Warning: polymorphic (vptr), union, virtual base
//   Safe:    fixed-width integers, float, double, bool, char,
//            char8_t/char16_t/char32_t, long long, unsigned long long,
//            fixed enum, bounded arrays of safe types,
//            classes/structs whose ALL bases and members are safe

namespace detail {

    // Bring in helpers from parent namespace boost::typelayout
    using boost::typelayout::get_member_count;
    using boost::typelayout::get_base_count;
    using boost::typelayout::is_fixed_enum;

    /// Return the worse (higher) of two safety levels.
    consteval SafetyLevel worse(SafetyLevel a, SafetyLevel b) {
        return static_cast<int>(a) >= static_cast<int>(b) ? a : b;
    }

    // Forward declaration for mutual recursion
    template<typename T, typename Policy>
    consteval SafetyLevel classify_impl();

    // --- Per-member safety check (template-indexed, P2996 compatible) ---

    template<typename T, typename Policy, std::size_t Index>
    consteval SafetyLevel classify_one_member() {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];

        // Bit-fields are always Risk (layout not portable)
        if constexpr (is_bit_field(member)) {
            return SafetyLevel::Risk;
        } else {
            using FieldType = [:type_of(member):];
            return classify_impl<FieldType, Policy>();
        }
    }

    template<typename T, typename Policy, std::size_t... Is>
    consteval SafetyLevel classify_members_impl(std::index_sequence<Is...>) {
        SafetyLevel result = SafetyLevel::Safe;
        ((result = worse(result, classify_one_member<T, Policy, Is>())), ...);
        return result;
    }

    template<typename T, typename Policy>
    consteval SafetyLevel classify_members() {
        constexpr std::size_t count = get_member_count<T>();
        if constexpr (count == 0) {
            return SafetyLevel::Safe;
        } else {
            return classify_members_impl<T, Policy>(std::make_index_sequence<count>{});
        }
    }

    // --- Per-base safety check (template-indexed, P2996 compatible) ---

    template<typename T, typename Policy, std::size_t Index>
    consteval SafetyLevel classify_one_base() {
        using namespace std::meta;
        constexpr auto base = bases_of(^^T, access_context::unchecked())[Index];
        // Virtual base → Warning (non-portable layout)
        if constexpr (is_virtual(base)) {
            return SafetyLevel::Warning;
        } else {
            using BaseType = [:type_of(base):];
            return classify_impl<BaseType, Policy>();
        }
    }

    template<typename T, typename Policy, std::size_t... Is>
    consteval SafetyLevel classify_bases_impl(std::index_sequence<Is...>) {
        SafetyLevel result = SafetyLevel::Safe;
        ((result = worse(result, classify_one_base<T, Policy, Is>())), ...);
        return result;
    }

    template<typename T, typename Policy>
    consteval SafetyLevel classify_bases() {
        constexpr std::size_t count = get_base_count<T>();
        if constexpr (count == 0) {
            return SafetyLevel::Safe;
        } else {
            return classify_bases_impl<T, Policy>(std::make_index_sequence<count>{});
        }
    }

    // --- Main classification engine ---

    template<typename T, typename Policy>
    consteval SafetyLevel classify_impl() {
        using CleanT = std::remove_cv_t<T>;

        // 0. Policy override: let the policy short-circuit for specific types
        //    (e.g., XOffset containers registered as safe leaves)
        constexpr int override_val = Policy::template type_override<CleanT>();
        if constexpr (override_val >= 0) {
            return static_cast<SafetyLevel>(override_val);
        }

        // 1. Pointer / Reference / Member-pointer / Function → Risk
        else if constexpr (std::is_pointer_v<CleanT> ||
                           std::is_reference_v<CleanT> ||
                           std::is_member_pointer_v<CleanT> ||
                           std::is_function_v<CleanT>) {
            return Policy::check(SafetyLevel::Risk);
        }

        // 1b. Fixed-width integer whitelist → Safe (before platform check)
        //     On LP64, uint64_t == unsigned long, which would otherwise
        //     be caught by the platform-dependent check below.
        else if constexpr (is_fixed_width_integer<CleanT>()) {
            return Policy::check(SafetyLevel::Safe);
        }

        // 2. Platform-dependent integers (long, unsigned long) → Risk
        //    This only triggers for bare `long`/`unsigned long` on LLP64
        //    (Windows), where they are NOT the same type as int64_t/uint64_t.
        //    On LP64, `unsigned long` == `uint64_t` → caught by 1b above.
        else if constexpr (is_platform_dependent_integer_v<CleanT>) {
            return Policy::check(SafetyLevel::Risk);
        }

        // 3. wchar_t → Risk (2 bytes on Windows, 4 bytes on Linux)
        else if constexpr (std::is_same_v<CleanT, wchar_t>) {
            return Policy::check(SafetyLevel::Risk);
        }

        // 4. long double → Risk (80-bit on x86, 64-bit on ARM/MSVC)
        else if constexpr (std::is_same_v<CleanT, long double>) {
            return Policy::check(SafetyLevel::Risk);
        }

        // 5. Enum → check if it has a fixed underlying type
        else if constexpr (std::is_enum_v<CleanT>) {
            return Policy::check(
                is_fixed_enum<CleanT>() ? SafetyLevel::Safe : SafetyLevel::Risk
            );
        }

        // 6. Bounded array → recurse into element type
        else if constexpr (std::is_bounded_array_v<CleanT>) {
            return classify_impl<std::remove_extent_t<CleanT>, Policy>();
        }

        // 7. Union → Warning (members overlap; not safe for cross-platform
        //    serialization-free transfer without explicit layout validation)
        else if constexpr (std::is_union_v<CleanT>) {
            return Policy::check(SafetyLevel::Warning);
        }

        // 8. Class/struct → check polymorphic, then recurse bases + members
        else if constexpr (std::is_class_v<CleanT>) {
            SafetyLevel level = SafetyLevel::Safe;

            // 8a. Polymorphic → Warning (contains vptr)
            if constexpr (std::is_polymorphic_v<CleanT>) {
                level = worse(level, SafetyLevel::Warning);
            }

            // 8b. Check all base classes
            level = worse(level, classify_bases<CleanT, Policy>());

            // 8c. Check all non-static data members
            level = worse(level, classify_members<CleanT, Policy>());

            return Policy::check(level);
        }

        // 9. Fixed-width arithmetic types → Safe
        //    (int8_t through int64_t, uint8_t through uint64_t,
        //     float, double, bool, char, char8_t, char16_t, char32_t,
        //     signed char, unsigned char, short, unsigned short,
        //     int, unsigned int, long long, unsigned long long)
        //    NOTE: 'long' and 'unsigned long' are caught in branch 2.
        else if constexpr (std::is_arithmetic_v<CleanT>) {
            return Policy::check(SafetyLevel::Safe);
        }

        // 10. Everything else → Risk (void, nullptr_t, unknown)
        else {
            return Policy::check(SafetyLevel::Risk);
        }
    }

} // namespace detail


// =========================================================================
// Public API
// =========================================================================

/// Compile-time safety classification of type T.
///
/// @tparam T       The type to classify
/// @tparam Policy  Safety policy (default: DefaultSafetyPolicy)
/// @return SafetyLevel::Safe, Warning, or Risk
///
/// Example:
///   static_assert(consteval_classify_safety<int32_t>() == SafetyLevel::Safe);
///   static_assert(consteval_classify_safety<long>()    == SafetyLevel::Risk);
template<typename T, typename Policy = DefaultSafetyPolicy>
[[nodiscard]] consteval SafetyLevel consteval_classify_safety() {
    return detail::classify_impl<T, Policy>();
}

/// Convenience: is the type safe for zero-copy cross-platform transfer?
template<typename T, typename Policy = DefaultSafetyPolicy>
[[nodiscard]] consteval bool is_consteval_safe() {
    return consteval_classify_safety<T, Policy>() == SafetyLevel::Safe;
}

} // namespace compat
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_CONSTEVAL_SAFETY_HPP