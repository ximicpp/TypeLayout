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

This design means Phase 2 can run on any machine, even without access to the
target platforms.

## Quick Start

```bash
# List available target platforms
./tools/typelayout-compat list-platforms

# Full pipeline: export + compare across platforms
./tools/typelayout-compat check \
    --types include/protocol/types.hpp \
    --platforms x86_64-linux-clang,arm64-linux-clang

# Export signatures for the current platform only
./tools/typelayout-compat export \
    --types include/protocol/types.hpp \
    --output sigs/

# Compare previously exported signature files
./tools/typelayout-compat compare --sigs sigs/
```

## Commands

### `check` — Full Pipeline

Runs both phases automatically: exports signatures on each specified platform
(via Docker or locally) and then compares them all.

```bash
typelayout-compat check \
    --types <header.hpp> \
    --platforms <platform1,platform2,...> \
    [--output <dir>] \
    [--include <dir>]
```

| Option | Description |
|--------|-------------|
| `--types`, `-t` | Path to C++ header with struct/class definitions |
| `--platforms`, `-p` | Comma-separated list of target platforms |
| `--output`, `-o` | Output directory for `.sig.hpp` files (default: `./typelayout-sigs`) |
| `--include`, `-I` | Additional include directory |

### `export` — Export Current Platform

Compiles and runs the signature exporter for the current platform only.
Requires a P2996-capable Clang compiler.

```bash
typelayout-compat export \
    --types <header.hpp> \
    [--output <dir>]
```

### `compare` — Compare Existing Signatures

Compares pre-exported `.sig.hpp` files from a directory. Useful when signature
files were generated separately (e.g., in CI or on remote machines).

```bash
typelayout-compat compare --sigs <directory>
```

### `list-platforms` — Show Available Platforms

Lists all platforms defined in `platforms.conf`.

```bash
typelayout-compat list-platforms
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

## How It Works

### Auto-Detection

The toolchain automatically scans your types header for `struct` and `class`
definitions:

```cpp
// my_types.hpp
struct PacketHeader {        // ← auto-detected
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t payload_len;
    uint32_t checksum;
};
```

Each detected type is registered with `SigExporter::add<T>()` in the generated
export program.

### Signature Format

Exported `.sig.hpp` files contain `inline constexpr const char[]` arrays with
normalized layout strings:

```
[64-le]record[s:16,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2],@6:u16[s:2,a:2],@8:u32[s:4,a:4],@12:u32[s:4,a:4]}
```

This format captures:
- Architecture prefix (`[64-le]`)
- Total size and alignment (`s:16,a:4`)
- Each field's offset, type, size, and alignment (`@0:u32[s:4,a:4]`)

### Comparison Report

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
      types_header: 'include/protocol/types.hpp'
      platforms: 'x86_64-linux-clang,arm64-linux-clang'
```

The workflow:
1. Parses the platform list into a GitHub Actions matrix
2. Exports signatures in parallel (one job per platform)
3. Downloads all `.sig.hpp` artifacts and runs the comparison
4. Uploads a compatibility report artifact

### Custom CI Pipeline

For non-GitHub CI systems, use the CLI directly:

```bash
# On each build agent (Phase 1):
./tools/typelayout-compat export \
    --types include/types.hpp \
    --output /shared/sigs/

# On any machine (Phase 2):
./tools/typelayout-compat compare --sigs /shared/sigs/
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
| `tools/sig_export_template.cpp.in` | Phase 1 export program template |
| `tools/compat_check_template.cpp.in` | Phase 2 checker program template |
| `include/boost/typelayout/tools/sig_export.hpp` | `SigExporter` class (P2996) |
| `include/boost/typelayout/tools/compat_check.hpp` | `CompatReporter` class (C++17) |
| `.github/workflows/compat-check.yml` | GitHub Actions reusable workflow |
