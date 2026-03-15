## Why

System review of the 54eab2c..801b8d4 commit range (2026-03-02 to 2026-03-15, ~45 commits) identified 8 findings. Two require code fixes (R8: container relocatable macro pointer scan incomplete; R7: documentation out of sync with new opaque container signature format). The remaining are design trade-off acknowledgments or optional enhancements.

## What Changes

- **Fix R8 -- Container/Map Relocatable `pointer_free` scan incomplete**: `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE` and `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE` only scan for `ptr[` when deriving `pointer_free`. They miss `fnptr[`, `memptr[`, `ref[`, and `rref[`. Must align with `layout_traits.hpp`'s `sig_has_pointer()` token set.
- **Fix R7 -- Update AGENTS.md and rules.mdc for opaque container signature format**: The new `O(name|N|A)<elem_sig>` and `O(name|N|A)<key_sig,val_sig>` formats are not documented in AGENTS.md or rules.mdc.
- **Fix R6 annotation -- Add comment explaining RELOCATABLE pointer_free=true assumption**: The `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE` macro hardcodes `pointer_free = true` by design (relocatable semantics exclude native pointers). Add an explicit comment documenting this assumption.
- **Fix R3 annotation -- Document cross-validation skip scope in layout_traits.hpp**: When a struct contains any opaque member, the entire struct's padding cross-validation is skipped. Add a comment explaining the scope and rationale of this relaxation.

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

(none -- all changes are bug fixes and documentation sync, no spec-level behavior changes)

## Impact

- **opaque.hpp**: Fix `pointer_free` derivation in `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE` and `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE` macros. Add comment to `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE`.
- **layout_traits.hpp**: Add documentation comment to cross-validation `static_assert`.
- **AGENTS.md**: Update Signature Format Rules section for opaque container format.
- **.codemaker/rules/rules.mdc**: Update Signature Format Grammar section.
- **No breaking changes**. No API signature changes. No test behavior changes (existing tests do not exercise container elements with fnptr/memptr/ref/rref).
- **New test coverage recommended**: Add test case for container with function-pointer element to verify `pointer_free` correctness after fix.
