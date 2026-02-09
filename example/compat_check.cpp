// Cross-Platform Compatibility Check (Phase 2)
//
// Compile with any C++17 compiler (P2996 NOT required).
// ZST requires: C1 (layout match) ∧ C2 (Safety=Safe) ∧ A1 (IEEE 754).
// static_assert checks C1 only; use CompatReporter for full C1∧C2 verdict.
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

// ---- Compile-Time Checks (C1 only) ----
// Both Linux and macOS are LP64; only long double differs (16B x86 vs 8B ARM).

// C1 ✓ ∧ C2 ✓ → Serialization-free
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

// C1 ✓ ∧ C2 ✗ → Layout OK but NOT serialization-free (contains pointers)
static_assert(layout_match(linux_plat::UnsafeWithPointer_layout,
                           macos_plat::UnsafeWithPointer_layout),
    "UnsafeWithPointer: Linux/macOS layout mismatch!");

// C1 ✗ → Needs serialization (long double: 16B on Linux x86_64, 8B on macOS ARM64)
// static_assert(layout_match(linux_plat::UnsafeStruct_layout,
//                            macos_plat::UnsafeStruct_layout),
//     "UnsafeStruct: long double size differs!");

// ---- Runtime Report (full C1 ∧ C2 verdict) ----

TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang, x86_64_windows_msvc)