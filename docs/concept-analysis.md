# TypeLayout Concept Analysis

Analysis of the library's concept structure after all simplification rounds.

---

## 1. Concept Structure

The public-facing mental model is smallest when organized into 3 layers:

1. **Core questions** -- `get_layout_signature<T>()`, `is_byte_copy_safe_v<T>`,
   `is_transfer_safe<T>(sig)`
2. **Extension mechanisms** -- opaque registration plus the cross-platform export/report
   pipeline
3. **Internal support** -- `detail::layout_traits<T>`, padding cross-checks, signature
   parsing, and report-only safety classification

Under the hood, the library still has two technical roots: **signature generation**
and **P2996 reflection**. They converge in `detail::layout_traits<T>`, which feeds the
transport safety layer.

```
P2996 Reflection Engine
  |
  +--- Signature Generation -----> get_layout_signature<T>()
  |                                    |
  |                              layout_traits<T>  [detail::]
  |                              /        |        \
  |                       has_pointer  has_padding  has_opaque [detail only]
  |                            |          |
  |                            v          v
  |                    is_byte_copy_safe_v<T>  (mixed: signature + P2996)
  |                            |
  |                            v
  |                    is_transfer_safe<T>(sig)  (byte_copy_safe + sig ==)
  |
  +--- Opaque Registration (4 macros)
  |
  +--- Tools: SigExporter, CompatReporter, classify_signature
```

### Data source classification

| Concept | Source |
|---------|--------|
| `get_layout_signature<T>()` | P2996 reflection -> string encoding |
| `has_pointer` | Signature token scan (non-opaque) / user declaration (opaque) |
| `has_padding` | **Independent** P2996 byte-coverage bitmap (not from signature) |
| `has_opaque` | **Independent** P2996 recursive traversal (not from signature) |
| `is_byte_copy_safe_v<T>` | Mixed: reads `has_pointer` (signature) + P2996 member recursion |
| `is_transfer_safe<T>(sig)` | `is_byte_copy_safe` + signature string comparison |
| `classify_signature()` | Signature token scan |
| `sig_has_padding()` | Signature offset/size parsing |

---

## 2. Current Public Concept Inventory

After the simplification rounds, the library is easiest to explain as 3 public-facing
questions plus extension mechanisms.

```
Core questions
  get_layout_signature<T>()               signature.hpp
  is_byte_copy_safe_v<T>                  admission.hpp
  is_transfer_safe<T>(remote_sig)         transfer.hpp

Extension mechanisms
  TYPELAYOUT_REGISTER_OPAQUE              opaque.hpp
  TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE      opaque.hpp
  TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE opaque.hpp
  TYPELAYOUT_OPAQUE_MAP_RELOCATABLE       opaque.hpp
  SigExporter                             sig_export.hpp
  CompatReporter                          compat_check.hpp

Internal support (not stable user-facing concepts)
  detail::layout_traits<T>                layout_traits.hpp
  detail::SafetyLevel                     tools/safety_level.hpp
  detail::classify_signature()            tools/safety_level.hpp
  FixedString<N>                          fixed_string.hpp
  get_arch_prefix()                       signature.hpp  [detail::]
  sig_has_padding(string_view)            sig_parser.hpp  [detail::]
  opaque_elements_safe<T>                 fwd.hpp  [detail::]
```

This framing keeps the user-visible surface small without changing the underlying
architecture.

---

## 3. Simplification History

### Round 1: Remove redundant alias traits

| Removed | Reason |
|---------|--------|
| `serialization_free.hpp` (entire file) | `is_local_serialization_free` was `trivially_copyable && !has_pointer` -- trivial `&&` |
| `signature_compare<T,U>` | Duplicate of direct `==` comparison |
| `is_trivial_safe_v` | Alias for `classify_v == TrivialSafe` |
| `is_layout_compatible_v` | Naming collision with `std::is_layout_compatible` (C++20) |
| `is_memcpy_safe_v` | Deprecated alias |
| `serialization_free_assert<T>` | Two `static_assert` lines, no logic |

### Round 2: Remove classify system + platform traits

| Removed | Reason |
|---------|--------|
| `classify.hpp` (entire file) | `classify<T>` compressed orthogonal booleans into lossy scalar |
| `has_bit_field` | Only consumed by deleted `classify<T>` |
| `is_platform_variant` | Lint-level warning, not decision-enabling |
| 4 alias macros in `opaque.hpp` | Zero external usage |
| `SafetyLevel` moved to `detail::` | Display-only, consumed by `CompatReporter` internally |
| `classify_signature` moved to `detail::` | Same |

### Round 3: Remove field_count, clarify has_opaque scope

| Change | Reason |
|--------|--------|
| `field_count` removed from `layout_traits` | No core consumer; `sizeof`/`alignof` level metadata with no decision value |
| `has_opaque` recognized as detail-only | Already in `detail::layout_traits`; only consumed by internal cross-validation `static_assert` |

### Round 4: Remove layout_signatures_match

| Change | Reason |
|--------|--------|
| `layout_signatures_match<T1,T2>()` removed | One-line `==` wrapper with no added value. Users write `get_layout_signature<A>() == get_layout_signature<B>()` directly. `FixedString` already supports `operator==`. |

---

## 4. Why Each Remaining Concept is Necessary

### Core questions (cannot remove any)

| Concept | Why it cannot be removed |
|---------|--------------------------|
| `get_layout_signature<T>()` | Foundation of the library. All downstream features consume the layout signature. Signature comparison uses `FixedString::operator==` directly. |
| `is_byte_copy_safe_v<T>` | Core byte-copy transport predicate. Its recursive decision tree cannot be reduced to signature equality alone. |
| `is_transfer_safe<T>(sig)` | Direct answer to the library's core runtime question: can local `T` be safely transferred to this remote endpoint? |

### Extension mechanisms: opaque registration (cannot reduce below 4)

| Macro | Why it is distinct |
|-------|-------------------|
| `REGISTER_OPAQUE` | Trivially copyable concrete type. Has `static_assert`. |
| `OPAQUE_TYPE_RELOCATABLE` | Non-trivially-copyable concrete type (e.g. offset_ptr wrappers). No `static_assert`. Different safety semantics. |
| `OPAQUE_CONTAINER_RELOCATABLE` | Single-parameter template. Embeds element type signature. Generates `opaque_elements_safe` specialization that recurses into element. |
| `OPAQUE_MAP_RELOCATABLE` | Two-parameter template. Cannot be expressed by single-parameter variant. |

### Extension mechanisms: tools (display + pipeline)

| Concept | Why it is kept |
|---------|---------------|
| `SafetyLevel` / `classify_signature` | Consumed by `CompatReporter::compare()` for report output. Kept in `detail::` namespace to signal "not for programmatic decisions". |
| `SigExporter` | Phase 1 of cross-platform pipeline. No alternative. |
| `CompatReporter` | Phase 2 of cross-platform pipeline. No alternative. |

### Internal support (not public, but technically necessary)

| Concept | Role |
|---------|------|
| `has_opaque` | Cross-validation `static_assert` skip condition in `layout_traits`. Not exposed as user API. |
| `total_size` / `alignment` | `sizeof`/`alignof` wrappers in `detail::layout_traits`. Zero concept cost. |
| `sig_has_padding` | Second path of dual-path padding validation. |
| `opaque_elements_safe<T>` | Recursion termination point for `is_byte_copy_safe` on opaque types. |

---

## 5. Deletability Analysis

Can any remaining concept be removed without breaking the system?

### Not deletable (hard dependency chain)

| Concept | Critical consumer |
|---------|-------------------|
| `get_layout_signature<T>()` | Foundation; all downstream concepts consume the signature |
| `layout_traits<T>` | `admission.hpp` reads `has_pointer`; `sig_export.hpp` reads `has_pointer` |
| `is_byte_copy_safe_v<T>` | `is_transfer_safe`; opaque macros' `opaque_elements_safe` |
| `REGISTER_OPAQUE` (4 macros) | Only mechanism for unanalyzable types; each variant handles a distinct shape |
| `SigExporter` | Phase 1 of cross-platform pipeline; no alternative |
| `CompatReporter` | Phase 2 of cross-platform pipeline; no alternative |
| `sig_has_padding()` | Dual-path cross-validation `static_assert` in `layout_traits` |

### Technically deletable but not recommended

| Concept | Could replace with | Why keep |
|---------|-------------------|----------|
| `SafetyLevel` / `classify_signature()` | Inline `sig_contains_token("ptr[")` in `check_transfer_safe` | `CompatReporter::print_report()` would lose the 5-tier classification display (`***`, `**-`, `*!-`, `*--`, `---`). Report degrades to binary MATCH/DIFFER with no explanation of *why* a match is still risky. |
| `is_transfer_safe<T>(sig)` | Users write `is_byte_copy_safe_v<T> && sig == get_layout_signature<T>()` | 3-line wrapper, zero maintenance cost. Gives the library's core question a named API. |

### Conclusion

No core question is redundant. The only candidates for deletion (`detail::SafetyLevel`,
`is_transfer_safe`) have near-zero maintenance cost and provide clear explanatory or
user-facing value. Further simplification would degrade usability without reducing
complexity.

---

## 6. Concept Usage in Applications

The core concepts serve a two-phase cross-platform workflow. See
`docs/applications.md` for full code examples covering IPC, network
protocols, plugin ABI, and cross-platform binary files.

```
Phase 1 (compile-time, P2996)         Phase 2 (runtime, C++17)
─────────────────────────             ──────────────────────────
get_layout_signature<T>()  ────┐
is_byte_copy_safe_v<T>    ────┤      sig_match() / operator==
REGISTER_OPAQUE            ────┤      classify_signature() → SafetyLevel
                               │      CompatReporter::are_transfer_safe()
                               ▼
                          SigExporter ──→ .sig.hpp ──→ CompatReporter
```

| Concept | Phase 1 role | Phase 2 role |
|---------|-------------|-------------|
| `get_layout_signature<T>()` | SigExporter calls to generate signature | -- (serialized as `const char*`) |
| `is_byte_copy_safe_v<T>` | SigExporter::add `static_assert` gate | -- |
| `REGISTER_OPAQUE` | User registers before Phase 1 export | -- |
| `SafetyLevel` | -- | `classify_signature()` classifies each sig |
| `is_transfer_safe<T>(sig)` | -- | Runtime single-endpoint check |
| `SigExporter` | Generates `.sig.hpp` per platform | -- |
| `CompatReporter` | -- | Loads `.sig.hpp` files, generates report |
