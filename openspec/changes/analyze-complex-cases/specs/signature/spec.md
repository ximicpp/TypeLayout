## ADDED Requirements

### Requirement: Complex Case Validation

The TypeLayout library SHALL correctly handle complex type structures including:
- Deep nesting (at least 10 levels)
- Large structures (at least 100 members)
- Complex template recursion
- Diamond/virtual inheritance

#### Scenario: Deep nested structure signature generation
- **WHEN** a structure with 10 levels of nesting is passed to `get_layout_signature`
- **THEN** a valid, complete signature including all nested levels SHALL be returned
- **AND** compilation SHALL complete within reasonable resource limits

#### Scenario: Large structure signature generation
- **WHEN** a structure with 100+ members is passed to `get_layout_signature`
- **THEN** a valid signature containing all member layouts SHALL be returned
- **AND** no compile-time resource exhaustion SHALL occur

#### Scenario: Complex template signature generation
- **WHEN** a deeply nested template type (e.g., `std::tuple<std::tuple<...>>`) is passed
- **THEN** the complete recursive type structure SHALL be captured in the signature

#### Scenario: Diamond inheritance signature generation
- **WHEN** a class with diamond inheritance pattern is passed
- **THEN** the virtual base class layout SHALL appear only once in the signature
- **AND** the signature SHALL correctly reflect the actual memory layout
