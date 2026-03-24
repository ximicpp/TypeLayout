# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test

This project requires Bloomberg Clang P2996 (C++26 static reflection). The compiler is NOT available natively on Windows or macOS — use WSL or Docker.

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

### Docker (any platform, fallback)

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

- `LD_LIBRARY_PATH` is required at runtime for the P2996 libc++
- All 12 tests must pass (7 core + 5 tools)
- If build fails with stale cache, delete `build/` directory and reconfigure
- Git push must be done from Windows git (WSL lacks credentials): `git push origin main`

## Git Conventions

- Commit message format: `type: description` (e.g., `fix:`, `feat:`, `test:`, `docs:`, `chore:`, `perf:`)
- Push from Windows, not WSL (HTTPS credentials are in Windows credential manager)
- No emoji in code, documentation, or commit messages. Use text markers like `[FIXED]`, `[TODO]`, `(!)` instead.

## Project Overview

**Boost.TypeLayout** — compile-time type layout signature library via C++26 static reflection (P2996).

Core question it answers: "Can struct T be safely `memcpy`'d between A and B?"

Two `consteval` functions form the public API:
- `get_layout_signature<T>()` → `FixedString` with full byte-level layout
- Compare signatures directly: `get_layout_signature<T>() == get_layout_signature<U>()` → `true` if identical byte layouts

Signatures are deterministic, human-readable strings encoding field types, sizes, alignments, offsets, and padding. Example: `[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:f64[s:8,a:8]}`

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

**Tools layer** (mostly C++17, no P2996 needed) — safety analysis + cross-platform:
```
include/boost/typelayout/tools/
├── safety_level.hpp            # SafetyLevel enum + classify_signature() + sig_has_padding()
├── transfer.hpp                # Forwarding header → core transfer.hpp (backward compat)
├── sig_export.hpp              # SigExporter, TYPELAYOUT_EXPORT_TYPES — requires P2996
├── sig_types.hpp               # TypeEntry, PlatformInfo structs (shared data types)
├── compat_check.hpp            # CompatReporter: are_transfer_safe(types, platforms), ABI fingerprinting
├── platform_detect.hpp         # Arch/OS/compiler detection macros + get_data_model()
└── detail/foreach.hpp          # Variadic macro helper
```

Tools layer must not `#include` core headers unless the tool file is explicitly marked as P2996-required.

## Key Design Concepts

- **FixedString<N>**: N = exact content length (no wasted capacity, no over-allocation). All signatures are FixedString.
- **consteval, not constexpr** for all signature-generating functions. `constexpr` is only for helper utilities.
- **Flattening**: Structs are recursively flattened — field names and inheritance erased, only byte identity preserved.
- **Safety levels** (ordered worst→best): `Opaque > PointerRisk > PlatformVariant > PaddingRisk > TrivialSafe`
- **Terminology**: "serialization-free" = TrivialSafe (strict); "transfer-safe" = byte-copy safe + layout match (broad, includes PaddingRisk/Opaque/PlatformVariant)
- **Dual-path padding detection**: `compute_has_padding` (compile-time bitmap via P2996) cross-validated against `sig_has_padding` (runtime string parser), enforced by `static_assert`. Both must agree.
- **Opaque types**: Unanalyzable types registered via `TYPELAYOUT_REGISTER_OPAQUE`. Signature format: `O(Tag|N|A)`. Opaque container: `O(Tag|N|A)<elem_sig>`. Opaque map: `O(Tag|N|A)<key_sig,val_sig>`. Opaque types must be registered BEFORE any signature generation that encounters them.
- **Admission predicates** (`admission.hpp`): `is_byte_copy_safe_v<T>` recursively checks each member/base, accepting registered relocatable opaque types. Orthogonal to SafetyLevel — a type can be `SafetyLevel::Opaque` AND `is_byte_copy_safe == true`.
- **Serialization-free blocking**: Only `PointerRisk` blocks serialization-free. `PaddingRisk`, `PlatformVariant`, and `Opaque` do not block when signatures match across the specified platforms.
- **Array element recursion**: `type_has_opaque` and `compute_has_padding` recurse into array element types via `std::remove_all_extents_t`.
- **Virtual inheritance rejection**: `signature_impl.hpp` rejects types with virtual bases at compile time.

## Code Conventions

- Namespace: `boost::typelayout::v1` (inline), detail code in `boost::typelayout::v1::detail`
  - All public symbols are in `inline namespace v1` for ABI versioning
  - User code writes `boost::typelayout::` (the `v1` is transparent)
- Header guards: `#ifndef BOOST_TYPELAYOUT_<FILENAME>_HPP` / `#define` / `#endif`
- Include order: project headers, then standard library, then test utilities
- No .cpp source files in the library itself — header-only. Tests are .cpp files.
- `static_assert` preferred: every compile-time property should be validated with `static_assert` first, optionally printed at runtime for debugging.
- Inline test types: test structs/classes are defined inside the test .cpp file, not in shared headers.

## Test Matrix

12 tests (7 core + 5 tools):

| Test | Label | C++ | What it validates |
|------|-------|-----|-------------------|
| `test_fixed_string` | core | C++26 | FixedString ops, to_fixed_string |
| `test_opaque` | core | C++26 | Opaque registration, CV-qualified, base class, EBO |
| `test_layout_traits` | core | C++26 | Signature consistency, has_pointer/opaque/padding, field_count |
| `test_empty_member_probe` | core | C++26 | Empty member/NUA/EBO signature correctness |
| `test_padding_precision` | core | C++26 | Byte-coverage bitmap vs sig parser, classify consistency |
| `test_byte_copy_safe` | core | C++26 | Recursive byte-copy admission, opaque elements, polymorphic rejection |
| `test_classify` | tools | C++26 | Five-tier classify<T> for all type categories |
| `test_transfer` | tools | C++26 | is_transfer_safe, cross-endpoint transfer verification |
| `test_sig_export` | tools | C++26 | SigExporter output structure |
| `test_rt_padding` | tools | C++17 | Runtime sig_has_padding (no P2996) |
| `test_compat_check` | tools | C++17 | CompatReporter, classify_signature, are_transfer_safe, ABI equivalence |
| `test_advanced_types` | core | C++26 | Multi-dimensional arrays, nested unions, cross-platform round-trip, is_transfer_safe |

CMake test labels: P2996 core tests use `LABELS "core"` with 120s timeout; C++17-only tests use `LABELS "tools"` with 30s timeout. C++17 tests set `-std=c++17 -stdlib=libc++` manually and do NOT link the `typelayout` interface library.

## Signature Format

- **Arch prefix**: `[BITS-ENDIAN]` e.g. `[64-le]`, `[32-be]`
- **Record**: `record[s:SIZE,a:ALIGN]{@OFFSET:TYPE[s:SIZE,a:ALIGN],...}`
- **Padding**: `pad:N` for N padding bytes at a given offset
- **Opaque**: `O(Tag|N|A)`, container: `O(Tag|N|A)<elem_sig>`, map: `O(Tag|N|A)<key_sig,val_sig>`
- **Array**: `TYPE[N]` for fixed-size arrays, element type is recursed into

## Code Map — Key Entry Points

| Task | Start here |
|------|-----------|
| How signatures are generated | `detail/signature_impl.hpp` → `TypeSignature<T>::calculate()` |
| How types are classified | `detail/reflect.hpp` → `classify_type()`, `detail/type_map.hpp` |
| How layout_traits works | `layout_traits.hpp` → `layout_traits<T>` struct |
| How padding is detected | `layout_traits.hpp` → `compute_has_padding<T>()` (bitmap) |
| How safety is classified | `tools/safety_level.hpp` → `classify_signature()` |
| How opaque types work | `opaque.hpp` → macros + `has_opaque_signature` concept |
| How admission works | `admission.hpp` → `is_byte_copy_safe<T>`, `opaque_copy_safe<T>` |
| Cross-platform pipeline | `tools/sig_export.hpp` (Phase 1) → `tools/compat_check.hpp` (Phase 2) |
| Transfer-safe query | `tools/compat_check.hpp` → `CompatReporter::are_transfer_safe()` |
| Data model / ABI detect | `tools/platform_detect.hpp` → `get_data_model()` |

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
