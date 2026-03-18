## Context

### Problem: Concept Proliferation

TypeLayout currently exposes 5 different predicates that answer variants of "can I byte-copy
this type?":

```
Layer       Concept                              Checks
------      ------                               ------
admission   is_byte_copy_safe_v<T>               recursive member check + opaque
s11n_free   is_local_serialization_free_v<T>     trivially_copyable + !has_pointer
s11n_free   serialization_free_assert<T>         same, with diagnostic static_asserts
s11n_free   is_transfer_safe<T>(sig)             byte_copy_safe + signature match
compat      are_serialization_free(types, plats) cross-platform batch version
```

The mathematical relationship is:

```
is_local_serialization_free_v<T>
    <==>  std::is_trivially_copyable_v<T> && is_byte_copy_safe_v<T>

is_transfer_safe<T>(remote_sig)
    <==>  is_byte_copy_safe_v<T> && (local_sig == remote_sig)

are_serialization_free(types, platforms)
    <==>  for_all(t in types, p in platforms): sig_match(t,p) && !has_pointer(t)
```

`is_local_serialization_free` is a strict subset of `is_byte_copy_safe` intersected with
`std::is_trivially_copyable`. It provides no information that the user cannot obtain from
combining two existing predicates.

### Problem: Terminology Inconsistency

The term "serialization-free" appears in:
- `is_local_serialization_free` (predicate)
- `serialization_free_assert` (diagnostic)
- `SignatureRegistry::is_serialization_free()` (method)
- `CompatReporter::are_serialization_free()` (method)
- CompatReporter report output: "Serialization-free" verdict
- File name: `serialization_free.hpp`
- Test name: `test_serialization_free.cpp`

But the actual semantics of these vary:
- `is_local_serialization_free` checks trivially_copyable + no pointer (strict)
- `SignatureRegistry::is_serialization_free` checks signature match only (lenient)
- `CompatReporter::are_serialization_free` checks sig match + no PointerRisk (medium)

The term "transfer-safe" also appears in CompatReporter output as a distinct verdict,
further confusing the taxonomy.

### Desired End State

A clean three-concept model:

```
                        User Question                          TypeLayout Answer
                        =============                          ================

  "Is T safe for byte-level transport?"              is_byte_copy_safe_v<T>
                                                     (compile-time, recursive, P2996)

  "Does T have the same layout on both ends?"        is_transfer_safe<T>(remote_sig)
                                                     = byte_copy_safe + sig_match

  "What risks should I know about?"                  SafetyLevel / classify<T>
                                                     (TrivialSafe / PaddingRisk /
                                                      PlatformVariant / PointerRisk /
                                                      Opaque)
```

One predicate to admit, one to compare, one to classify. No aliases, no derived helpers.

## Goals / Non-Goals

**Goals:**
- Remove `is_local_serialization_free` and `serialization_free_assert` from public API
- Unify all "serialization-free" terminology to "transfer-safe"
- Rename `serialization_free.hpp` to `transfer.hpp`
- Rename all public methods containing "serialization_free" to "transfer_safe"
- Update CompatReporter report output to use consistent terminology
- Rename `test_serialization_free.cpp` to `test_transfer.cpp`
- Update all documentation and rule files

**Non-Goals:**
- Changing `is_byte_copy_safe` logic or behavior
- Changing `is_transfer_safe` logic or behavior
- Changing `SafetyLevel` enum or `classify<T>` behavior
- Changing `SignatureRegistry` behavior (only rename one method)
- Changing `CompatReporter` behavior (only rename one method + update labels)
- Removing `SignatureRegistry` class
- Changing signature format

## Decisions

### D1: Delete is_local_serialization_free entirely (not deprecate)

**Decision**: Remove the struct, the variable template, and the assert wrapper.
Do not provide a deprecated alias.

**Rationale**: This is a pre-1.0 library with no external users yet. Deprecation cycles
are unnecessary. The replacement (`std::is_trivially_copyable_v<T> && is_byte_copy_safe_v<T>`)
is trivial and well-documented.

**Migration path for hypothetical users**:
```cpp
// Before:
static_assert(is_local_serialization_free_v<T>);

// After:
static_assert(std::is_trivially_copyable_v<T> && is_byte_copy_safe_v<T>);
```

### D2: Rename serialization_free.hpp to transfer.hpp

**Decision**: Rename the file. Update the header guard to `BOOST_TYPELAYOUT_TOOLS_TRANSFER_HPP`.

**Rationale**: The file's remaining contents (`is_transfer_safe`, `SignatureRegistry`) are
about transfer safety, not serialization freedom. The name should reflect content.

**Impact**: All `#include <boost/typelayout/tools/serialization_free.hpp>` must change.
Affected files:
- `include/boost/typelayout/typelayout.hpp` (umbrella header)
- `test/test_transfer.cpp` (renamed test)
- `test/test_byte_copy_safe.cpp` (if it includes this)
- `docs/api-reference.md`, `docs/quickstart.md`, `docs/applications.md`

### D3: Rename SignatureRegistry::is_serialization_free to is_transfer_safe

**Decision**: Rename the method. Both overloads (string_view key and template<T>).

**Rationale**: The method checks whether two endpoints' signatures match for a given type.
This is "transfer safety", not "serialization freedom". The current name was chosen before
`is_byte_copy_safe` existed and reflects an outdated mental model.

**Code change**:
```cpp
// Before:
[[nodiscard]] bool is_serialization_free(std::string_view key) const;
template <typename T>
[[nodiscard]] bool is_serialization_free() const;

// After:
[[nodiscard]] bool is_transfer_safe(std::string_view key) const;
template <typename T>
[[nodiscard]] bool is_transfer_safe() const;
```

### D4: Rename CompatReporter::are_serialization_free to are_transfer_safe

**Decision**: Rename the method. Both overloads (initializer_list and vector).

**Code change**:
```cpp
// Before:
[[nodiscard]] bool are_serialization_free(
    std::initializer_list<std::string_view> type_names,
    std::initializer_list<std::string_view> platform_names) const;

// After:
[[nodiscard]] bool are_transfer_safe(
    std::initializer_list<std::string_view> type_names,
    std::initializer_list<std::string_view> platform_names) const;
```

### D5: Update CompatReporter report output terminology

**Decision**: Change report text to use consistent "transfer-safe" language.

**Changes in format_verdict()**:
```
Before                                          After
------                                          -----
"Serialization-free"                            "Transfer-safe"
"Transfer-safe (padding may leak...)"           "Transfer-safe (padding may leak...)"
"Transfer-safe (contains opaque...)"            "Transfer-safe (opaque, verify manually)"
"Layout match (pointer values not portable)"    (unchanged)
"Needs serialization"                           "Layout mismatch"
```

**Changes in summary section**:
```
Before                                          After
------                                          -----
"Serialization-free (strict): N/M"              (removed -- no longer a distinct tier)
"Transfer-safe: N/M"                            "Transfer-safe: N/M"
"Needs serialization: N/M"                      "Layout mismatch: N/M"
```

**Rationale**: "Serialization-free" was a separate tier from "Transfer-safe" in the old model
because `is_local_serialization_free` was stricter than `is_byte_copy_safe`. With the concept
removed, there's no reason to distinguish -- both map to "transfer-safe".

The old three-tier report ("Serialization-free" / "Transfer-safe" / "Needs serialization")
becomes a two-tier report ("Transfer-safe" / "Layout mismatch"), with risk modifiers in
parentheses for types that are transfer-safe but have caveats.

### D6: Rename test file

**Decision**: `test/test_serialization_free.cpp` -> `test/test_transfer.cpp`.
CMake target: `test_serialization_free` -> `test_transfer`.

**Rationale**: File name should match the header it tests.

### D7: Keep SigExporter::add() static_assert as is_trivially_copyable

**Decision**: `SigExporter::add<T>()` retains `static_assert(std::is_trivially_copyable_v<T>)`.

**Rationale**: `SigExporter` exports signatures for cross-platform comparison. Types with
pointers (e.g. `UnsafeStruct`) are trivially copyable and have valid signatures -- the
export step is about signature collection, not safety admission. Safety classification
happens at the CompatReporter report layer. Changing to `is_byte_copy_safe_v` would reject
types with pointers from export, breaking the cross-platform comparison workflow that needs
to report pointer-risk types (not silently exclude them).

Non-trivially-copyable relocatable opaque types can use `add_relocatable<T>()` instead.

### D8: Update admission.hpp header comment

**Decision**: Remove the comparison with `is_local_serialization_free` from the header
comment block. Replace with a simpler explanation of `is_byte_copy_safe` as the primary
admission predicate.

### D9: Delete is_trivial_safe_v, is_layout_compatible_v, is_memcpy_safe_v

**Decision**: Remove all three convenience aliases from `classify.hpp`.

**Rationale**:
- `is_trivial_safe_v<T>` is `classify_v<T> == SafetyLevel::TrivialSafe` -- a one-line
  expression. Users who need this can write it directly, or more informatively check
  individual `layout_traits<T>` booleans.
- `is_layout_compatible_v<T>` collides with `std::is_layout_compatible<T,U>` (C++20).
  The standard version is a binary trait testing union CIS aliasing legality; this version
  is a unary trait testing transport safety. **Completely different semantics, identical name.**
  This is an API design bug that must be fixed before any user adopts it.
- `is_memcpy_safe_v<T>` is already marked deprecated. It aliases `is_layout_compatible_v`.

**Migration**:
```cpp
// Before:                          After:
is_trivial_safe_v<T>                classify_v<T> == SafetyLevel::TrivialSafe
is_layout_compatible_v<T>           (remove; check layout_traits<T> booleans directly)
is_memcpy_safe_v<T>                 (remove; already deprecated)
```

Note: SafetyLevel and classify<T> themselves are kept in this change. Their removal is
a separate, larger refactoring (Step 2) that requires redesigning CompatReporter's report
format.

### D10: Delete signature_compare<T,U>

**Decision**: Remove `signature_compare<T,U>` struct and `signature_compare_v<T,U>` variable
template from `layout_traits.hpp`.

**Rationale**: Exact duplicate of `layout_signatures_match<T1,T2>()` in `signature.hpp`.
Two public symbols for the same operation is confusing. `layout_signatures_match` is the
canonical form (used in docs and examples), so `signature_compare` goes.

**Migration**:
```cpp
// Before:                                  After:
signature_compare_v<T, U>                   layout_signatures_match<T, U>()
signature_compare<T, U>::value              layout_signatures_match<T, U>()
```

**Impact on tests**: `test_layout_traits.cpp` Part 6 ("signature_compare") must be rewritten
to use `layout_signatures_match<T,U>()`.

## Risks / Trade-offs

- **[Risk] Breaking API changes**: `is_local_serialization_free`, `serialization_free_assert`,
  method renames, file rename. Mitigation: pre-1.0 library, no external users.
- **[Risk] CompatReporter report format change**: The three-tier report becomes two-tier.
  Mitigation: No known CI pipelines parsing this output. It's human-readable.
- **[Risk] test_compat_check.cpp has hardcoded report expectations**: The report format
  change will break string comparisons. Mitigation: Update expected strings in test.
- **[Trade-off] Losing diagnostic specificity**: `serialization_free_assert` gave two separate
  error messages ("not trivially_copyable" vs "has pointer"). With `static_assert(is_byte_copy_safe_v<T>)`,
  the user gets a single yes/no. Mitigation: Acceptable for now; can add a diagnostic helper later
  if users request it.

## Concept Relationship After Change

```
                        ┌──────────────────────┐
                        │ SafetyLevel          │ "What risks exist?"
                        │ (classify<T>)        │ Orthogonal dimension
                        └──────────┬───────────┘
                                   │
    ┌──────────────────────────────┼──────────────────────────────┐
    │                              │                              │
    ▼                              ▼                              ▼
┌──────────────┐      ┌──────────────────┐      ┌──────────────────────┐
│byte_copy_safe│      │ is_transfer_safe │      │ layout_signatures    │
│   <T>        │─────▶│ <T>(remote_sig)  │◀─────│ _match<T,U>()        │
│              │      │                  │      │                      │
│ "T's bytes   │      │ "byte_copy_safe  │      │ "T and U have        │
│  are safe    │      │  + sig match"    │      │  identical layout"   │
│  to copy"    │      │                  │      │                      │
└──────────────┘      └──────────────────┘      └──────────────────────┘
    Core 1                  Core 2                     Core 3
    (admission)             (endpoint check)           (type comparison)

                    ┌──────────────────────┐
                    │ SignatureRegistry    │  Runtime registry wrapper
                    │ .is_transfer_safe() │  around Core 2
                    └──────────────────────┘

                    ┌──────────────────────┐
                    │ CompatReporter       │  Cross-platform batch
                    │ .are_transfer_safe() │  report using Core 1-3
                    └──────────────────────┘

DELETED:
  is_local_serialization_free   (was: trivially_copyable && byte_copy_safe)
  serialization_free_assert     (was: diagnostic wrapper)
  is_trivial_safe_v             (was: classify_v == TrivialSafe)
  is_layout_compatible_v        (was: classify_v <= PaddingRisk; C++20 naming collision)
  is_memcpy_safe_v              (was: deprecated alias)
  signature_compare<T,U>        (was: duplicate of layout_signatures_match)
```

## File-Level Impact Map

### Headers Modified

| File | Change |
|------|--------|
| `tools/serialization_free.hpp` | **RENAME** to `tools/transfer.hpp`; delete `is_local_serialization_free`, `serialization_free_assert`; rename `SignatureRegistry::is_serialization_free` -> `is_transfer_safe` |
| `tools/compat_check.hpp` | Rename `are_serialization_free` -> `are_transfer_safe`; update report text in `format_verdict()` and `print_report_impl()` |
| `tools/sig_export.hpp` | Change `static_assert(is_trivially_copyable_v<T>)` to `static_assert(is_byte_copy_safe_v<T>)` in `add<T>()` |
| `tools/classify.hpp` | Delete `is_trivial_safe_v`, `is_layout_compatible_v`, `is_memcpy_safe_v` |
| `layout_traits.hpp` | Delete `signature_compare<T,U>` struct and `signature_compare_v<T,U>` |
| `admission.hpp` | Update header comment (remove `is_local_serialization_free` comparison) |
| `typelayout.hpp` | Update `#include` path from `serialization_free.hpp` to `transfer.hpp` |

### Tests Modified

| File | Change |
|------|--------|
| `test/test_serialization_free.cpp` | **RENAME** to `test/test_transfer.cpp`; remove `is_local_serialization_free` assertions; rename `is_serialization_free` calls to `is_transfer_safe` |
| `test/test_byte_copy_safe.cpp` | Remove `is_local_serialization_free` references (subsumption tests at line 240-247) |
| `test/test_layout_traits.cpp` | Rewrite Part 6 ("signature_compare") to use `layout_signatures_match`; remove `signature_compare` references |
| `test/test_classify.cpp` | Remove `is_trivial_safe_v` and `is_layout_compatible_v` static_asserts |
| `test/test_compat_check.cpp` | Rename `are_serialization_free` calls to `are_transfer_safe`; update expected report strings |

### Build System

| File | Change |
|------|--------|
| `CMakeLists.txt` | Rename `test_serialization_free` target to `test_transfer`; update source file path |

### Documentation

| File | Change |
|------|--------|
| `AGENTS.md` | Update test table: rename entry; update Code Map; remove `is_local_serialization_free` references |
| `.codemaker/rules/rules.mdc` | Update API signatures section; update test catalog; update Code Map |
| `docs/api-reference.md` | Rewrite "Tools: serialization_free" section to "Tools: transfer"; remove `is_local_serialization_free` docs; rename method docs |
| `docs/quickstart.md` | Update section 9 title and code examples |
| `docs/applications.md` | Update `#include` paths and function calls in examples |
| `docs/paper/sec5-toolchain.md` | Update terminology |
| `docs/conference-proposal.md` | Update `is_transfer_safe` reference (already correct name) |
| `example/README.md` | Update if references serialization_free |

### Files NOT Changed

| File | Reason |
|------|--------|
| `admission.hpp` (logic) | `is_byte_copy_safe` implementation unchanged |
| `layout_traits.hpp` (except signature_compare removal) | Core logic unchanged |
| `signature.hpp` | No serialization_free references |
| `opaque.hpp` | No serialization_free references |
| `fixed_string.hpp` | Infrastructure, unchanged |
| `detail/sig_parser.hpp` | No serialization_free references |
| `tools/safety_level.hpp` | SafetyLevel enum unchanged |
| `tools/classify.hpp` (classify<T>/classify_v<T>) | Core classification logic unchanged (Step 2) |
| `tools/sig_types.hpp` | Data structs unchanged |
| `tools/platform_detect.hpp` | Platform macros unchanged |
