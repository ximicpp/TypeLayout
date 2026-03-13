## Project Rules

- **No emoji in code or documentation.** All source files, comments, commit messages, Markdown documents, and spec files must use plain text only. Use text markers like `[FIXED]`, `[TODO]`, `(!)` instead of emoji.
- **consteval, not constexpr** for all signature-generating functions. `constexpr` is only for helper utilities.
- **FixedString<N>** with N = exact content length. No slack, no over-allocation.
- **Header-only library** -- no .cpp source files in the library itself. Tests are .cpp files.

## Code Conventions

- Namespace: `boost::typelayout`, detail code in `boost::typelayout::detail`
- Header guards: `#ifndef BOOST_TYPELAYOUT_<FILENAME>_HPP` / `#define` / `#endif`
- No copyright header required in test files
- Include order: project headers, then standard library, then test utilities

## Architecture Invariants

- **Two layers**: Core (requires P2996) and Tools (mostly C++17). Tools layer must not `#include` core headers unless the tool file is explicitly marked as P2996-required.
- **Dual-path padding detection**: `compute_has_padding` (compile-time bitmap in `layout_traits.hpp`) must be cross-validated against `sig_has_padding` (runtime string parser in `detail/sig_parser.hpp`). Both must agree or `static_assert` fires.
- **Virtual inheritance rejection**: `signature_impl.hpp` rejects types with virtual bases at compile time.
- **Opaque ordering**: Opaque types must be registered BEFORE any signature generation that encounters them.
- **Flattening**: Struct field names and inheritance hierarchy are erased. Only byte-level identity is preserved in signatures.

## Testing Invariants

- **static_assert preferred**: Every compile-time property should be validated with `static_assert` first, then optionally printed at runtime for debugging.
- **Inline test types**: Test structs/classes are defined inside the test .cpp file, not in shared headers.
- **Timeout values**: P2996 tests get 120s timeout; C++17-only tests get 30s timeout.
- **P2996 core tests**: Use `LABELS "core"`, link to `typelayout` interface library (provides compile flags automatically)
- **P2996 tools tests**: Use `LABELS "tools"`, link to `typelayout` interface library
- **C++17-only tests**: Use `LABELS "tools"`, set `-std=c++17 -stdlib=libc++` manually, do NOT link `typelayout`

## Signature Format Rules

- **Arch prefix**: `[BITS-ENDIAN]` e.g. `[64-le]`, `[32-be]`
- **Record format**: `record[s:SIZE,a:ALIGN]{@OFFSET:TYPE[s:SIZE,a:ALIGN],...}`
- **Padding encoding**: `pad:N` for N padding bytes at a given offset
- **Opaque encoding**: `O(Tag|N|A)` via `TYPELAYOUT_REGISTER_OPAQUE`
- **Array encoding**: `TYPE[N]` for fixed-size arrays, element type is recursed into

## Code Map -- Key Entry Points

| Task | Start here |
|------|-----------|
| How signatures are generated | `detail/signature_impl.hpp` -> `TypeSignature<T>::calculate()` |
| How types are classified | `detail/reflect.hpp` -> `classify_type()`, `detail/type_map.hpp` |
| How layout_traits works | `layout_traits.hpp` -> `layout_traits<T>` struct |
| How padding is detected | `layout_traits.hpp` -> `compute_has_padding<T>()` (bitmap) |
| How safety is classified | `tools/classify.hpp` -> `classify<T>`, `tools/safety_level.hpp` -> `classify_signature()` |
| How opaque types work | `opaque.hpp` -> macros + `has_opaque_signature` concept |
| Cross-platform pipeline | `tools/sig_export.hpp` (Phase 1) -> `tools/compat_check.hpp` (Phase 2) |
| Serialization-free query | `tools/compat_check.hpp` -> `CompatReporter::are_serialization_free(types, platforms)` |
| Data model / ABI detect | `tools/platform_detect.hpp` -> `get_data_model()` |

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
| `test_compat_check.cpp` | Tools | CompatReporter, classify_signature, are_serialization_free, ABI equivalence (C++17 only) |
