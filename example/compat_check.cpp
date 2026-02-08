// Cross-Platform Compatibility Check Example (Phase 2)
//
// Demonstrates compile-time and runtime compatibility checking across
// three platforms: x86_64 Linux, ARM64 macOS, and x86_64 Windows.
//
// Phase 2 does NOT require the P2996 compiler — any C++17 compiler works.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

// Include pre-generated signature headers from target platforms
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_macos_clang.sig.hpp"
#include "sigs/x86_64_windows_msvc.sig.hpp"

// Include compatibility check utilities
#include <boost/typelayout/tools/compat_check.hpp>

// Namespace aliases
namespace linux_plat  = boost::typelayout::platform::x86_64_linux_clang;
namespace macos_plat  = boost::typelayout::platform::arm64_macos_clang;
namespace win_plat    = boost::typelayout::platform::x86_64_windows_msvc;

using boost::typelayout::compat::layout_match;
using boost::typelayout::compat::definition_match;

// =========================================================================
// Compile-Time Checks: Linux x86_64 ↔ macOS ARM64
// =========================================================================
// Both are LP64, same long/wchar_t sizes. Only long double differs.

// Safe types - all pass (fixed-width only)
static_assert(layout_match(linux_plat::PacketHeader_layout,
                           macos_plat::PacketHeader_layout),
    "PacketHeader: Linux/macOS layout mismatch!");

static_assert(layout_match(linux_plat::SharedMemRegion_layout,
                           macos_plat::SharedMemRegion_layout),
    "SharedMemRegion: Linux/macOS layout mismatch!");

static_assert(layout_match(linux_plat::FileHeader_layout,
                           macos_plat::FileHeader_layout),
    "FileHeader: Linux/macOS layout mismatch!");

static_assert(layout_match(linux_plat::SensorRecord_layout,
                           macos_plat::SensorRecord_layout),
    "SensorRecord: Linux/macOS layout mismatch!");

static_assert(layout_match(linux_plat::IpcCommand_layout,
                           macos_plat::IpcCommand_layout),
    "IpcCommand: Linux/macOS layout mismatch!");

static_assert(layout_match(linux_plat::MixedSafety_layout,
                           macos_plat::MixedSafety_layout),
    "MixedSafety: Linux/macOS layout mismatch!");

// Pointer types - same on both (8B pointers)
static_assert(layout_match(linux_plat::UnsafeWithPointer_layout,
                           macos_plat::UnsafeWithPointer_layout),
    "UnsafeWithPointer: Linux/macOS layout mismatch!");

// UnsafeStruct - FAILS because long double differs (16B vs 8B)
// Uncomment to verify the compile error:
// static_assert(layout_match(linux_plat::UnsafeStruct_layout,
//                            macos_plat::UnsafeStruct_layout),
//     "UnsafeStruct: long double is 16B on Linux x86_64 but 8B on macOS ARM64!");

// =========================================================================
// Compile-Time Checks: Linux x86_64 ↔ Windows x86_64
// =========================================================================
// Linux LP64 vs Windows LLP64 — long, wchar_t, long double all differ.

static_assert(layout_match(linux_plat::PacketHeader_layout,
                           win_plat::PacketHeader_layout),
    "PacketHeader: Linux/Windows layout mismatch!");

static_assert(layout_match(linux_plat::IpcCommand_layout,
                           win_plat::IpcCommand_layout),
    "IpcCommand: Linux/Windows layout mismatch!");

// =========================================================================
// Compile-Time Checks: All Three Platforms
// =========================================================================
// A type that is safe across ALL three platforms simultaneously.

static_assert(
    layout_match(linux_plat::PacketHeader_layout, macos_plat::PacketHeader_layout) &&
    layout_match(linux_plat::PacketHeader_layout, win_plat::PacketHeader_layout),
    "PacketHeader: not universally portable across Linux/macOS/Windows!");

static_assert(
    layout_match(linux_plat::SensorRecord_layout, macos_plat::SensorRecord_layout) &&
    layout_match(linux_plat::SensorRecord_layout, win_plat::SensorRecord_layout),
    "SensorRecord: not universally portable across Linux/macOS/Windows!");

// =========================================================================
// Runtime Report — All Three Platforms
// =========================================================================

int main() {
    using namespace boost::typelayout::compat;

    CompatReporter reporter;

    // Register all three platforms with full metadata
    reporter.add_platform({
        linux_plat::platform_name,
        reinterpret_cast<const TypeEntry*>(linux_plat::types),
        linux_plat::type_count,
        linux_plat::pointer_size,
        linux_plat::sizeof_long,
        linux_plat::sizeof_wchar_t,
        linux_plat::sizeof_long_double,
        linux_plat::max_align,
        linux_plat::arch_prefix
    });

    reporter.add_platform({
        macos_plat::platform_name,
        reinterpret_cast<const TypeEntry*>(macos_plat::types),
        macos_plat::type_count,
        macos_plat::pointer_size,
        macos_plat::sizeof_long,
        macos_plat::sizeof_wchar_t,
        macos_plat::sizeof_long_double,
        macos_plat::max_align,
        macos_plat::arch_prefix
    });

    reporter.add_platform({
        win_plat::platform_name,
        reinterpret_cast<const TypeEntry*>(win_plat::types),
        win_plat::type_count,
        win_plat::pointer_size,
        win_plat::sizeof_long,
        win_plat::sizeof_wchar_t,
        win_plat::sizeof_long_double,
        win_plat::max_align,
        win_plat::arch_prefix
    });

    // Print detailed three-platform comparison
    reporter.print_report();

    return 0;
}