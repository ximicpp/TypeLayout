## ADDED Requirements

### Requirement: Compile-Time Fixed-Size String
The library SHALL provide a `FixedString<N>` type that supports compile-time
string manipulation including concatenation, equality comparison, length
computation, leading-character removal, stream output, and conversion to
`std::string_view`.

#### Scenario: Conversion to string_view
- **WHEN** a `FixedString<N>` value exists
- **THEN** it SHALL be implicitly convertible to `std::string_view`
- **AND** the resulting `string_view` SHALL have the same length as `FixedString::length()`

#### Scenario: Concatenation
- **WHEN** two `FixedString` values are concatenated with `operator+`
- **THEN** the result SHALL contain the characters of both operands in order

#### Scenario: Equality comparison
- **WHEN** two `FixedString` values are compared with `operator==`
- **THEN** the result SHALL be `true` if and only if their logical content is identical
- **AND** comparison SHALL work across different buffer sizes

#### Scenario: Skip first character
- **WHEN** `skip_first()` is called on a non-empty `FixedString`
- **THEN** the result SHALL contain all characters except the first
