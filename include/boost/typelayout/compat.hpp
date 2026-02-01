// Boost.TypeLayout
//
// Cross-Platform Compatibility Check Tool - User Configuration Interface
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TYPELAYOUT_COMPAT_HPP
#define BOOST_TYPELAYOUT_COMPAT_HPP

#include <boost/typelayout.hpp>
#include <iostream>
#include <array>
#include <string_view>

namespace boost {
namespace typelayout {

// =========================================================================
// Type List Utility
// =========================================================================

/// A compile-time list of types
template<typename... Ts>
struct type_list {
    static constexpr std::size_t size = sizeof...(Ts);
};

// =========================================================================
// Platform Enumeration
// =========================================================================

/// Supported target platforms
enum class Platform {
    linux_x64,      // Linux x86_64 (LP64)
    linux_arm64,    // Linux AArch64 (LP64)
    windows_x64,    // Windows x86_64 (LLP64)
    macos_x64,      // macOS x86_64 (LP64)
    macos_arm64     // macOS ARM64 (LP64)
};

/// Get platform name as string
constexpr std::string_view platform_name(Platform p) {
    switch (p) {
        case Platform::linux_x64:   return "linux-x64";
        case Platform::linux_arm64: return "linux-arm64";
        case Platform::windows_x64: return "windows-x64";
        case Platform::macos_x64:   return "macos-x64";
        case Platform::macos_arm64: return "macos-arm64";
        default: return "unknown";
    }
}

/// Detect current platform at compile time
consteval Platform current_platform() {
#if defined(__linux__)
    #if defined(__x86_64__)
        return Platform::linux_x64;
    #elif defined(__aarch64__)
        return Platform::linux_arm64;
    #else
        return Platform::linux_x64;  // fallback
    #endif
#elif defined(_WIN32) || defined(_WIN64)
    return Platform::windows_x64;
#elif defined(__APPLE__)
    #if defined(__arm64__) || defined(__aarch64__)
        return Platform::macos_arm64;
    #else
        return Platform::macos_x64;
    #endif
#else
    return Platform::linux_x64;  // fallback
#endif
}

// =========================================================================
// Signature Emission
// =========================================================================

namespace detail {

/// Emit signature for a single type
template<typename T>
void emit_type_signature(std::ostream& os) {
    // Use reflection to get type name
    constexpr auto type_info = ^^T;
    constexpr auto name = identifier_of(type_info);
    
    os << name << " "
       << get_layout_hash<T>() << " "
       << sizeof(T) << " "
       << alignof(T) << "\n";
}

/// Emit signatures for all types in a type_list
template<typename... Ts>
void emit_all_signatures(type_list<Ts...>, std::ostream& os) {
    (emit_type_signature<Ts>(os), ...);
}

} // namespace detail

/// Generate signatures for all registered types
/// Output format:
///   __PLATFORM__ linux-x64
///   __ARCH__ [64-le]
///   TypeName Hash Size Align
///   ...
template<typename TypeList>
void emit_signatures(std::ostream& os) {
    // Emit platform identifier
    os << "__PLATFORM__ " << platform_name(current_platform()) << "\n";
    
    // Emit architecture prefix
    os << "__ARCH__ " << get_arch_prefix() << "\n";
    
    // Emit each type's signature
    detail::emit_all_signatures(TypeList{}, os);
}

} // namespace typelayout
} // namespace boost

// =========================================================================
// User Configuration Macros
// =========================================================================

/// Register types for compatibility checking
/// Usage: TYPELAYOUT_TYPES(MyStruct1, MyStruct2, MyStruct3)
#define TYPELAYOUT_TYPES(...) \
    namespace typelayout_user_config { \
        using RegisteredTypes = ::boost::typelayout::type_list<__VA_ARGS__>; \
    }

/// Specify target platforms (optional)
/// Usage: TYPELAYOUT_PLATFORMS(linux_x64, windows_x64)
/// If not specified, defaults to linux_x64 and windows_x64
#define TYPELAYOUT_PLATFORMS(...) \
    namespace typelayout_user_config { \
        inline constexpr auto target_platforms = std::array{ \
            ::boost::typelayout::Platform::__VA_ARGS__ \
        }; \
        inline constexpr bool platforms_specified = true; \
    }

/// Generate a main() function that emits signatures
/// Usage: TYPELAYOUT_SIGGEN_MAIN()
#define TYPELAYOUT_SIGGEN_MAIN() \
    int main() { \
        ::boost::typelayout::emit_signatures< \
            ::typelayout_user_config::RegisteredTypes \
        >(std::cout); \
        return 0; \
    }

#endif // BOOST_TYPELAYOUT_COMPAT_HPP
