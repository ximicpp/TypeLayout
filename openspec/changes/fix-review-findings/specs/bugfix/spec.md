## ADDED Requirements

### Requirement: Container relocatable pointer_free scan must match canonical token set

The `pointer_free` derivation in `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE` and `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE` MUST scan for all 5 pointer-like tokens: `ptr[`, `fnptr[`, `memptr[`, `ref[`, `rref[`. This set MUST match the tokens used by `layout_traits.hpp`'s `sig_has_pointer` detection.

#### Scenario: Container with function-pointer element reports pointer_free=false
- **WHEN** a container-relocatable type is instantiated with an element type whose signature contains `fnptr[`
- **THEN** the macro-generated `pointer_free` SHALL be `false`

#### Scenario: Container with plain scalar element reports pointer_free=true
- **WHEN** a container-relocatable type is instantiated with an element type whose signature contains no pointer tokens
- **THEN** the macro-generated `pointer_free` SHALL be `true`

### Requirement: Opaque container signature format documented in project rules

AGENTS.md and rules.mdc MUST document the opaque container and map signature formats alongside the existing opaque format.

#### Scenario: Developer reads signature format rules
- **WHEN** a developer consults AGENTS.md or rules.mdc for signature format grammar
- **THEN** the documentation SHALL list `O(Tag|SIZE|ALIGN)<elem_sig>` for containers and `O(Tag|SIZE|ALIGN)<key_sig,val_sig>` for maps
