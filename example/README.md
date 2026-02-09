# Cross-Platform Compatibility Check — Pure C++

## What It Does

Determines whether your C++ types can be shared **directly** across different
platforms (via shared memory, mmap, network sockets, file I/O) **without any
serialization** — using Boost.TypeLayout's compile-time signature system.

**100% pure C++** — no Python, no external tools, no runtime dependencies.

## How It Works — Two-Phase Pipeline

```
Phase 1: Export (per-platform, requires P2996)
┌─────────────────────────┐
│ x86_64 Linux (Clang)    │
│ compile + run sig_export │──► x86_64_linux_clang.sig.hpp
└─────────────────────────┘
┌─────────────────────────┐
│ x86_64 Windows (MSVC)   │
│ compile + run sig_export │──► x86_64_windows_msvc.sig.hpp
└─────────────────────────┘
         │
         ▼
Phase 2: Check (any platform, C++17 only — no P2996 needed)
┌───────────────────────────────────────┐
│ #include all .sig.hpp headers         │
│                                       │
│ TYPELAYOUT_CHECK_COMPAT(              │
│   x86_64_linux_clang,                 │
│   x86_64_windows_msvc)                │
│                                       │
│ // Compiles + runs = report ✅        │
│ // Or use TYPELAYOUT_ASSERT_COMPAT    │
│ // for compile-time proof ✅          │
└───────────────────────────────────────┘
```

## Quick Start

### Step 1: Write the Export Source (Phase 1)

```cpp
// cross_platform_check.cpp — 3 lines of user code
#include <boost/typelayout/tools/sig_export.hpp>
#include "my_types.hpp"    // your struct definitions

TYPELAYOUT_EXPORT_TYPES(PacketHeader, SensorRecord, SharedMemRegion)
```

The `TYPELAYOUT_EXPORT_TYPES(...)` macro generates a complete `main()` function.
No boilerplate needed.

### Step 2: Compile & Run on Each Platform

```bash
# Build the signature exporter on each target platform
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -I./include -o sig_export example/cross_platform_check.cpp

# Run it to generate a .sig.hpp for this platform
./sig_export sigs/
# → creates sigs/x86_64_linux_clang.sig.hpp
```

Using Docker:
```bash
docker run --rm -v $(pwd):/workspace -w /workspace \
    -e LD_LIBRARY_PATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu \
    ghcr.io/ximicpp/typelayout-p2996:latest \
    bash -c 'clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
        -I./include -o sig_export example/cross_platform_check.cpp && \
        ./sig_export sigs/'
```

### Step 3: Write the Check Source (Phase 2)

```cpp
// compat_check.cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_macos_clang.sig.hpp"
#include "sigs/x86_64_windows_msvc.sig.hpp"
#include <boost/typelayout/tools/compat_auto.hpp>

// Option A: Runtime report
TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang, x86_64_windows_msvc)
```

Or for compile-time verification:

```cpp
// compat_assert.cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_linux_clang.sig.hpp"
#include <boost/typelayout/tools/compat_auto.hpp>

// Option B: Compile-time — fails if any type differs
TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_clang, arm64_linux_clang)
```

### Step 4: Compile & Run the Check

```bash
# Compile the checker — any C++17 compiler works!
# P2996 is NOT required for this step.
clang++ -std=c++17 -stdlib=libc++ -I./include -I./example \
    -o compat_check example/compat_check.cpp

# Run for a detailed report
./compat_check
```

### Example Output

```
========================================================================
  Boost.TypeLayout — Cross-Platform Compatibility Report
========================================================================

Platforms compared: 3
  * x86_64_linux_clang [64-le]
    pointer=8B, long=8B, wchar_t=4B, long_double=16B, max_align=16B
  * arm64_macos_clang [64-le]
    pointer=8B, long=8B, wchar_t=4B, long_double=8B, max_align=16B
  * x86_64_windows_msvc [64-le]
    pointer=8B, long=4B, wchar_t=2B, long_double=8B, max_align=16B

Assumptions: IEEE 754 floats, same endianness across platforms.
Safety: *** = safe for zero-copy, **- = layout ok but has
        pointers/vptr, *-- = bit-fields or platform-dependent types.

--------------------------------------------------------------------------------
  Type                      Layout  Definition  Safety  Verdict
--------------------------------------------------------------------------------
  PacketHeader               MATCH       MATCH    ***  Serialization-free
  SharedMemRegion             MATCH       MATCH    ***  Serialization-free
  FileHeader                  MATCH       MATCH    ***  Serialization-free
  SensorRecord                MATCH       MATCH    ***  Serialization-free
  IpcCommand                  MATCH       MATCH    ***  Serialization-free
  UnsafeStruct               DIFFER      DIFFER    *--  Needs serialization
  UnsafeWithPointer           MATCH       MATCH    **-  Layout OK (pointer values not portable)
  MixedSafety                 MATCH       MATCH    ***  Serialization-free
--------------------------------------------------------------------------------

  [DIFFER] UnsafeStruct layout signatures:
    x86_64_linux_clang: [64-le]record[s:48,a:16]{@0:i64[s:8,a:8],...}
    x86_64_windows_msvc: [64-le]record[s:32,a:8]{@0:i32[s:4,a:4],...}

  Safety warnings:
  [**-] UnsafeWithPointer — contains pointers or vptr

========================================================================
  Serialization-free (C1+C2): 6/8 (75%)
  Layout-compatible (C1):     7/8 (layout matches but has pointers/bit-fields)
  Needs serialization:        1/8
========================================================================
```

**Reading the report:**
- **Serialization-free** (C1 ∧ C2): Safe for zero-copy `send()/recv()` — no serialization needed
- **Layout OK** (C1 only): Memory layout matches, but has pointers or bit-fields — use with care
- **Needs serialization** (¬C1): Layout differs across platforms — must serialize

## Adding Your Own Types

Create your own export source — just list the types:

```cpp
// my_export.cpp
#include <boost/typelayout/tools/sig_export.hpp>
#include "my_protocol.hpp"

struct MyProtocolMessage {
    uint32_t msg_id;
    uint64_t timestamp;
    float    data[8];
};

TYPELAYOUT_EXPORT_TYPES(MyProtocolMessage)
```

Types can be defined inline in the file or included from headers.

## Fine-Grained Compile-Time Checks

You can also mix macros with manual `static_assert` for fine-grained control:

```cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_macos_clang.sig.hpp"
#include <boost/typelayout/tools/compat_auto.hpp>

namespace linux_plat = boost::typelayout::platform::x86_64_linux_clang;
namespace macos_plat = boost::typelayout::platform::arm64_macos_clang;

using boost::typelayout::compat::layout_match;

// Check individual types
static_assert(layout_match(linux_plat::PacketHeader_layout,
                           macos_plat::PacketHeader_layout),
    "PacketHeader: Linux/macOS layout mismatch!");

// Use the macro for the runtime report
TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang)
```

## CMake Integration

```cmake
include(cmake/TypeLayoutCompat.cmake)

# One-liner: creates both export and check targets
typelayout_add_compat_pipeline(
    NAME          my_compat
    EXPORT_SOURCE example/cross_platform_check.cpp
    CHECK_SOURCE  example/compat_check.cpp
    SIGS_DIR      ${CMAKE_SOURCE_DIR}/example/sigs
    ADD_TEST
)

# Or fine-grained:
typelayout_add_sig_export(
    TARGET sig_export
    SOURCE example/cross_platform_check.cpp
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/sigs
)

typelayout_add_compat_check(
    TARGET compat_check
    SOURCE example/compat_check.cpp
    SIGS_DIR ${CMAKE_SOURCE_DIR}/example/sigs
)
```

## Automated Toolchain

For production use, the `typelayout-compat` CLI automates the entire pipeline:

```bash
# One-command cross-platform check (Docker-based)
./tools/typelayout-compat check \
    --export-source example/cross_platform_check.cpp \
    --check-source example/compat_check.cpp \
    --platforms x86_64-linux-clang,arm64-linux-clang
```

See [tools/README.md](../tools/README.md) for full toolchain documentation,
including CI/CD integration with GitHub Actions.

## Platform Naming Convention

Platform names follow the format `{arch}_{os}_{compiler}`:

| Platform | Name |
|----------|------|
| x86-64 Linux, Clang | `x86_64_linux_clang` |
| x86-64 Linux, GCC | `x86_64_linux_gcc` |
| AArch64 Linux, Clang | `arm64_linux_clang` |
| x86-64 Windows, MSVC | `x86_64_windows_msvc` |
| AArch64 macOS, Clang | `arm64_macos_clang` |

The platform is auto-detected from compiler macros. You can override it:

```cpp
boost::typelayout::SigExporter ex("my_custom_platform");
```

## Why This Matters

Traditional approaches to cross-platform data sharing require:
- Hand-written serialization code
- Schema languages (protobuf, FlatBuffers, Cap'n Proto)
- Runtime checks and validation

With TypeLayout's two-phase pipeline, you get:
- **Compile-time proof** that types are layout-compatible (C1: layout match)
- **Safety classification** that identifies pointer/bit-field risks (C2: safety check)
- **Zero runtime overhead** — no serialization needed for types satisfying C1 ∧ C2
- **Pure C++** — no external tools or languages
- **Detailed diagnostics** — know exactly which types differ and why

### Zero-Serialization Transfer (ZST) Quick Reference

| Condition | What It Checks | How to Verify |
|-----------|---------------|---------------|
| **C1** | Layout signature match | `static_assert(layout_match(...))` or CompatReporter |
| **C2** | Safety = Safe (no ptr/bit-fields) | CompatReporter Safety column: `***` |
| **A1** | IEEE 754 floats (axiom) | All modern hardware satisfies this |

**C1 ∧ C2 → zero-copy safe.** Otherwise → use serialization.

## File Reference

| File | Description |
|------|-------------|
| `cross_platform_check.cpp` | Phase 1: Export types using `TYPELAYOUT_EXPORT_TYPES(...)` |
| `compat_check.cpp` | Phase 2: Check/report using `TYPELAYOUT_CHECK_COMPAT(...)` |
| `sigs/*.sig.hpp` | Pre-generated signature files for 3 platforms |