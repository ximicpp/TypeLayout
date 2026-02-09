## ADDED Requirements

### Requirement: Definition Layer Value Analysis
The Definition layer SHALL be retained as a core component of the two-layer signature system, providing incremental value over the Layout layer through field name encoding, inheritance structure preservation, and enum qualified name tracking.

#### Scenario: Semantic drift detection
- **WHEN** a struct field is renamed from `timeout_ms` to `timeout_seconds` without changing the byte layout
- **THEN** `layout_signatures_match` SHALL return true (byte-compatible)
- **AND** `definition_signatures_match` SHALL return false (semantic change detected)
- **AND** this difference demonstrates the unique value of the Definition layer

#### Scenario: ODR violation detection across compilation units
- **WHEN** host and plugin independently compile the same-named struct with different field names but identical byte layout
- **THEN** the Definition layer SHALL detect the structural difference
- **AND** the Layout layer alone SHALL NOT detect this difference

#### Scenario: Nested structure vs flat structure differentiation
- **WHEN** one type contains a nested struct member and another has equivalent flat fields
- **THEN** `layout_signatures_match` SHALL return true (same byte offsets)
- **AND** `definition_signatures_match` SHALL return false (different structural hierarchy)
