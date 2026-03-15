## Context

System review of typelayout 54eab2c..801b8d4 (45 commits, 2026-03-02 to 2026-03-15) identified 8 findings (R1-R8). This change addresses the 4 actionable items: one code bug (R8), one documentation sync (R7), and two annotation improvements (R3, R6).

Current state:
- `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE` / `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE` derive `pointer_free` by scanning only for `ptr[` in the generated signature. The canonical pointer detection in `layout_traits.hpp` scans 5 tokens: `ptr[`, `fnptr[`, `memptr[`, `ref[`, `rref[`.
- AGENTS.md and rules.mdc document opaque encoding as `O(Tag|N|A)` but do not cover the new container/map format `O(name|N|A)<elem_sig>` / `O(name|N|A)<key_sig,val_sig>`.
- Two design trade-offs (R3: cross-validation skip scope, R6: relocatable pointer_free=true) lack explanatory comments.

## Goals / Non-Goals

**Goals:**
- Align container/map relocatable `pointer_free` scan with canonical `sig_has_pointer` token set
- Update AGENTS.md and rules.mdc to document opaque container/map signature format
- Add explanatory comments for R3 and R6 design trade-offs
- Add test coverage for container element with pointer-like types

**Non-Goals:**
- Refactoring the opaque macro system (keep current macro API stable)
- Adding diagnostic output to `are_serialization_free` (R5 -- deferred, optional enhancement)
- Modifying cross-validation behavior for opaque-containing structs (R3 -- document only)
- Changing the `sig_parser.hpp` hardcoded offset (R4 -- low risk, no action)

## Decisions

### D1: Fix pointer_free scan in opaque.hpp macros

**Decision**: Replace the single `ptr[` token check with the full 5-token set matching `sig_has_pointer` in `layout_traits.hpp`.

**Rationale**: The container/map macros auto-derive `pointer_free` from the generated signature. If the element type contains `fnptr`, `memptr`, `ref`, or `rref`, these are missed, resulting in a false `pointer_free = true`. The canonical token set is already defined in `layout_traits.hpp`; the macro must use the same set for consistency.

**Implementation**: In `opaque.hpp`, change the `pointer_free` computation from:
```
!calculate().contains_token(FixedString{"ptr["})
```
to:
```
!calculate().contains_token(FixedString{"ptr["}) &&
!calculate().contains_token(FixedString{"fnptr["}) &&
!calculate().contains_token(FixedString{"memptr["}) &&
!calculate().contains_token(FixedString{"ref["}) &&
!calculate().contains_token(FixedString{"rref["})
```

**Alternative considered**: Extract a shared helper function `sig_has_pointer_token(FixedString)`. Rejected because the `contains_token` calls happen in a `consteval` macro context where calling a separate function adds complexity without clear benefit. The token set is small and stable.

### D2: Documentation update strategy

**Decision**: Update both AGENTS.md (Signature Format Rules) and rules.mdc (Signature Format Grammar) to add the opaque container/map formats as sub-entries under the existing Opaque line.

**Format**:
```
Opaque:       O(Tag|SIZE|ALIGN)                  via TYPELAYOUT_REGISTER_OPAQUE
Opaque container: O(Tag|SIZE|ALIGN)<elem_sig>    via TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE
Opaque map:       O(Tag|SIZE|ALIGN)<key,val>     via TYPELAYOUT_OPAQUE_MAP_RELOCATABLE
```

### D3: Annotation approach

**Decision**: Use concise inline `//` comments (2-3 lines max) at the specific code locations. No separate documentation files.

- R3 annotation: Above the `static_assert` in `layout_traits.hpp` that checks padding consistency. Explain that opaque members cause the entire struct's cross-validation to be skipped because the bitmap treats opaque as atomic while the signature may embed element types.
- R6 annotation: Above `pointer_free = true` in `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE`. Explain that relocatable semantics inherently exclude native pointers (offset_ptr is not a pointer).

## Risks / Trade-offs

- **[Low] Token set drift**: If new pointer-like types are added to `type_map.hpp` in the future, both `layout_traits.hpp` and `opaque.hpp` must be updated. Mitigation: the cross-file dependency map in rules.mdc already tracks `opaque.hpp` dependencies.
- **[Low] No runtime test for R8**: The fix is in `consteval` macro code. Testing requires defining a container-relocatable type with a function-pointer element, which is an unusual combination. Mitigation: add a compile-time `static_assert` test in `test_opaque.cpp`.
