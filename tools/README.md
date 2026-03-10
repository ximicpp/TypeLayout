# Boost.TypeLayout Tools

Cross-platform layout compatibility toolchain for verifying that C++ struct
layouts are identical across compilers, operating systems, and architectures.

## Two-Phase Pipeline

The toolchain separates layout analysis into two independent phases:

**Phase 1 (Export)** runs on each target platform. It requires Bloomberg Clang
P2996 with `-freflection`. The output is a self-contained `.sig.hpp` header
containing `constexpr` signature strings and platform metadata.

**Phase 2 (Check)** runs on any single machine. It requires only a C++17
compiler. It includes all `.sig.hpp` headers produced in Phase 1 and compares
signatures across platforms.

```
Phase 1: per-platform (P2996 required)
  x86_64 Linux  --> sig_export --> x86_64_linux_clang.sig.hpp  --+
  ARM64 macOS   --> sig_export --> arm64_macos_clang.sig.hpp   --+--> Phase 2
  x86_64 Windows -> sig_export --> x86_64_windows_msvc.sig.hpp --+

Phase 2: any machine (C++17 only)
  #include all .sig.hpp headers
  TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang, x86_64_windows_msvc)
```

## SigExporter

`SigExporter` (header `<boost/typelayout/tools/sig_export.hpp>`) collects type
signatures on the current platform and writes a `.sig.hpp` file.

```cpp
#include <boost/typelayout/tools/sig_export.hpp>
#include "my_types.hpp"

int main() {
    boost::typelayout::SigExporter ex;          // auto-detect platform name
    ex.add<PacketHeader>("PacketHeader");
    ex.add<SensorRecord>("SensorRecord");
    int rc = ex.write("sigs/x86_64_linux_clang.sig.hpp");  // 0 on success
    return rc;
}
```

`add<T>` enforces `std::is_trivially_copyable_v<T>` via `static_assert`. The
platform name is auto-detected from compiler macros (e.g., `x86_64_linux_clang`).
To override: `SigExporter ex("my_custom_platform")`.

## TYPELAYOUT_EXPORT_TYPES

The macro generates `main()` for you, reducing Phase 1 to three lines:

```cpp
// export_types.cpp
#include <boost/typelayout/tools/sig_export.hpp>
#include "my_types.hpp"

TYPELAYOUT_EXPORT_TYPES(PacketHeader, SensorRecord, SharedMemRegion)
```

Compile with P2996 and run:

```bash
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -I./include -o sig_export export_types.cpp
./sig_export sigs/
# writes sigs/<platform_name>.sig.hpp
```

To register types on an existing exporter without generating `main()`:

```cpp
boost::typelayout::SigExporter ex("custom_platform");
TYPELAYOUT_REGISTER_TYPES(ex, PacketHeader, SensorRecord)
ex.write("sigs/custom_platform.sig.hpp");
```

## CompatReporter

`CompatReporter` (header `<boost/typelayout/tools/compat_check.hpp>`) compares
`.sig.hpp` files and produces a report. It requires only C++17.

```cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_macos_clang.sig.hpp"
#include <boost/typelayout/tools/compat_check.hpp>

namespace lx = boost::typelayout::platform::x86_64_linux_clang;
namespace mc = boost::typelayout::platform::arm64_macos_clang;

boost::typelayout::compat::CompatReporter reporter;
reporter.add_platform(lx::get_platform_info());
reporter.add_platform(mc::get_platform_info());
reporter.print_report(std::cout);
// or reporter.print_diff_report(std::cout) for ^--- diff annotations
```

## TYPELAYOUT_CHECK_COMPAT and TYPELAYOUT_ASSERT_COMPAT

The macros reduce Phase 2 to a few `#include` lines. Both are in
`<boost/typelayout/tools/compat_auto.hpp>`.

**Runtime report** (`TYPELAYOUT_CHECK_COMPAT`): generates `main()` that prints
the compatibility table and exits 0.

```cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_macos_clang.sig.hpp"
#include "sigs/x86_64_windows_msvc.sig.hpp"
#include <boost/typelayout/tools/compat_auto.hpp>

TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang, x86_64_windows_msvc)
```

**Compile-time assertion** (`TYPELAYOUT_ASSERT_COMPAT`): uses `static_assert` for
each type. Compilation fails if any type's layout differs across the listed platforms.

```cpp
TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_clang, arm64_macos_clang)
```

Both macros accept two or more platform names. Platform names must match the
namespace names in the `.sig.hpp` headers.

## Safety Levels

The compatibility report classifies each type using five tiers from
`SafetyLevel` (`<boost/typelayout/tools/safety_level.hpp>`):

| Level | Value | Meaning |
|-------|-------|---------|
| `TrivialSafe` | 0 | Safe for zero-copy and cross-platform transfer |
| `PaddingRisk` | 1 | Padding bytes may leak uninitialized data |
| `PlatformVariant` | 2 | Layout differs across platforms (`wchar_t`, `long double`, bit-fields) |
| `PointerRisk` | 3 | Contains pointers; values are address-space-specific |
| `Opaque` | 4 | Unanalyzable type; safety cannot be determined |

Runtime classification from a signature string (C++17, no P2996):

```cpp
#include <boost/typelayout/tools/safety_level.hpp>
SafetyLevel level = boost::typelayout::classify_signature(sig_string);
```

## CMake Integration

```cmake
include(cmake/TypeLayoutCompat.cmake)

# One-liner: creates export target, check target, and optional CTest entry
typelayout_add_compat_pipeline(
    NAME          my_compat
    EXPORT_SOURCE export_types.cpp
    CHECK_SOURCE  check_compat.cpp
    SIGS_DIR      ${CMAKE_SOURCE_DIR}/sigs
    ADD_TEST
)

# Fine-grained:
typelayout_add_sig_export(
    TARGET sig_export
    SOURCE export_types.cpp
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/sigs
)

typelayout_add_compat_check(
    TARGET compat_check
    SOURCE check_compat.cpp
    SIGS_DIR ${CMAKE_SOURCE_DIR}/sigs
)
```

## File Reference

| File | Description |
|------|-------------|
| `tools/typelayout-compat` | CLI orchestration script |
| `tools/platforms.conf` | Platform registry (INI format) |
| `include/boost/typelayout/tools/sig_export.hpp` | `SigExporter`, `TYPELAYOUT_EXPORT_TYPES`, `TYPELAYOUT_REGISTER_TYPES` |
| `include/boost/typelayout/tools/compat_check.hpp` | `CompatReporter` |
| `include/boost/typelayout/tools/compat_auto.hpp` | `TYPELAYOUT_CHECK_COMPAT`, `TYPELAYOUT_ASSERT_COMPAT` |
| `include/boost/typelayout/tools/safety_level.hpp` | `SafetyLevel`, `classify_signature`, `sig_has_padding` |
| `include/boost/typelayout/tools/sig_types.hpp` | `TypeEntry`, `PlatformInfo` (C++17 shared types) |
| `include/boost/typelayout/tools/platform_detect.hpp` | Compiler/arch detection macros |
| `cmake/TypeLayoutCompat.cmake` | CMake helper functions |

## Requirements

- **Phase 1**: Bloomberg Clang P2996 with `-freflection -freflection-latest -stdlib=libc++`
- **Phase 2**: Any C++17 compiler (`clang++`, `g++`, MSVC)
- **Docker mode**: Docker Engine (for cross-platform Phase 1 builds on a single machine)
