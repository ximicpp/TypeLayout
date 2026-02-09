# Cross-Platform Compatibility Check

Checks whether C++ types can be shared as raw bytes across platforms (shared
memory, network, files) without serialization, using TypeLayout's compile-time
signature system.

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

`TYPELAYOUT_EXPORT_TYPES(...)` generates `main()` for you.

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
clang++ -std=c++17 -stdlib=libc++ -I./include -I./example \
    -o compat_check example/compat_check.cpp
./compat_check
```

### Example Output

```
========================================================================
  Cross-Platform Compatibility Report
========================================================================

Platforms compared: 3
  * x86_64_linux_clang [64-le]
    pointer=8B, long=8B, wchar_t=4B, long_double=16B, max_align=16B
  * arm64_macos_clang [64-le]
    pointer=8B, long=8B, wchar_t=4B, long_double=8B, max_align=16B
  * x86_64_windows_msvc [64-le]
    pointer=8B, long=4B, wchar_t=2B, long_double=8B, max_align=16B

Safety: *** = zero-copy ok, **- = has pointers/vptr, *-- = bit-fields.

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

**Verdicts:**
- **Serialization-free**: Layout matches, no pointers/bit-fields — zero-copy safe
- **Layout OK**: Layout matches but has pointers or bit-fields — not safe for raw transfer
- **Needs serialization**: Layout differs — must use a serialization format

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

## Fine-Grained Compile-Time Checks

Mix macros with manual `static_assert`:

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

## File Reference

| File | Description |
|------|-------------|
| `cross_platform_check.cpp` | Phase 1: Export types using `TYPELAYOUT_EXPORT_TYPES(...)` |
| `compat_check.cpp` | Phase 2: Check/report using `TYPELAYOUT_CHECK_COMPAT(...)` |
| `sigs/*.sig.hpp` | Pre-generated signature files for 3 platforms |