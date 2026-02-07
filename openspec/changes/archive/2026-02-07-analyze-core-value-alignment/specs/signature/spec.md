## MODIFIED Requirements
### Requirement: Layout Signature Architecture
The library SHALL provide a two-layer layout signature generation system for compile-time memory layout analysis.

- **Layer 1 (Layout)**: Pure byte-level identity with inheritance and composition flattening, vptr markers for polymorphic types
- **Layer 2 (Definition)**: Structural identity preserving type names, field names, inheritance tree, and qualified names for namespaced types/enums

#### Scenario: Two-Layer System Correctness
- **GIVEN** the two-layer signature system
- **WHEN** comparing types T and U
- **THEN** `definition_signatures_match<T,U>()` SHALL imply `layout_signatures_match<T,U>()`
- **AND** the converse SHALL NOT be required (Layout match does not imply Definition match)