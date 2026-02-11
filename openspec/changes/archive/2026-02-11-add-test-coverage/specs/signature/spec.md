## ADDED Requirements

### Requirement: FixedString Unit Test Coverage
The library SHALL have direct unit tests for all public `FixedString<N>`
operations, independent of signature generation.

#### Scenario: Constructor and length
- **WHEN** a `FixedString` is constructed from a string literal
- **THEN** `length()` SHALL return the number of non-null characters

#### Scenario: Concatenation correctness
- **WHEN** two `FixedString` values are concatenated
- **THEN** the result SHALL contain both strings in sequence
- **AND** `length()` SHALL equal the sum of both operand lengths

#### Scenario: Cross-size equality
- **WHEN** a `FixedString<10>` containing "abc" is compared with a `FixedString<100>` containing "abc"
- **THEN** `operator==` SHALL return `true`

#### Scenario: Empty string handling
- **WHEN** a `FixedString` is default-constructed
- **THEN** `length()` SHALL return 0
- **AND** `skip_first()` SHALL return an empty string

### Requirement: SigExporter Output Verification
The library SHALL have tests verifying that `SigExporter::write()` produces
compilable, structurally correct .sig.hpp files.

#### Scenario: Exported signatures match runtime values
- **WHEN** `SigExporter::write()` generates a .sig.hpp file
- **THEN** the signature strings in the file SHALL match the values from
  `get_layout_signature<T>()` and `get_definition_signature<T>()`
