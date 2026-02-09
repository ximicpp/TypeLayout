// Cross-Platform Compatibility Check (Phase 2)
//
// Compile with any C++17 compiler. P2996 is NOT required.
// Run for a detailed compatibility report.
//
// Zero-Serialization Transfer (ZST) requires TWO conditions:
//   C1: Layout Signature Match  (sig_layout equality)
//   C2: Safety Classification = Safe  (no pointers, bit-fields, or platform-dep types)
//   A1: IEEE 754 (background axiom, holds on all modern hardware)
//
// static_assert verifies C1 only. For full ZST guarantee, also check C2
// via the runtime CompatReporter (Safety column).
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
// Compile-Time Checks (C1 only — layout signature match)
//
// NOTE: static_assert verifies C1 (layout match) but NOT C2 (safety).
// A passing static_assert means the memory layout is identical, but
// zero-serialization also requires Safety = Safe (no pointers/bit-fields).
// Use the runtime CompatReporter for the full C1 ∧ C2 verdict.
// =========================================================================

// Both Linux and macOS are LP64, same long/wchar_t sizes.
// Only long double differs (16B x86 vs 8B ARM).

// --- Safe types: C1 ✓ AND C2 ✓ → Serialization-free ---
// These types use only fixed-width integers, IEEE 754 floats, and byte arrays.
// layout_match + Safety=Safe → zero-copy send/recv is safe.

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

// --- Conditional types: C1 ✓ but C2 ✗ → Layout OK, NOT serialization-free ---
// UnsafeWithPointer: layout matches (both 64-bit, 8B pointers), but
// contains ptr[] fields → Safety=Warning → pointer VALUES are not portable
// across address spaces. Do NOT use for zero-copy network transfer.
static_assert(layout_match(linux_plat::UnsafeWithPointer_layout,
                           macos_plat::UnsafeWithPointer_layout),
    "UnsafeWithPointer: Linux/macOS layout mismatch!");
// ⚠️ C1 passes but C2 fails — this type is NOT serialization-free!

// --- Unsafe types: C1 ✗ → Layout DIFFER → Needs serialization ---
// UnsafeStruct: long double is 16B on Linux x86_64 but 8B on macOS ARM64.
// Uncomment to verify the compile error:
// static_assert(layout_match(linux_plat::UnsafeStruct_layout,
//                            macos_plat::UnsafeStruct_layout),
//     "UnsafeStruct: long double is 16B on Linux x86_64 but 8B on macOS ARM64!");

// =========================================================================
// Runtime Report (C1 ∧ C2 — full ZST verdict)
//
// The CompatReporter checks both C1 (layout match) and C2 (safety
// classification) to produce the precise zero-serialization verdict.
// =========================================================================

TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang, x86_64_windows_msvc)