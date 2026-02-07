## MODIFIED Requirements

### Requirement: FixedString Compile-Time String
The system SHALL provide a `FixedString<N>` template for compile-time string manipulation with minimal template parameter bloat.

#### Scenario: Number conversion returns compact buffer
- **WHEN** `from_number(x)` is called with any integer value
- **THEN** the result SHALL be `FixedString<21>` (uint64_t max = 20 digits + '\0'), not `FixedString<32>`

#### Scenario: Equality comparison with C-string is safe
- **WHEN** comparing `FixedString<N>` with a `const char*` using `operator==`
- **THEN** the comparison SHALL NOT access memory beyond the null terminator of either operand
