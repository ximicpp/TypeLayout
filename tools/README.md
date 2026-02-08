# Boost.TypeLayout — Cross-Platform Compatibility Toolchain

A command-line toolchain for verifying binary layout compatibility of C++ types
across different platforms, architectures, and compilers.

## Overview

The toolchain implements a **two-phase pipeline**:

1. **Phase 1 (Export)**: Compile your types on each target platform using a P2996
   Clang compiler. This produces `.sig.hpp` header files containing `constexpr`
   signature data.

2. **Phase 2 (Compare)**: Include all `.sig.hpp` headers in a single C++17 program
   and compare signatures. No P2996 required — any standard compiler works.

**Key design**: Both phases use declarative C++ macros. You write two small `.cpp`
files — the toolchain compiles and runs them. No header scanning, no code
generation, no templates.

## Quick Start

### 1. Write the Export Source (Phase 1)

Create a `.cpp` file that lists your types:

```cpp
// export_types.cpp
#include <boost/typelayout/tools/sig_export.hpp>
#include "my_types.hpp"

TYPELAYOUT_EXPORT_TYPES(PacketHeader, SensorRecord, SharedMemRegion)
```

### 2. Write the Check Source (Phase 2)

Create a `.cpp` file that lists the platforms to compare:

```cpp
// check_compat.cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_linux_clang.sig.hpp"
#include <boost/typelayout/tools/compat_auto.hpp>

TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_linux_clang)
```

### 3. Run the Pipeline

```bash
# Full pipeline: export on each platform + compare
./tools/typelayout-compat check \
    --export-source export_types.cpp \
    --check-source check_compat.cpp \
    --platforms x86_64-linux-clang,arm64-linux-clang

# Export only (on one platform)
./tools/typelayout-compat export \
    --source export_types.cpp \
    --output sigs/

# Compare only (from pre-exported signatures)
./tools/typelayout-compat compare \
    --source check_compat.cpp \
    --sigs sigs/
```

## Commands

### `check` — Full Pipeline

Runs both phases: exports signatures on each platform (via Docker or locally),
then compiles and runs the Phase 2 check source.

```bash
typelayout-compat check \
    --export-source <export.cpp> \
    --check-source <check.cpp> \
    --platforms <platform1,platform2,...> \
    [--output <dir>] \
    [--include <dir>]
```

| Option | Description |
|--------|-------------|
| `--export-source` | Phase 1 source (uses `TYPELAYOUT_EXPORT_TYPES(...)`) |
| `--check-source` | Phase 2 source (uses `TYPELAYOUT_CHECK_COMPAT(...)`) |
| `--platforms`, `-p` | Comma-separated list of target platforms |
| `--output`, `-o` | Output directory for `.sig.hpp` files (default: `./typelayout-sigs`) |
| `--include`, `-I` | Additional include directory |

### `export` — Export Current Platform

Compiles and runs the Phase 1 export source for a single platform.
Requires a P2996-capable Clang compiler.

```bash
typelayout-compat export \
    --source <export.cpp> \
    [--output <dir>]
```

### `compare` — Compare Existing Signatures

Compiles and runs the Phase 2 check source against pre-exported signatures.

```bash
typelayout-compat compare \
    --source <check.cpp> \
    --sigs <directory>
```

### `list-platforms` — Show Available Platforms

Lists all platforms defined in `platforms.conf`.

```bash
typelayout-compat list-platforms
```

## Declarative Macros

### Phase 1: `TYPELAYOUT_EXPORT_TYPES(...)`

Generates a complete `main()` that exports type signatures to a `.sig.hpp` file.

```cpp
#include <boost/typelayout/tools/sig_export.hpp>
#include "my_types.hpp"

TYPELAYOUT_EXPORT_TYPES(PacketHeader, SensorRecord, MyStruct)
// → Compile with P2996, run with: ./export sigs/
```

### Phase 2: `TYPELAYOUT_CHECK_COMPAT(...)` (Runtime Report)

Generates a `main()` that prints a detailed compatibility report.

```cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_linux_clang.sig.hpp"
#include <boost/typelayout/tools/compat_auto.hpp>

TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_linux_clang)
// → Compile with any C++17 compiler. Run for report.
```

### Phase 2: `TYPELAYOUT_ASSERT_COMPAT(...)` (Compile-Time Assert)

Emits `static_assert` checks — compilation fails if layouts differ.

```cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_linux_clang.sig.hpp"
#include <boost/typelayout/tools/compat_auto.hpp>

TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_clang, arm64_linux_clang)
// → Compilation succeeds iff all types match.
```

## Platform Registry (`platforms.conf`)

Platforms are configured in `tools/platforms.conf` using INI-style sections:

```ini
[x86_64-linux-clang]
description = x86-64 Linux (Bloomberg Clang P2996)
mode = docker
docker_image = ghcr.io/aspect-labs/typelayout-p2996:latest
compiler = clang++
flags = -std=c++26 -freflection -freflection-latest -stdlib=libc++
```

### Configuration Fields

| Field | Required | Description |
|-------|----------|-------------|
| `description` | No | Human-readable platform description |
| `mode` | No | `docker` (default) or `local` |
| `docker_image` | For `docker` mode | Docker image with P2996 Clang |
| `compiler` | No | Compiler binary name (default: `clang++`) |
| `flags` | No | Compiler flags |
| `link_flags` | No | Linker flags |
| `env` | No | Comma-separated `KEY=VALUE` environment variables |

### Adding a Custom Platform

Add a new section to `platforms.conf`:

```ini
[my-custom-platform]
description = My Custom Build Environment
mode = docker
docker_image = myregistry.io/my-p2996-clang:latest
compiler = clang++
flags = -std=c++26 -freflection -freflection-latest -stdlib=libc++
```

## Signature Format

Exported `.sig.hpp` files contain `inline constexpr const char[]` arrays with
normalized layout strings:

```
[64-le]record[s:16,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2],@6:u16[s:2,a:2],@8:u32[s:4,a:4],@12:u32[s:4,a:4]}
```

This format captures:
- Architecture prefix (`[64-le]`)
- Total size and alignment (`s:16,a:4`)
- Each field's offset, type, size, and alignment (`@0:u32[s:4,a:4]`)

## Comparison Report

The compatibility report shows which types are safe for zero-copy transfer:

```
========================================================================
  Boost.TypeLayout — Cross-Platform Compatibility Report
========================================================================

Platforms compared: 3
  * arm64_macos_clang [64-le]
    pointer=8B, long=8B, wchar_t=4B, long_double=8B, max_align=16B
  * x86_64_linux_clang [64-le]
    pointer=8B, long=8B, wchar_t=4B, long_double=16B, max_align=16B
  * x86_64_windows_msvc [64-le]
    pointer=8B, long=4B, wchar_t=2B, long_double=8B, max_align=16B

------------------------------------------------------------------------
  Type                            Layout   Definition  Verdict
------------------------------------------------------------------------
  PacketHeader                     MATCH        MATCH  Serialization-free
  UnsafeStruct                    DIFFER       DIFFER  Needs serialization
------------------------------------------------------------------------

  87% of types (7/8) are serialization-free across all platforms.
  1 type(s) need serialization for cross-platform use.
========================================================================
```

## CI/CD Integration

### GitHub Actions (Reusable Workflow)

Use the provided reusable workflow in your repository:

```yaml
# .github/workflows/compat-check.yml
name: Type Compatibility
on: [push, pull_request]

jobs:
  compat-check:
    uses: aspect-labs/typelayout/.github/workflows/compat-check.yml@v1
    with:
      export_source: 'src/export_types.cpp'
      check_source: 'src/check_compat.cpp'
      platforms: 'x86_64-linux-clang,arm64-linux-clang'
```

The workflow:
1. Parses the platform list into a GitHub Actions matrix
2. Compiles and runs your `export_source` on each platform (via Docker)
3. Downloads all `.sig.hpp` artifacts
4. Compiles and runs your `check_source` with all signatures available
5. Uploads a compatibility report artifact

### Custom CI Pipeline

For non-GitHub CI systems, use the CLI directly:

```bash
# On each build agent (Phase 1):
./tools/typelayout-compat export \
    --source src/export_types.cpp \
    --output /shared/sigs/

# On any machine (Phase 2):
./tools/typelayout-compat compare \
    --source src/check_compat.cpp \
    --sigs /shared/sigs/
```

## CMake Integration

```cmake
include(cmake/TypeLayoutCompat.cmake)

# Option A: High-level one-liner
typelayout_add_compat_pipeline(
    NAME            myproject_compat
    EXPORT_SOURCE   src/export_types.cpp
    CHECK_SOURCE    src/check_compat.cpp
    SIGS_DIR        ${CMAKE_SOURCE_DIR}/sigs
    ADD_TEST
)

# Option B: Fine-grained control
typelayout_add_sig_export(
    TARGET sig_export
    SOURCE src/export_types.cpp
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/sigs
)

typelayout_add_compat_check(
    TARGET compat_check
    SOURCE src/check_compat.cpp
    SIGS_DIR ${CMAKE_SOURCE_DIR}/sigs
)
```

## Requirements

- **Phase 1**: Bloomberg Clang P2996 (or compatible fork) with `-freflection`
- **Phase 2**: Any C++17 compiler (`g++`, `clang++`, MSVC)
- **Docker mode**: Docker Engine for cross-platform builds
- **Bash**: GNU Bash 4.0+ (Linux/macOS/WSL)

## File Reference

| File | Description |
|------|-------------|
| `tools/typelayout-compat` | Main CLI orchestration script |
| `tools/platforms.conf` | Platform registry (INI format) |
| `include/boost/typelayout/tools/sig_export.hpp` | `SigExporter` class + `TYPELAYOUT_EXPORT_TYPES` macro |
| `include/boost/typelayout/tools/compat_auto.hpp` | `TYPELAYOUT_CHECK_COMPAT` / `TYPELAYOUT_ASSERT_COMPAT` macros |
| `include/boost/typelayout/tools/compat_check.hpp` | `CompatReporter` class + `layout_match()` |
| `include/boost/typelayout/tools/sig_types.hpp` | Shared `TypeEntry` / `PlatformInfo` types |
| `cmake/TypeLayoutCompat.cmake` | CMake integration functions |
| `.github/workflows/compat-check.yml` | GitHub Actions reusable workflow |