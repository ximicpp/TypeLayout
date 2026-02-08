// Cross-Platform Compatibility Check (Phase 2)
//
// Compile with any C++17 compiler. P2996 is NOT required.
// Run for a detailed compatibility report.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_macos_clang.sig.hpp"
#include "sigs/x86_64_windows_msvc.sig.hpp"

// Include compatibility check utilities
#include <boost/typelayout/tools/compat_auto.hpp>

// Namespace aliases
namespace linux_plat  = boost::typelayout::platform::x86_64_linux_clang;
namespace macos_plat  = boost::typelayout::platform::arm64_macos_clang;
namespace win_plat    = boost::typelayout::platform::x86_64_windows_msvc;

using boost::typelayout::compat::layout_match;

// =========================================================================
// Compile-Time Checks
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
// Runtime Report
// =========================================================================

TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang, x86_64_windows_msvc)