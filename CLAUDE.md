# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test

This project requires a P2996-capable compiler (C++26 static reflection). Supported compilers:
- **GCC 16+** (trunk merged P2996 on 2026-01-15) — recommended
- **Bloomberg Clang P2996 fork** — experimental, legacy option

Neither is available natively on Windows or macOS — use WSL or Docker.

### Windows (WSL) — Primary Method

Bloomberg Clang P2996 is installed in WSL at `/root/clang-p2996-install/`.

**Configure** (only needed once or after CMakeLists.txt changes):
```bash
wsl -e bash -c 'cd /mnt/g/workspace/TypeLayout && export CXX=/root/clang-p2996-install/bin/clang++ && cmake -B build -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++"'
```

**Build**:
```bash
wsl -e bash -c 'cd /mnt/g/workspace/TypeLayout && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build -j$(nproc)'
```

**Test**:
```bash
wsl -e bash -c 'cd /mnt/g/workspace/TypeLayout && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && ctest --test-dir build --output-on-failure'
```

### macOS — Local Clang or Docker

First detect if a local P2996-capable clang exists:
```bash
clang++ -std=c++26 -freflection -x c++ -c /dev/null -o /dev/null 2>/dev/null && echo "P2996_OK" || echo "P2996_MISSING"
```

If P2996_OK, build directly:
```bash
cmake -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++"
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

If P2996_MISSING, use Docker (see below).

### Docker with GCC 16 — Recommended

```bash
docker build -t typelayout-gcc16:latest -f .github/docker/Dockerfile.gcc16 .github/docker/
docker run --rm -v $(pwd):/workspace -w /workspace typelayout-gcc16:latest \
  bash -c 'cmake -B build -DCMAKE_CXX_COMPILER=g++ && cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure'
```

### Docker with Bloomberg Clang (legacy fallback)

```bash
docker run --rm -v $(pwd):/workspace -w /workspace \
  -e LD_LIBRARY_PATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu \
  ghcr.io/ximicpp/typelayout-p2996:latest \
  bash -c 'cmake -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++" && cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure'
```

### Run a Single Test

After building, run one test by name:
```bash
ctest --test-dir build -R test_layout_traits --output-on-failure
```

### Important Notes

- GCC 16: `LD_LIBRARY_PATH` may be needed for the GCC runtime (`/opt/gcc-latest/lib64`)
- Bloomberg Clang: `LD_LIBRARY_PATH` is required at runtime for the P2996 libc++
- If build fails with stale cache, delete `build/` directory and reconfigure
- Git push must be done from Windows git (WSL lacks credentials): `git push origin main`

## Git Conventions

- Commit message format: `type: description` (e.g., `fix:`, `feat:`, `test:`, `docs:`, `chore:`, `perf:`)
- Push from Windows, not WSL (HTTPS credentials are in Windows credential manager)
- No emoji in code, documentation, or commit messages. Use text markers like `[FIXED]`, `[TODO]`, `(!)` instead.

## Project Overview

**Boost.TypeLayout** — compile-time type layout signature library via C++26 static reflection (P2996).

Core question: **"Can this struct be safely memcpy'd between A and B?"**

Public API:
- `get_layout_signature<T>()` → `FixedString` — compile-time byte-level layout encoding
- `is_byte_copy_safe_v<T>` → can this type's bytes be safely transported?
- `is_transfer_safe<T>(remote_sig)` → byte-copy safe AND layout matches remote?

Example signature: `[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:f64[s:8,a:8]}`

## Architecture

Header-only library. Two layers:

**Core layer** (requires P2996) — signature generation + type analysis:
```
include/boost/typelayout/
├── typelayout.hpp              # Umbrella header
├── signature.hpp               # get_layout_signature, get_arch_prefix
├── layout_traits.hpp           # layout_traits<T>: signature + has_pointer/padding/opaque/etc.
├── admission.hpp               # is_byte_copy_safe<T>, opaque_copy_safe<T>
├── transfer.hpp                # is_transfer_safe<T>(sig) — cross-endpoint verification
├── fixed_string.hpp            # FixedString<N>: compile-time string, to_fixed_string()
├── opaque.hpp                  # TYPELAYOUT_REGISTER_OPAQUE macro
├── fwd.hpp                     # Forward declarations
├── config.hpp                  # Configuration macros
└── detail/
    ├── signature_impl.hpp      # TypeSignature<T>::calculate() — the core recursive engine
    ├── reflect.hpp             # P2996 reflection helpers (type classification, primitives)
    ├── type_map.hpp            # Type → canonical name mapping (int→i32, double→f64, etc.)
    └── sig_parser.hpp          # Signature string parser (C++17): padding detection, token matching
```

**Tools layer** (cross-platform pipeline):
```
include/boost/typelayout/tools/
├── sig_export.hpp              # SigExporter, TYPELAYOUT_EXPORT_TYPES (P2996)
├── compat_check.hpp            # CompatReporter: layout_match(), are_transfer_safe()
├── sig_types.hpp               # TypeEntry, PlatformInfo (shared data types)
├── safety_level.hpp            # Internal: SafetyLevel enum for CompatReporter display
├── platform_detect.hpp         # Arch/OS/compiler detection macros
└── detail/foreach.hpp          # Variadic macro helper
```

## Core Concepts

Six concepts form the conceptual foundation. Everything else is implementation detail.

### 1. Layout Signature

Encode a type's byte-level layout as a compile-time string. The signature captures: architecture prefix (bit-width + endianness), leaf field types, sizes, alignments, and offsets. It does NOT capture: field names, type names, or nesting structure. Two signatures are equal if and only if the byte layouts are identical.

### 2. Recursive Flattening

Nested structs and base classes are recursively expanded into the parent's offset space. Field names and structural boundaries are erased — only leaf fields with absolute offsets survive. This means two differently-structured types with identical byte layouts produce the same signature. Design trade-off: this correctly answers "can I memcpy these bytes?" but cannot detect semantic field reordering (e.g., swapped field names with same types).

### 3. Byte-Copy Safe

`is_byte_copy_safe_v<T>` — can this type's bytes be safely transported (to a buffer, over network, into shared memory)? Recursively checks each member and base: no pointers/references (would dangle after memcpy), not polymorphic (vtable pointer). Note: this is a transport-level concept, not a C++ object-model concept. `trivially_copyable` is used as a fast-path shortcut in the implementation, but is neither necessary nor sufficient for byte-copy safety.

### 4. Layout Match

Same struct, two platforms — are the layout signatures identical? Mismatches arise from platform-dependent types: `long` (LP64 8B vs LLP64 4B), `long double` (x86_64 16B vs ARM64 8B), `wchar_t` (Linux 4B vs Windows 2B), or different compiler packing rules. Checked by comparing exported signature strings — either via `static_assert` (compile-time) or `CompatReporter` (runtime report).

### 5. Transfer Safe

**transfer_safe = byte_copy_safe AND layout_match.** This is the final answer the project provides. A type is transfer-safe when its bytes can be memcpy'd on platform A, sent to platform B, and read back correctly.

### 6. Opaque

User-controlled analysis boundary. The user deliberately tells TypeLayout to stop recursive analysis and takes responsibility for byte-copy safety. This is NOT a system limitation — it is a design feature for user control. Typical uses: types containing pointers that remain valid after memcpy (e.g., `offset_ptr`, fixed-mapped shared memory), third-party types already verified by other means, or types whose internal structure should not be exposed in signatures. Signature records only tag + size + align: `O(Tag|N|A)`. Registered via `TYPELAYOUT_REGISTER_OPAQUE` (requires trivially_copyable) or `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE` (no such requirement).

## Implementation Details

These support the core concepts but are not part of the conceptual framework:

- **FixedString<N>**: Compile-time string type. N = exact content length. All signatures are FixedString.
- **consteval**: All signature-generating functions are `consteval`, not `constexpr`.
- **Safety Classification**: Five-tier display labels (TrivialSafe/PaddingRisk/PlatformVariant/PointerRisk/Opaque) used by `CompatReporter` output. Not a decision predicate — use `is_byte_copy_safe` + `layout_match` for programmatic decisions.
- **Dual-path padding detection**: `compute_has_padding` (compile-time bitmap) cross-validated against `sig_has_padding` (string parser) via `static_assert`. Internal correctness check.
- **trivially_copyable**: Fast-path in `is_byte_copy_safe` admission check. Not a core concept — it is a C++ object-model property orthogonal to transport safety.
- **Virtual inheritance rejection**: Types with virtual bases are rejected at compile time.

## Code Conventions

- Namespace: `boost::typelayout::v1` (inline), detail code in `boost::typelayout::v1::detail`
  - All public symbols are in `inline namespace v1` for ABI versioning
  - User code writes `boost::typelayout::` (the `v1` is transparent)
- Header guards: `#ifndef BOOST_TYPELAYOUT_<FILENAME>_HPP` / `#define` / `#endif`
- Include order: project headers, then standard library, then test utilities
- No .cpp source files in the library itself — header-only. Tests are .cpp files.
- `static_assert` preferred: every compile-time property should be validated with `static_assert` first, optionally printed at runtime for debugging.
- Inline test types: test structs/classes are defined inside the test .cpp file, not in shared headers.

## Signature Format

- **Arch prefix**: `[BITS-ENDIAN]` e.g. `[64-le]`, `[32-be]`
- **Record**: `record[s:SIZE,a:ALIGN]{@OFFSET:TYPE[s:SIZE,a:ALIGN],...}`
- **Padding**: `pad:N` for N padding bytes at a given offset
- **Opaque**: `O(Tag|N|A)`, container: `O(Tag|N|A)<elem_sig>`, map: `O(Tag|N|A)<key_sig,val_sig>`
- **Array**: `TYPE[N]` for fixed-size arrays, element type is recursed into

## Code Map

**Public API** (6 core concepts):

| Concept | Entry point |
|---------|-------------|
| Layout Signature | `signature.hpp` → `get_layout_signature<T>()` |
| Byte-Copy Safe | `admission.hpp` → `is_byte_copy_safe_v<T>` |
| Transfer Safe | `transfer.hpp` → `is_transfer_safe<T>(remote_sig)` |
| Layout Match | `tools/compat_check.hpp` → `layout_match(a, b)` |
| Opaque | `opaque.hpp` → `TYPELAYOUT_REGISTER_OPAQUE` macros |
| Cross-platform pipeline | `tools/sig_export.hpp` → `SigExporter`, `tools/compat_check.hpp` → `CompatReporter` |

**Internal implementation**:

| What | Where |
|------|-------|
| Recursive flattening engine | `detail/signature_impl.hpp` → `TypeSignature<T>::calculate()` |
| Type → canonical name mapping | `detail/type_map.hpp` |
| P2996 reflection helpers | `detail/reflect.hpp` |
| Padding detection (bitmap) | `layout_traits.hpp` → `compute_has_padding<T>()` |
| Signature string parser | `detail/sig_parser.hpp` |
| Display classification | `tools/safety_level.hpp` → `SafetyLevel` (internal to CompatReporter) |

## Available Skills (.claude/commands/)

| Skill | Purpose |
|-------|---------|
| `/build-test` | Build and run all 12 tests |
| `/commit`, `/push` | Git workflow |
| `/sig-check <Type>` | Generate signature for a type interactively |
| `/add-type-category` | Add a new type to signature generation |
| `/add-tool` | Add a new analyzer to the tools layer |
| `/add-test` | Write a new test following conventions |
| `/debug-signature` | Debug signature generation issues |
| `/modify-signature-format` | Change the signature grammar (breaking) |
| `/review` | Review code design and implementation quality |
