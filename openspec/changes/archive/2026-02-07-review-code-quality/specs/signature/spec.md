## MODIFIED Requirements

### Requirement: FixedString Compile-Time String
The system SHALL provide a `FixedString<N>` template and a free function `to_fixed_string(num)` for compile-time string manipulation.

#### Scenario: Number conversion via free function
- **WHEN** converting an integer to a fixed string
- **THEN** the caller SHALL use `to_fixed_string(num)` instead of `FixedString<N>::from_number(num)`

#### Scenario: Foundation header independence
- **WHEN** `fwd.hpp` is included without P2996 reflection support
- **THEN** it SHALL compile without errors (no dependency on `<experimental/meta>`)
