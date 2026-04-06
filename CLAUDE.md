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

Core mechanism: Layout Signature — encode a type's byte-level layout as a compile-time string via P2996. Two applications derived from the signature:
- Cross-platform comparison: `sigA == sigB` → byte layouts are identical
- Safety checking: `is_byte_copy_safe_v<T>` → pointer token scan in the signature
- `TYPELAYOUT_REGISTER_OPAQUE` — practical utility: user seals internal layout from signatures

Answer: byte_copy_safe AND signatures match → safe to memcpy across platforms.

Example signature: `[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:f64[s:8,a:8]}`

## Architecture

Header-only library. Two layers:

**Core layer** (requires P2996) — signature generation + type analysis:
```
include/boost/typelayout/
├── typelayout.hpp              # Umbrella header
├── signature.hpp               # get_layout_signature, get_arch_prefix
├── layout_traits.hpp           # detail::layout_traits<T>: signature + has_pointer
├── admission.hpp               # is_byte_copy_safe<T>, opaque_copy_safe<T>
├── fixed_string.hpp            # FixedString<N>: compile-time string, to_fixed_string()
├── opaque.hpp                  # TYPELAYOUT_REGISTER_OPAQUE macro
├── fwd.hpp                     # Forward declarations
├── config.hpp                  # Configuration macros
└── detail/
    ├── signature_impl.hpp      # TypeSignature<T>::calculate() — the core recursive engine
    ├── reflect.hpp             # P2996 reflection helpers (member/base count, virtual base check)
    ├── type_map.hpp            # Type → canonical name mapping (int→i32, double→f64, etc.)
    └── sig_parser.hpp          # Signature string parser (C++17): pointer token matching
```

**Tools layer** (cross-platform pipeline):
```
include/boost/typelayout/tools/
├── sig_export.hpp              # SigExporter, TYPELAYOUT_EXPORT_TYPES (P2996)
├── compat_check.hpp            # CompatReporter: cross-platform compatibility report
├── sig_types.hpp               # TypeEntry, PlatformInfo (shared data types)
├── safety_level.hpp            # Internal: SafetyLevel enum for CompatReporter display
├── platform_detect.hpp         # Arch/OS/compiler detection macros
└── detail/foreach.hpp          # Variadic macro helper
```

## Core Concept

One core mechanism — Layout Signature. Everything else is an application of the signature or an implementation detail.

**Layout Signature** — `get_layout_signature<T>()` → `FixedString`

Encode a type's byte-level layout as a compile-time string. Captures: architecture prefix (bit-width + endianness), leaf field types (including pointer/reference tokens), sizes, alignments, and offsets. Does NOT capture: field names, type names, or nesting structure. Nested structs and base classes are recursively flattened into the parent's offset space — only leaf fields with absolute offsets survive. Design trade-off: correctly answers "can I memcpy these bytes?" but cannot detect semantic field reordering (e.g., swapped field names with same types).

Two applications derived from the signature:

**Application 1: Cross-platform layout comparison** — Two platforms produce the same signature → byte layouts are identical. Different signatures → pinpoint exactly which types differ. Layout match is just signature string equality.

**Application 2: Byte-copy safety checking** — `is_byte_copy_safe_v<T>`: can this type's bytes be safely transported? Pointer detection reuses the Layout Signature (scans for pointer tokens `ptr`, `fnptr`, `ref`, `rref`, `memptr`, `vptr` in the signature string). Polymorphic types are caught via the `vptr` token encoded in their signature. `trivially_copyable` is only a fast-path shortcut.

**The answer**: byte_copy_safe AND signatures match → safe to memcpy across platforms. Both checks are compile-time: byte_copy_safe via `static_assert`, signature comparison via `static_assert` in a CI build that `#include`s exported `.sig.hpp` from each target platform.

## Implementation Details

These support the core mechanism but are not part of the conceptual framework:

- **FixedString<N>**: Compile-time string type. N = exact content length. All signatures are FixedString.
- **consteval**: All signature-generating functions are `consteval`, not `constexpr`.
- **Opaque registration** (`TYPELAYOUT_REGISTER_OPAQUE` macros): Practical utility for users who want to seal a type's internal layout from the signature, replacing it with `O(Tag|N|A)` (tag + size + align only). The system can analyze any type — Opaque is not about system limitations. Users choose it to keep internal implementation details out of signatures, stabilize signatures across internal refactors, or handle types with valid-after-memcpy pointers (e.g., `offset_ptr`). Two registration paths: `TYPELAYOUT_REGISTER_OPAQUE` (requires trivially_copyable) and `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE` (no such requirement).
- **Safety Classification**: Four-tier display labels (TrivialSafe/PlatformVariant/PointerRisk/Opaque) used by `CompatReporter` output. Not a decision predicate — use `is_byte_copy_safe_v` + signature comparison for programmatic decisions.
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
- **Polymorphic record**: `record[s:SIZE,a:ALIGN,vptr]{...}` (vptr flag, no offset — P2996 does not expose vptr position)
- **Union**: `union[s:SIZE,a:ALIGN]{@OFFSET:TYPE,...}`
- **Opaque**: `O(Tag|N|A)`, container: `O(Tag|N|A)<elem_sig>`, map: `O(Tag|N|A)<key_sig,val_sig>`
- **Array**: `array[s:SIZE,a:ALIGN]<elem_sig,count>`, byte arrays: `bytes[s:N,a:1]`

## Code Map

**Public API**:

| Role | Name | Entry point |
|------|------|-------------|
| Core | Layout Signature | `signature.hpp` → `get_layout_signature<T>()` |
| App 1 | Layout comparison | Signature string equality (`sigA == sigB`) |
| App 2 | Safety checking | `admission.hpp` → `is_byte_copy_safe_v<T>` |
| Utility | Opaque | `opaque.hpp` → `TYPELAYOUT_REGISTER_OPAQUE` macros |
| Pipeline | Cross-platform CI | `tools/sig_export.hpp` → `SigExporter`, `tools/compat_check.hpp` → `CompatReporter` |

**Internal implementation**:

| What | Where |
|------|-------|
| Recursive flattening engine | `detail/signature_impl.hpp` → `TypeSignature<T>::calculate()` |
| Type → canonical name mapping | `detail/type_map.hpp` |
| P2996 reflection helpers | `detail/reflect.hpp` |
| Pointer detection | `layout_traits.hpp` → `detail::layout_traits<T>::has_pointer` |
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
