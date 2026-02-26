# TypeLayout Quickstart

Get started with TypeLayout in under 5 minutes.

## Prerequisites

- **Compiler**: Bloomberg Clang P2996 fork (C++26 with static reflection)
- **Standard**: C++26 (`-std=c++26 -freflection`)
- **Dependencies**: None (header-only library)

## 1. Minimal Example

Create a file `check.cpp`:

```cpp
#include <boost/typelayout/typelayout.hpp>
#include <iostream>
#include <cstdint>

using namespace boost::typelayout;

// Two structs in different subsystems -- are they compatible?
struct SenderMsg   { uint32_t id; uint64_t timestamp; double value; };
struct ReceiverMsg { uint32_t id; uint64_t timestamp; double value; };

// Compile-time verification: both layers
static_assert(layout_signatures_match<SenderMsg, ReceiverMsg>(),
    "Binary layout mismatch -- unsafe to memcpy");
static_assert(definition_signatures_match<SenderMsg, ReceiverMsg>(),
    "Structural mismatch -- field names or hierarchy differ");

int main() {
    // Print the signatures for inspection
    constexpr auto layout = get_layout_signature<SenderMsg>();
    constexpr auto defn   = get_definition_signature<SenderMsg>();

    std::cout << "Layout:     " << layout     << "\n";
    std::cout << "Definition: " << defn        << "\n";
    std::cout << "Match: YES\n";
    return 0;
}
```

## 2. Build and Run

```bash
cmake -B build
cmake --build build

# Or compile directly:
clang++ -std=c++26 -freflection -I include check.cpp -o check
./check
```

Expected output (on x86-64, little-endian):

```
Layout:     [64-le]record[s:24,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8],@16:f64[s:8,a:8]}
Definition: [64-le]record[s:24,a:8]{@0[id]:u32[s:4,a:4],@8[timestamp]:u64[s:8,a:8],@16[value]:f64[s:8,a:8]}
Match: YES
```

## 3. Catching Mismatches

If a field is renamed or reordered, the compiler catches it immediately:

```cpp
struct SenderMsg   { uint32_t id; uint64_t timestamp; double value; };
struct ReceiverMsg { uint32_t id; double value; uint64_t timestamp; };  // reordered!

// This static_assert FAILS at compile time:
static_assert(layout_signatures_match<SenderMsg, ReceiverMsg>());
//            ^^^ error: layout mismatch -- offsets differ
```

## 4. Reading a Signature

```
[64-le]record[s:24,a:8]{@0[id]:u32[s:4,a:4],@8[timestamp]:u64[s:8,a:8],@16[value]:f64[s:8,a:8]}
 ^      ^      ^    ^    ^  ^    ^    ^   ^
 |      |      |    |    |  |    |    |   alignment
 |      |      |    |    |  |    |    size
 |      |      |    |    |  |    type name
 |      |      |    |    |  field name (Definition only)
 |      |      |    |    byte offset
 |      |      |    struct alignment
 |      |      struct size
 |      aggregate type (record / union / enum)
 platform prefix: pointer width + endianness
```

## 5. Which Function to Use?

| Question | Function |
|----------|----------|
| "Can I safely `memcpy` between these types?" | `layout_signatures_match<T, U>()` |
| "Are these types structurally identical?" | `definition_signatures_match<T, U>()` |
| "I am not sure which one I need" | `definition_signatures_match` (safer default) |

`definition_match` implies `layout_match` (Projection property), so
Definition is strictly safer -- it catches everything Layout catches,
plus field renames and inheritance changes.

## 6. Cross-Platform Verification

For comparing types across different platforms (e.g., ARM64 vs x86-64),
TypeLayout uses a **two-phase pipeline**:

```
Phase 1: Export (per-platform, requires P2996)
+--------------------------+
| x86_64 Linux (Clang)    |
| compile + run sig_export |---> x86_64_linux_clang.sig.hpp
+--------------------------+
+--------------------------+
| x86_64 Windows (MSVC)   |
| compile + run sig_export |---> x86_64_windows_msvc.sig.hpp
+--------------------------+
         |
         v
Phase 2: Check (any platform, C++17 only -- no P2996 needed)
+---------------------------------------+
| #include all .sig.hpp headers         |
|                                       |
| TYPELAYOUT_CHECK_COMPAT(              |
|   x86_64_linux_clang,                 |
|   x86_64_windows_msvc)               |
+---------------------------------------+
```

### Step 1: Write the Export Source (Phase 1)

```cpp
// cross_platform_check.cpp -- 3 lines of user code
#include <boost/typelayout/tools/sig_export.hpp>
#include "my_types.hpp"    // your struct definitions

TYPELAYOUT_EXPORT_TYPES(PacketHeader, SensorRecord, SharedMemRegion)
```

`TYPELAYOUT_EXPORT_TYPES(...)` generates `main()` for you.

### Step 2: Compile and Run on Each Platform

```bash
# Build the signature exporter on each target platform
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -I./include -o sig_export example/cross_platform_check.cpp

# Run it to generate a .sig.hpp for this platform
./sig_export sigs/
# -> creates sigs/x86_64_linux_clang.sig.hpp
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

// Option B: Compile-time -- fails if any type differs
TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_clang, arm64_linux_clang)
```

### Step 4: Compile and Run the Check

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
  SensorRecord                MATCH       MATCH    ***  Serialization-free
  UnsafeStruct               DIFFER      DIFFER    *--  Needs serialization
  UnsafeWithPointer           MATCH       MATCH    **-  Layout OK (pointer values not portable)
--------------------------------------------------------------------------------

  [DIFFER] UnsafeStruct layout signatures:
    x86_64_linux_clang: [64-le]record[s:48,a:16]{@0:i64[s:8,a:8],...}
    x86_64_windows_msvc: [64-le]record[s:32,a:8]{@0:i32[s:4,a:4],...}

========================================================================
  Serialization-free (C1+C2): 3/5 (60%)
  Layout-compatible (C1):     4/5
  Needs serialization:        1/5
========================================================================
```

### Platform Naming Convention

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

### Fine-Grained Compile-Time Checks

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

### CMake Integration

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

## Common Pitfalls

### Padding bytes are uninitialized

Layout signature match guarantees identical field offsets and sizes, but
**padding bytes between fields are uninitialized**. If you `memcmp` two
structs that are layout-compatible, the comparison may fail due to
differing padding contents. Always compare field-by-field, or zero-fill
the struct before use (`= {}` or `memset`).

### Unsupported types

The following types cannot produce signatures (compile error):

- `void` -- use `void*` instead
- `T[]` (unbounded array) -- use `T[N]` with a known size
- Bare function types like `void(int)` -- use function pointers `void(*)(int)`

## Next Steps

- [API Reference](api-reference.md) -- all public symbols with signatures and examples
- [Migration Guide](migration-guide.md) -- upgrading from earlier versions
- [Best Practices](best-practices.md) -- field ordering, naming, CI integration
- [Examples](../example/) -- source code for the pipeline demos