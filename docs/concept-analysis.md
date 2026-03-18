# TypeLayout Concept Analysis

Analysis of the library's concept structure after all simplification rounds.

---

## 1. Concept Structure

The library has two roots: **signature generation** and **P2996 reflection**.
They converge in `layout_traits<T>`, which feeds the transport safety layer.

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

After three rounds of simplification (25 -> 21 -> 19 concepts):

```
Core Layer (7 concepts)
  get_layout_signature<T>()               signature.hpp
  layout_signatures_match<T1, T2>()       signature.hpp
  layout_traits<T>::has_pointer           layout_traits.hpp  [detail::]
  layout_traits<T>::has_padding           layout_traits.hpp  [detail::]
  is_byte_copy_safe_v<T>                  admission.hpp
  is_transfer_safe<T>(remote_sig)         transfer.hpp

Opaque Registration (4 macros)
  TYPELAYOUT_REGISTER_OPAQUE              opaque.hpp
  TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE      opaque.hpp
  TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE opaque.hpp
  TYPELAYOUT_OPAQUE_MAP_RELOCATABLE       opaque.hpp

Tools Layer (display + cross-platform pipeline)
  detail::SafetyLevel                     safety_level.hpp
  detail::classify_signature()            safety_level.hpp
  SigExporter                             sig_export.hpp
  CompatReporter                          compat_check.hpp

Infrastructure
  FixedString<N>                          fixed_string.hpp
  get_arch_prefix()                       signature.hpp  [detail::]

Internal (not public API)
  layout_traits<T>::has_opaque            layout_traits.hpp  [detail::]
  layout_traits<T>::total_size            layout_traits.hpp  [detail::]
  layout_traits<T>::alignment             layout_traits.hpp  [detail::]
  sig_has_padding(string_view)            sig_parser.hpp  [detail::]
  opaque_elements_safe<T>                 fwd.hpp  [detail::]
```

---

## 3. Simplification History

### Round 1: Remove redundant alias traits

| Removed | Reason |
|---------|--------|
| `serialization_free.hpp` (entire file) | `is_local_serialization_free` was `trivially_copyable && !has_pointer` -- trivial `&&` |
| `signature_compare<T,U>` | Duplicate of `layout_signatures_match` |
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

---

## 4. Why Each Remaining Concept is Necessary

### Core concepts (cannot remove any)

| Concept | Why it cannot be removed |
|---------|--------------------------|
| `get_layout_signature<T>()` | Foundation of the library. All downstream features consume the signature. |
| `layout_signatures_match<T1,T2>()` | One-line `==` wrapper, but provides the core semantic verb. Removing forces users to write `get_layout_signature<T1>() == get_layout_signature<T2>()`. |
| `has_pointer` | Core transport-safety predicate. `is_byte_copy_safe` Branch 1 and Branch 2 both read it. Users need it for "does my type contain address-space-dependent data?" |
| `has_padding` | Independent byte-coverage bitmap algorithm (not derivable from signature at the same precision). Dual-path cross-validation with `sig_has_padding`. Users need it for info-leak detection. |
| `is_byte_copy_safe_v<T>` | 4-branch recursive decision tree. Branch 3 (non-trivially-copyable class member recursion) cannot be derived from the signature. |
| `is_transfer_safe<T>(sig)` | Direct answer to the library's core question: "can T be safely byte-transported to this remote endpoint?" |

### Opaque macros (cannot reduce below 4)

| Macro | Why it is distinct |
|-------|-------------------|
| `REGISTER_OPAQUE` | Trivially copyable concrete type. Has `static_assert`. |
| `OPAQUE_TYPE_RELOCATABLE` | Non-trivially-copyable concrete type (e.g. offset_ptr wrappers). No `static_assert`. Different safety semantics. |
| `OPAQUE_CONTAINER_RELOCATABLE` | Single-parameter template. Embeds element type signature. Generates `opaque_elements_safe` specialization that recurses into element. |
| `OPAQUE_MAP_RELOCATABLE` | Two-parameter template. Cannot be expressed by single-parameter variant. |

### Tools (display + pipeline)

| Concept | Why it is kept |
|---------|---------------|
| `SafetyLevel` / `classify_signature` | Consumed by `CompatReporter::compare()` for report output. Kept in `detail::` namespace to signal "not for programmatic decisions". |
| `SigExporter` | Phase 1 of cross-platform pipeline. No alternative. |
| `CompatReporter` | Phase 2 of cross-platform pipeline. No alternative. |

### Internal (not public, but technically necessary)

| Concept | Role |
|---------|------|
| `has_opaque` | Cross-validation `static_assert` skip condition in `layout_traits`. Not exposed as user API. |
| `total_size` / `alignment` | `sizeof`/`alignof` wrappers in `detail::layout_traits`. Zero concept cost. |
| `sig_has_padding` | Second path of dual-path padding validation. |
| `opaque_elements_safe<T>` | Recursion termination point for `is_byte_copy_safe` on opaque types. |

---

## 5. Conclusion

The concept structure is minimal. Every public concept satisfies at least
one of:

1. Has an irreplaceable consumer in the core logic
2. Cannot be derived from any other concept
3. Provides independent user decision value

No two concepts are equivalent, mutually derivable, or trivially composable
from other concepts.
