# TypeLayout Concept Analysis (Revised)

Analysis of every public concept in the library after the code cleanup.
Identifies which concepts are essential, which remain redundant, and
evaluates the changes made since the initial review.

---

## 1. Changes Adopted Since Initial Review

The following changes were made in response to the first analysis:

| Recommendation | Status | Details |
|----------------|--------|---------|
| Remove `serialization_free.hpp` | [DONE] | File deleted. `is_local_serialization_free`, `serialization_free_assert` removed. |
| Remove `signature_compare<T,U>` | [DONE] | Removed from `layout_traits.hpp`. Only `layout_signatures_match` remains. |
| Remove `is_trivial_safe_v` | [DONE] | Removed from `classify.hpp`. |
| Remove `is_layout_compatible_v` (naming collision) | [DONE] | Removed from `classify.hpp`. |
| Remove `is_memcpy_safe_v` (deprecated) | [DONE] | Removed from `classify.hpp`. |
| Extract `is_transfer_safe` + `SignatureRegistry` to `transfer.hpp` | [DONE] | New file `tools/transfer.hpp` replaces transport portions of deleted `serialization_free.hpp`. |
| Rename `SignatureRegistry::is_serialization_free` | [DONE] | Renamed to `is_transfer_safe` in `transfer.hpp:88`. |
| Remove SafetyLevel / classify system | Not done | `safety_level.hpp` and `classify.hpp` retained unchanged. |
| Remove `has_bit_field` / `is_platform_variant` | Not done | Both retained in `layout_traits.hpp`. |

---

## 2. Current Concept Inventory

After cleanup, the public API consists of the following concepts,
grouped by architectural layer:

```
Layer 0 -- Infrastructure
  FixedString<N>                               fixed_string.hpp
  get_arch_prefix()                            signature.hpp:15

Layer 1 -- Signature Generation
  get_layout_signature<T>()                    signature.hpp:27
  layout_signatures_match<T1, T2>()            signature.hpp:32

Layer 2 -- Property Extraction (layout_traits<T>)
  layout_traits<T>::signature                  layout_traits.hpp:248
  layout_traits<T>::has_padding                layout_traits.hpp:268
  layout_traits<T>::has_pointer                layout_traits.hpp:250
  layout_traits<T>::has_opaque                 layout_traits.hpp:262
  layout_traits<T>::has_bit_field              layout_traits.hpp:259
  layout_traits<T>::is_platform_variant        layout_traits.hpp:265
  layout_traits<T>::field_count                layout_traits.hpp:286
  layout_traits<T>::total_size                 layout_traits.hpp:294
  layout_traits<T>::alignment                  layout_traits.hpp:295

Layer 3 -- Composite Classification
  SafetyLevel enum                             safety_level.hpp:23
  classify<T> / classify_v<T>                  classify.hpp:42
  classify_signature(string_view)              safety_level.hpp:52

Layer 4 -- Transport Safety
  is_byte_copy_safe<T>                         admission.hpp:124
  is_transfer_safe<T>(remote_sig)              transfer.hpp:32
  SignatureRegistry                            transfer.hpp:46

Layer 5 -- Opaque Registration
  TYPELAYOUT_REGISTER_OPAQUE                   opaque.hpp:31
  TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE           opaque.hpp:63
  TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE      opaque.hpp:85
  TYPELAYOUT_OPAQUE_MAP_RELOCATABLE            opaque.hpp:123
  TYPELAYOUT_REGISTER_OPAQUE_TRIVIAL           opaque.hpp:167  (alias)
  TYPELAYOUT_REGISTER_OPAQUE_RELOCATABLE       opaque.hpp:170  (alias)
  TYPELAYOUT_REGISTER_OPAQUE_CONTAINER_RELOCATABLE  opaque.hpp:173  (alias)
  TYPELAYOUT_REGISTER_OPAQUE_MAP_RELOCATABLE        opaque.hpp:176  (alias)
  opaque_elements_safe<T>                      fwd.hpp (default), opaque.hpp (specializations)

Layer 6 -- Runtime Tools
  SigExporter / TYPELAYOUT_EXPORT_TYPES        sig_export.hpp
  CompatReporter                               compat_check.hpp:105
  sig_has_padding(string_view)                 sig_parser.hpp
  sig_contains_token(string_view, string_view) sig_parser.hpp
  safety_level_name(SafetyLevel)               safety_level.hpp:31
```

Total: ~21 distinct public concepts (down from ~25).

---

## 3. Dependency Graph (Current)

```
  get_layout_signature<T>()                  [root]
            |
      layout_traits<T>                       [aggregator]
      /    |     |      \       \
has_pointer |  has_padding |   has_opaque
has_bit_field    is_platform_variant
      |         |          |
      v         v          v
  SafetyLevel / classify<T>                  [composite -- still present]
                    |
  is_byte_copy_safe<T>                       [transport]
  is_transfer_safe<T>(remote_sig)
  SignatureRegistry
                    |
  SigExporter                                [runtime tools]
  CompatReporter
  classify_signature(string_view)
  sig_has_padding(string_view)
```

---

## 4. Per-Concept Verdict (Revised)

### 4.1 Essential -- no change from initial review

All concepts identified as essential in the initial review remain essential.
The cleanup correctly preserved them:

| Concept | Location | Status |
|---------|----------|--------|
| `get_layout_signature<T>()` | `signature.hpp:27` | Retained |
| `FixedString<N>` | `fixed_string.hpp` | Retained |
| `get_arch_prefix()` | `signature.hpp:15` | Retained |
| `has_padding` | `layout_traits.hpp:268` | Retained |
| `has_pointer` | `layout_traits.hpp:250` | Retained |
| `has_opaque` | `layout_traits.hpp:262` | Retained |
| `is_byte_copy_safe<T>` | `admission.hpp:124` | Retained |
| `is_transfer_safe<T>(remote_sig)` | `transfer.hpp:32` | Retained (moved from deleted file) |
| `layout_signatures_match<T1,T2>` | `signature.hpp:32` | Retained |
| `sig_has_padding(string_view)` | `sig_parser.hpp` | Retained |
| `SigExporter` | `sig_export.hpp` | Retained |
| `TYPELAYOUT_REGISTER_OPAQUE` | `opaque.hpp:31` | Retained |
| `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE` | `opaque.hpp:85` | Retained |
| `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE` | `opaque.hpp:123` | Retained |
| `opaque_elements_safe<T>` | `fwd.hpp` / `opaque.hpp` | Retained |
| `layout_traits<T>` | `layout_traits.hpp:247` | Retained |
| `field_count` / `total_size` / `alignment` | `layout_traits.hpp:286-295` | Retained |

### 4.2 Previously redundant -- now removed

These were correctly cleaned up:

| Concept | What happened |
|---------|---------------|
| `is_local_serialization_free<T>` | Removed with `serialization_free.hpp`. Was `trivially_copyable && !has_pointer`. |
| `serialization_free_assert<T>` | Removed with `serialization_free.hpp`. Was two `static_assert` lines. |
| `signature_compare<T,U>` | Removed from `layout_traits.hpp`. Duplicate of `layout_signatures_match`. |
| `is_trivial_safe_v<T>` | Removed from `classify.hpp`. Was alias for `classify_v == TrivialSafe`. |
| `is_layout_compatible_v<T>` | Removed from `classify.hpp`. Had naming collision with `std::is_layout_compatible`. |
| `is_memcpy_safe_v<T>` | Removed from `classify.hpp`. Was deprecated alias. |

### 4.3 Still redundant -- retained but should be reconsidered

| Concept | Location | Why it remains redundant |
|---------|----------|--------------------------|
| `SafetyLevel` enum | `safety_level.hpp:23` | Still compresses orthogonal boolean dimensions into a single scalar. Information loss problem remains (see Section 5). |
| `classify<T>` / `classify_v<T>` | `classify.hpp:42` | Compile-time entry point for `SafetyLevel`. Redundant if enum is redundant. |
| `classify_signature(string_view)` | `safety_level.hpp:52` | Runtime entry point for `SafetyLevel`. Same information-loss problem, plus the documented limitation that it cannot detect `!trivially_copyable` (`safety_level.hpp:46-51`). |
| `has_bit_field` | `layout_traits.hpp:259` | Only consumed by `classify<T>` to route into `PlatformVariant`. No independent user-facing use case. Could be folded into `is_platform_variant` or kept as internal detail. |
| `is_platform_variant` | `layout_traits.hpp:265` | Detects `wchar_t` and `long double`. At compile time `sizeof` is known and fixed. This is a lint-level observation, not a decision-enabling property. The only user response is "switch to fixed-width types", which is a coding guideline, not a runtime branch. |

### 4.4 Optional but reasonable -- utility-layer conveniences

| Concept | Location | Notes |
|---------|----------|-------|
| `SignatureRegistry` | `transfer.hpp:46` | Runtime handshake facility. Depends on `std::map` + `typeid().name()`, the latter unstable across compilers. Useful for single-compiler IPC scenarios. The `is_serialization_free` method was correctly renamed to `is_transfer_safe`. |
| `CompatReporter` | `compat_check.hpp:105` | Phase 2: cross-platform comparison. `compare()` returning `vector<TypeResult>` is the core logic. `print_report()` / `print_diff_report()` are presentation. The `are_transfer_safe()` query is a valuable addition. |
| `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE` | `opaque.hpp:63` | Differs from `REGISTER_OPAQUE` only in dropping `trivially_copyable` assertion. Could be merged into one macro with a bool parameter. |
| Alias macros | `opaque.hpp:167-177` | `REGISTER_OPAQUE_TRIVIAL`, `REGISTER_OPAQUE_RELOCATABLE`, etc. Forwarding aliases. Neither help nor hurt. |
| `safety_level_name(SafetyLevel)` | `safety_level.hpp:31` | Display helper. Reasonable if `SafetyLevel` is kept. |
| `safety_label` / `safety_stars` / `safety_reason` | `compat_check.hpp:36-67` | Display helpers for `CompatReporter` output. Reasonable as formatting utilities. |

---

## 5. Remaining Issue: SafetyLevel Information Loss

The `SafetyLevel` enum and its associated `classify<T>` / `classify_signature()`
remain in the codebase. The information-loss problem identified in the
initial review still applies.

The priority order is:

```
Opaque(4) > PointerRisk(3) > PlatformVariant(2) > PaddingRisk(1) > TrivialSafe(0)
```

### Example: multiple simultaneous problems

```cpp
struct ProblematicMsg {
    int32_t  id;
    // 4 bytes padding
    double   value;
    wchar_t  label[8];   // platform-variant
    char*    debug_ptr;   // pointer
};
```

`classify<ProblematicMsg>` returns `PointerRisk`. The user sees one problem.
In reality there are three: pointer, padding, and platform-variant.

With raw traits, all three are visible simultaneously:

```cpp
layout_traits<ProblematicMsg>::has_pointer        == true
layout_traits<ProblematicMsg>::has_padding         == true
layout_traits<ProblematicMsg>::is_platform_variant == true
```

### Why it is still kept

`SafetyLevel` serves two roles that are harder to replace:

1. **CompatReporter output formatting** -- `compat_check.hpp` uses `safety_label()`,
   `safety_stars()`, `safety_reason()` extensively for human-readable reports.
   Without the enum, the report formatting code would need to compose
   multiple boolean traits into display strings inline.

2. **classify_signature(string_view)** -- runtime classification of signature
   strings from remote platforms where compile-time traits are unavailable.
   The raw `has_padding` / `has_pointer` traits require P2996 or the type
   definition; `classify_signature` works on opaque strings only.

### Recommendation

If `SafetyLevel` is retained, it should be clearly documented as a
**summary for display purposes**, not as a decision-making predicate.
Users should be directed to `layout_traits<T>::has_*` for programmatic
decisions. The enum is a lossy projection useful for reporting, not for
logic.

---

## 6. Current API Surface vs Proposed Minimal

| Layer | Current (21 concepts) | Proposed minimal (17 concepts) |
|-------|----------------------|-------------------------------|
| Infrastructure | `FixedString<N>`, `get_arch_prefix()` | Same |
| Signature | `get_layout_signature<T>()`, `layout_signatures_match<T1,T2>()` | Same |
| Properties | `has_padding`, `has_pointer`, `has_opaque`, `has_bit_field`, `is_platform_variant`, `field_count`, `total_size`, `alignment` | Remove `has_bit_field`, `is_platform_variant` (fold internally) |
| Classification | `SafetyLevel`, `classify<T>`, `classify_signature()` | Remove all three; or keep as display-only utility |
| Transport | `is_byte_copy_safe<T>`, `is_transfer_safe<T>()`, `SignatureRegistry` | Same |
| Opaque | 4 macros + 4 aliases + `opaque_elements_safe` | Remove 4 aliases; merge `OPAQUE_TYPE_RELOCATABLE` into `REGISTER_OPAQUE` |
| Tools | `SigExporter`, `CompatReporter`, `sig_has_padding()` | Same |

The gap from 21 to 17 consists of:
- `has_bit_field` (fold into internal detail)
- `is_platform_variant` (fold or remove)
- `SafetyLevel` + `classify<T>` + `classify_signature()` (remove or demote to display-only)

Minus the 4 alias macros which are zero-cost either way.

---

## 7. Assessment of Cleanup Quality

The cleanup was well-executed:

| Aspect | Assessment |
|--------|------------|
| `serialization_free.hpp` removal | Clean. No orphaned references. `is_transfer_safe` and `SignatureRegistry` correctly migrated to `transfer.hpp`. |
| `signature_compare` removal | Clean. `layout_traits.hpp` now ends at line 304 without the duplicate. |
| `classify.hpp` trimming | Clean. Removed 3 alias traits (`is_trivial_safe_v`, `is_layout_compatible_v`, `is_memcpy_safe_v`). Only `classify<T>` and `classify_v<T>` remain. File went from 70 lines to 53. |
| `SignatureRegistry` renaming | `is_serialization_free()` correctly renamed to `is_transfer_safe()` in `transfer.hpp:88`. Consistent with the function template in `transfer.hpp:32`. |
| No broken dependencies | `compat_check.hpp` still includes `safety_level.hpp` and uses `SafetyLevel` / `classify_signature()` for reporting. `sig_export.hpp` still uses `is_byte_copy_safe_v` from `admission.hpp`. All include chains intact. |

### Minor issue found

`sig_export.hpp`'s `SigExporter::add<T>` has a `static_assert` with message
text referencing "trivially copyable types", but the actual check is
`is_byte_copy_safe_v<T>` which is broader (also accepts relocatable opaque
types). The error message is slightly misleading. Not a correctness bug,
but the message should say "byte-copy safe" instead.

---

## 8. Updated Migration Guide

For the remaining redundant concepts, if further cleanup is performed:

| Concept to remove | Replacement |
|-------------------|-------------|
| `classify_v<T>` (for programmatic decisions) | Use `layout_traits<T>::has_pointer`, `has_padding`, `has_opaque` directly |
| `classify_signature(sig)` (for reports) | Keep as internal detail of `CompatReporter` if needed, remove from public API |
| `SafetyLevel` enum | Keep as `compat::SafetyLevel` scoped to report formatting if needed |
| `has_bit_field` | Fold into `is_platform_variant` check or `classify` internals |
| `is_platform_variant` | Remove from `layout_traits`, or keep as internal detail consumed only by `classify` |
| 4 alias macros | Remove; users use the primary macro names directly |
