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
│ #include both .sig.hpp headers        │
│                                       │
│ static_assert(layout_match(           │
│   linux::PacketHeader_layout,         │
│   windows::PacketHeader_layout));     │
│                                       │
│ // Compiles = binary-compatible ✅    │
│ // Fails = layout mismatch ❌        │
└───────────────────────────────────────┘
```

## Quick Start

### Step 1: Export Signatures (Phase 1)

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

### Step 2: Compatibility Check (Phase 2)

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

Platforms compared: 2
  * x86_64_linux_clang [64-le]
    pointer=8B, long=8B, wchar_t=4B, long_double=16B, max_align=16B
  * x86_64_windows_msvc [64-le]
    pointer=8B, long=4B, wchar_t=2B, long_double=8B, max_align=16B

------------------------------------------------------------------------
  Type                            Layout   Definition  Verdict
------------------------------------------------------------------------
  PacketHeader                     MATCH        MATCH  Serialization-free
  SharedMemRegion                  MATCH        MATCH  Serialization-free
  FileHeader                       MATCH        MATCH  Serialization-free
  SensorRecord                     MATCH        MATCH  Serialization-free
  IpcCommand                       MATCH        MATCH  Serialization-free
  UnsafeStruct                    DIFFER       DIFFER  Needs serialization
  UnsafeWithPointer                MATCH        MATCH  Serialization-free
  MixedSafety                      MATCH        MATCH  Serialization-free
------------------------------------------------------------------------

  [DIFFER] UnsafeStruct layout signatures:
    x86_64_linux_clang: [64-le]record[s:48,a:16]{@0:i64[s:8,a:8],...}
    x86_64_windows_msvc: [64-le]record[s:32,a:8]{@0:i32[s:4,a:4],...}

========================================================================
  87% of types (7/8) are serialization-free across all platforms.
  1 type(s) need serialization for cross-platform use.
========================================================================
```

## How It Works

### Phase 1: Signature Export

The `SigExporter` uses TypeLayout's `consteval` signature engine to extract
real signatures at compile time, then writes them to a `.sig.hpp` header file:

```cpp
#include <boost/typelayout/tools/sig_export.hpp>

int main() {
    boost::typelayout::SigExporter ex;  // auto-detects platform name
    ex.add<MyType>("MyType");
    ex.write("sigs/");
}
```

The generated `.sig.hpp` contains `constexpr const char[]` variables:

```cpp
namespace boost::typelayout::platform::x86_64_linux_clang {
    inline constexpr const char MyType_layout[] = "[64-le]record[s:8,a:4]{...}";
    inline constexpr const char MyType_definition[] = "[64-le]record[s:8,a:4]{...}";
}
```

### Phase 2: Compile-Time Check

Include `.sig.hpp` files from all target platforms and use `static_assert`:

```cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_linux_clang.sig.hpp"
#include <boost/typelayout/tools/compat_check.hpp>

namespace pa = boost::typelayout::platform::x86_64_linux_clang;
namespace pb = boost::typelayout::platform::arm64_linux_clang;

// Compilation succeeds only if types are binary-compatible
static_assert(boost::typelayout::compat::layout_match(
    pa::MyType_layout, pb::MyType_layout),
    "MyType is NOT compatible across these platforms!");
```

**Key insight**: Phase 2 does NOT require P2996 — the `.sig.hpp` files are
plain C++17 `constexpr` data. You can run this check with GCC, MSVC, or
any standard compiler.

## Adding Your Own Types

Edit `cross_platform_check.cpp` and register your types:

```cpp
struct MyProtocolMessage {
    uint32_t msg_id;
    uint64_t timestamp;
    float    data[8];
};

// In main():
ex.add<MyProtocolMessage>("MyProtocolMessage");
```

## CMake Integration

```cmake
include(cmake/TypeLayoutCompat.cmake)

# Phase 1: export signatures
typelayout_add_sig_export(
    TARGET sig_export
    SOURCE export_sigs.cpp
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/sigs
)

# Phase 2: compatibility check
typelayout_add_compat_check(
    TARGET compat_check
    SOURCE check_compat.cpp
    SIGS_DIR ${CMAKE_SOURCE_DIR}/sigs
)
```

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
SigExporter ex("my_custom_platform");
```

## Why This Matters

Traditional approaches to cross-platform data sharing require:
- Hand-written serialization code
- Schema languages (protobuf, FlatBuffers, Cap'n Proto)
- Runtime checks and validation

With TypeLayout's two-phase pipeline, you get:
- **Compile-time proof** that types are binary-compatible
- **Zero runtime overhead** — no serialization needed for compatible types
- **Pure C++** — no external tools or languages
- **Detailed diagnostics** — know exactly which types differ and why