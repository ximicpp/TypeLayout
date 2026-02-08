// Platform detection utilities for cross-platform signature export.
//
// Detects arch, OS, and compiler from predefined macros and produces
// a canonical platform identifier in the form "{arch}_{os}_{compiler}".
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_TOOLS_PLATFORM_DETECT_HPP
#define BOOST_TYPELAYOUT_TOOLS_PLATFORM_DETECT_HPP

#include <string>

namespace boost {
namespace typelayout {
namespace platform {

// =========================================================================
// Architecture detection
// =========================================================================

#if defined(__x86_64__) || defined(_M_X64)
    #define TYPELAYOUT_ARCH_NAME "x86_64"
    #define TYPELAYOUT_ARCH_DISPLAY "x86-64"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define TYPELAYOUT_ARCH_NAME "arm64"
    #define TYPELAYOUT_ARCH_DISPLAY "AArch64"
#elif defined(__i386__) || defined(_M_IX86)
    #define TYPELAYOUT_ARCH_NAME "x86"
    #define TYPELAYOUT_ARCH_DISPLAY "x86"
#elif defined(__arm__) || defined(_M_ARM)
    #define TYPELAYOUT_ARCH_NAME "arm"
    #define TYPELAYOUT_ARCH_DISPLAY "ARM"
#elif defined(__riscv) && (__riscv_xlen == 64)
    #define TYPELAYOUT_ARCH_NAME "riscv64"
    #define TYPELAYOUT_ARCH_DISPLAY "RISC-V 64"
#elif defined(__riscv) && (__riscv_xlen == 32)
    #define TYPELAYOUT_ARCH_NAME "riscv32"
    #define TYPELAYOUT_ARCH_DISPLAY "RISC-V 32"
#elif defined(__powerpc64__)
    #define TYPELAYOUT_ARCH_NAME "ppc64"
    #define TYPELAYOUT_ARCH_DISPLAY "PowerPC 64"
#elif defined(__s390x__)
    #define TYPELAYOUT_ARCH_NAME "s390x"
    #define TYPELAYOUT_ARCH_DISPLAY "s390x"
#else
    #define TYPELAYOUT_ARCH_NAME "unknown_arch"
    #define TYPELAYOUT_ARCH_DISPLAY "Unknown Arch"
#endif

// =========================================================================
// OS detection
// =========================================================================

#if defined(__linux__)
    #define TYPELAYOUT_OS_NAME "linux"
    #define TYPELAYOUT_OS_DISPLAY "Linux"
#elif defined(_WIN32)
    #define TYPELAYOUT_OS_NAME "windows"
    #define TYPELAYOUT_OS_DISPLAY "Windows"
#elif defined(__APPLE__) && defined(__MACH__)
    #define TYPELAYOUT_OS_NAME "macos"
    #define TYPELAYOUT_OS_DISPLAY "macOS"
#elif defined(__FreeBSD__)
    #define TYPELAYOUT_OS_NAME "freebsd"
    #define TYPELAYOUT_OS_DISPLAY "FreeBSD"
#elif defined(__ANDROID__)
    #define TYPELAYOUT_OS_NAME "android"
    #define TYPELAYOUT_OS_DISPLAY "Android"
#else
    #define TYPELAYOUT_OS_NAME "unknown_os"
    #define TYPELAYOUT_OS_DISPLAY "Unknown OS"
#endif

// =========================================================================
// Compiler detection
// =========================================================================

// Note: __clang__ must be checked before __GNUC__ because Clang also
// defines __GNUC__.

#if defined(__clang__)
    #define TYPELAYOUT_COMPILER_NAME "clang"
    #define TYPELAYOUT_COMPILER_DISPLAY "Clang"
#elif defined(__GNUC__)
    #define TYPELAYOUT_COMPILER_NAME "gcc"
    #define TYPELAYOUT_COMPILER_DISPLAY "GCC"
#elif defined(_MSC_VER)
    #define TYPELAYOUT_COMPILER_NAME "msvc"
    #define TYPELAYOUT_COMPILER_DISPLAY "MSVC"
#else
    #define TYPELAYOUT_COMPILER_NAME "unknown_compiler"
    #define TYPELAYOUT_COMPILER_DISPLAY "Unknown Compiler"
#endif

// =========================================================================
// Combined platform identifiers
// =========================================================================

/// Canonical platform name: "{arch}_{os}_{compiler}" (valid C++ identifier).
/// Example: "x86_64_linux_clang"
inline std::string get_platform_name() {
    return TYPELAYOUT_ARCH_NAME "_" TYPELAYOUT_OS_NAME "_" TYPELAYOUT_COMPILER_NAME;
}

/// Human-readable platform description.
/// Example: "x86-64 Linux (Clang)"
inline std::string get_platform_display_name() {
    return TYPELAYOUT_ARCH_DISPLAY " " TYPELAYOUT_OS_DISPLAY " (" TYPELAYOUT_COMPILER_DISPLAY ")";
}

} // namespace platform
} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_TOOLS_PLATFORM_DETECT_HPP
