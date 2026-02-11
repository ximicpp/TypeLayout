## ADDED Requirements

### Requirement: FixedString Utility
The library SHALL provide a `FixedString<N>` compile-time string type where
`N` represents the **character count** (excluding the null terminator),
consistent with C++ proposal P2484 (`std::basic_fixed_string`).
The internal buffer SHALL be `N + 1` bytes to accommodate the null terminator.
CTAD SHALL deduce `N` from string literals such that
`FixedString{"hello"}` produces `FixedString<5>`.

#### Scenario: CTAD deduction from string literal
- **WHEN** a user writes `FixedString{"hello"}`
- **THEN** the deduced type SHALL be `FixedString<5>`
- **AND** `FixedString<5>::size` SHALL equal `5`

#### Scenario: Concatenation preserves character count semantics
- **WHEN** `FixedString<3>` is concatenated with `FixedString<4>` via `operator+`
- **THEN** the result type SHALL be `FixedString<7>`
- **AND** the resulting string SHALL contain all characters from both operands

#### Scenario: Construction from string_view
- **WHEN** a `FixedString<N>` is constructed from a `std::string_view` of length L
- **THEN** `min(N, L)` characters SHALL be copied
- **AND** the buffer SHALL be null-terminated

#### Scenario: Signature output unchanged
- **WHEN** signature generation uses the updated FixedString
- **THEN** all generated signature strings SHALL be byte-identical to the previous implementation
