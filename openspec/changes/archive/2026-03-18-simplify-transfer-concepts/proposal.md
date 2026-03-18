## Why

TypeLayout's core value is answering "can type T be safely byte-copied between endpoints?"
(`is_byte_copy_safe_v<T>`). Around this core, the library has accumulated multiple overlapping
concepts (see `docs/concept-analysis.md` for full inventory):

**Derivable / redundant concepts:**
- `is_local_serialization_free_v<T>` -- trivially_copyable + no pointer (derivable)
- `serialization_free_assert<T>` -- diagnostic wrapper for the above (derivable)
- `is_trivial_safe_v<T>` -- alias for `classify_v<T> == TrivialSafe` (derivable)
- `is_memcpy_safe_v<T>` -- already marked deprecated
- `signature_compare<T,U>` -- duplicate of `layout_signatures_match<T1,T2>()`

**Naming collision:**
- `is_layout_compatible_v<T>` -- name collides with `std::is_layout_compatible<T,U>` (C++20)
  with completely different semantics (unary transport check vs binary CIS aliasing check).
  This will confuse every user familiar with the C++20 standard.

**Terminology inconsistency:**
- `SignatureRegistry::is_serialization_free()` / `CompatReporter::are_serialization_free()`
  use "serialization-free" but check "transfer-safe" semantics. The term "serialization-free"
  is used with three different meanings across the codebase.

Users face ~10 overlapping concepts for a handful of core questions. Six of those concepts
are derivable, duplicated, deprecated, or dangerously named.

## What Changes

**DELETE (6 concepts):**
1. `is_local_serialization_free<T>` / `is_local_serialization_free_v<T>` --
   equivalent to `std::is_trivially_copyable_v<T> && is_byte_copy_safe_v<T>`.
2. `serialization_free_assert<T>` --
   replaced by `static_assert(is_byte_copy_safe_v<T>, ...)`.
3. `is_trivial_safe_v<T>` --
   alias for `classify_v<T> == SafetyLevel::TrivialSafe`; users can check traits directly.
4. `is_layout_compatible_v<T>` --
   naming collision with `std::is_layout_compatible` (C++20); also derivable.
5. `is_memcpy_safe_v<T>` --
   already marked deprecated; alias of `is_layout_compatible_v`.
6. `signature_compare<T,U>` / `signature_compare_v<T,U>` --
   exact duplicate of `layout_signatures_match<T1,T2>()`.

**RENAME (3 symbols + 1 file + 1 test):**
7. `serialization_free.hpp` -> `transfer.hpp`
8. `SignatureRegistry::is_serialization_free()` -> `SignatureRegistry::is_transfer_safe()`
9. `CompatReporter::are_serialization_free()` -> `CompatReporter::are_transfer_safe()`
10. `test_serialization_free.cpp` -> `test_transfer.cpp`

**UPDATE:**
11. CompatReporter report output text: unify terminology to "transfer-safe".
12. All documentation, rules, AGENTS.md, paper sections.

## Capabilities

### Removed Capabilities
- `is_local_serialization_free<T>` / `is_local_serialization_free_v<T>`
- `serialization_free_assert<T>`
- `is_trivial_safe_v<T>`
- `is_layout_compatible_v<T>`
- `is_memcpy_safe_v<T>`
- `signature_compare<T,U>` / `signature_compare_v<T,U>`

### Modified Capabilities
- `SignatureRegistry::is_serialization_free()` renamed to `is_transfer_safe()`
- `CompatReporter::are_serialization_free()` renamed to `are_transfer_safe()`
- `serialization_free.hpp` renamed to `transfer.hpp`
- `test_serialization_free.cpp` renamed to `test_transfer.cpp`

## Impact

- **Breaking API change**: 6 public symbols removed. See Migration Guide in design.md.
- **Breaking API change**: Users calling `SignatureRegistry::is_serialization_free()` or
  `CompatReporter::are_serialization_free()` must update call sites.
- **Breaking include path**: `<boost/typelayout/tools/serialization_free.hpp>` becomes
  `<boost/typelayout/tools/transfer.hpp>`.
- **Breaking**: `signature_compare<T,U>` removed; use `layout_signatures_match<T1,T2>()`.
- **Naming collision fix**: `is_layout_compatible_v<T>` removed, eliminating confusion
  with `std::is_layout_compatible<T,U>` (C++20).
- **Test rename**: `test_serialization_free` target becomes `test_transfer`.
- **No behavioral changes**: All runtime/compile-time logic is preserved.
  SafetyLevel, classify<T>, layout_traits<T> remain unchanged (reserved for Step 2).
