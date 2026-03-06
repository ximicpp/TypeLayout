# TypeLayout - Project Instructions

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

### Important Notes

- `LD_LIBRARY_PATH` is required at runtime for the P2996 libc++
- All 10 tests must pass (5 core + 5 tools)
- If build fails with stale cache, delete `build/` directory and reconfigure
- Git push must be done from Windows git (WSL lacks credentials): `git push origin main`

## Git Conventions

- Commit message format: `type: description` (e.g., `fix:`, `test:`, `docs:`, `chore:`, `perf:`)
- Push from Windows, not WSL (HTTPS credentials are in Windows credential manager)

## Project Overview

**Boost.TypeLayout** — compile-time type layout signature library via C++26 static reflection (P2996).

Core question it answers: "Can struct T be safely `memcpy`'d between A and B?"

Two `consteval` functions form the public API:
- `get_layout_signature<T>()` → `FixedString` with full byte-level layout
- `layout_signatures_match<T, U>()` → `true` if identical byte layouts

Signatures are deterministic, human-readable strings encoding field types, sizes, alignments, offsets, and padding. Example: `[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:f64[s:8,a:8]}`

## Architecture

Header-only library. Two layers:

**Core layer** (requires P2996) — signature generation + type analysis:
```
include/boost/typelayout/
├── typelayout.hpp              # Umbrella header
├── signature.hpp               # get_layout_signature, layout_signatures_match, get_arch_prefix
├── layout_traits.hpp           # layout_traits<T>: signature + has_pointer/padding/opaque/etc.
├── fixed_string.hpp            # FixedString<N>: compile-time string, to_fixed_string()
├── opaque.hpp                  # TYPELAYOUT_OPAQUE_TYPE/CONTAINER/MAP, TYPELAYOUT_REGISTER_OPAQUE
├── fwd.hpp                     # Forward declarations
├── config.hpp                  # Configuration macros
└── detail/
    ├── signature_impl.hpp      # TypeSignature<T>::calculate() — the core recursive engine
    ├── reflect.hpp             # P2996 reflection helpers (type classification, primitives)
    └── type_map.hpp            # Type → canonical name mapping (int→i32, double→f64, etc.)
```

**Tools layer** (mostly C++17, no P2996 needed) — safety analysis + cross-platform:
```
include/boost/typelayout/tools/
├── safety_level.hpp            # SafetyLevel enum + classify_signature() + sig_has_padding()
├── classify.hpp                # classify<T> (compile-time, wraps layout_traits) — requires P2996
├── serialization_free.hpp      # is_local_serialization_free<T>, SignatureRegistry — requires P2996
├── sig_export.hpp              # SigExporter, TYPELAYOUT_EXPORT_TYPES — requires P2996
├── sig_types.hpp               # TypeEntry, PlatformInfo structs (shared data types)
├── compat_check.hpp            # CompatReporter (compares .sig.hpp across platforms)
├── platform_detect.hpp         # Arch/OS/compiler detection macros
└── detail/foreach.hpp          # Variadic macro helper
```

## Key Design Concepts

- **FixedString<N>**: N = exact content length (no wasted capacity). All signatures are FixedString.
- **Flattening**: Structs are recursively flattened — field names and inheritance erased, only byte identity preserved.
- **Safety levels** (ordered worst→best): `Opaque > PlatformVariant > PointerRisk > PaddingRisk > TrivialSafe`
- **Dual-path padding detection**: `compute_has_padding` (compile-time bitmap via P2996) cross-validated against `sig_has_padding` (runtime string parser), enforced by `static_assert`.
- **Opaque types**: Unanalyzable types registered with macros. `O!tag[s:N,a:A]` (legacy) or `O(Tag|N|A)` (new).
- **Array element recursion**: `type_has_opaque` and `compute_has_padding` recurse into array element types via `std::remove_all_extents_t`.

## Code Map — Key Entry Points

| Task | Start here |
|------|-----------|
| How signatures are generated | `detail/signature_impl.hpp` → `TypeSignature<T>::calculate()` |
| How types are classified | `detail/reflect.hpp` → `classify_type()`, `detail/type_map.hpp` |
| How layout_traits works | `layout_traits.hpp` → `layout_traits<T>` struct |
| How padding is detected | `layout_traits.hpp` → `compute_has_padding<T>()` (bitmap) |
| How safety is classified | `tools/classify.hpp` → `classify<T>`, `tools/safety_level.hpp` → `classify_signature()` |
| How opaque types work | `opaque.hpp` → macros + `has_opaque_signature` concept |
| Cross-platform pipeline | `tools/sig_export.hpp` (Phase 1) → `tools/compat_check.hpp` (Phase 2) |

## Tests

10 tests total (5 core + 5 tools). All use `static_assert` for compile-time + runtime verification.

| Test | Layer | What it validates |
|------|-------|-------------------|
| `test_fixed_string.cpp` | Core | FixedString ops, to_fixed_string |
| `test_opaque.cpp` | Core | Opaque registration, CV-qualified, base class, EBO |
| `test_layout_traits.cpp` | Core | Signature consistency, has_pointer/opaque/padding, field_count |
| `test_empty_member_probe.cpp` | Core | Empty member/NUA/EBO signature correctness |
| `test_padding_precision.cpp` | Core | Byte-coverage bitmap vs sig parser, classify consistency |
| `test_classify.cpp` | Tools | Five-tier classify<T> for all type categories |
| `test_serialization_free.cpp` | Tools | is_local_serialization_free, SignatureRegistry |
| `test_sig_export.cpp` | Tools | SigExporter output structure |
| `test_rt_padding.cpp` | Tools | Runtime sig_has_padding (C++17 only, no P2996) |
| `test_compat_check.cpp` | Tools | CompatReporter, classify_signature (C++17 only) |
