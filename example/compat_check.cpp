// Cross-platform compatibility check example.
// Compile with any C++17 compiler. P2996 is NOT required.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_macos_clang.sig.hpp"
#include "sigs/x86_64_windows_msvc.sig.hpp"

#include <boost/typelayout/tools/compat_auto.hpp>

namespace linux_plat  = boost::typelayout::platform::x86_64_linux_clang;
namespace macos_plat  = boost::typelayout::platform::arm64_macos_clang;
namespace win_plat    = boost::typelayout::platform::x86_64_windows_msvc;

using boost::typelayout::compat::layout_match;

// Compile-time layout checks (Linux vs macOS, both LP64).

// Safe types — layout matches and no pointers/bit-fields.
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

// Layout matches but contains pointers — not safe for zero-copy transfer.
static_assert(layout_match(linux_plat::UnsafeWithPointer_layout,
                           macos_plat::UnsafeWithPointer_layout),
    "UnsafeWithPointer: Linux/macOS layout mismatch!");

// Layout DIFFERS (long double: 16B on x86_64, 8B on ARM64) — needs serialization.
// static_assert(layout_match(linux_plat::UnsafeStruct_layout,
//                            macos_plat::UnsafeStruct_layout),
//     "UnsafeStruct: long double size differs!");

// Runtime report across all three platforms.
TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang, x86_64_windows_msvc)