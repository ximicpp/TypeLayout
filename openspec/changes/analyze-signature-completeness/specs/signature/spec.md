## ADDED Requirements

### Requirement: Signature Completeness Audit
Each layer's signature encoding SHALL be audited for completeness (injectivity) — ensuring that types with different layouts or structures produce different signatures.

#### Scenario: Layout layer injectivity for all type categories
- **WHEN** two types T and U have different byte layouts on the same platform
- **THEN** `get_layout_signature<T>()` SHALL differ from `get_layout_signature<U>()`
- **AND** the only known exception is the conservative `array vs discrete fields` case which is documented in Known Design Limits

#### Scenario: Definition layer injectivity for structural properties
- **WHEN** two types T and U differ in field names, inheritance structure, or enum qualified names
- **THEN** `get_definition_signature<T>()` SHALL differ from `get_definition_signature<U>()`

#### Scenario: Signature grammar unambiguity
- **WHEN** a valid signature string is produced by either layer
- **THEN** the string SHALL be uniquely parseable back to its encoded type properties
- **AND** no two different type property tuples SHALL produce the same signature string
