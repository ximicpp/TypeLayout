## ADDED Requirements

### Requirement: Architecture Decision Documentation
The library SHALL document the rationale for key architectural decisions.

#### Scenario: Two-layer justification
- **GIVEN** the library provides two signature layers (Layout and Definition)
- **THEN** the documentation SHALL explain why a single layer is insufficient
- **AND** the documentation SHALL explain why a third layer (semantic) is unnecessary
- **AND** the documentation SHALL describe Layout as answering "can I memcpy?" and Definition as answering "is the structure identical?"

#### Scenario: Two-phase pipeline justification
- **GIVEN** the library separates signature export (Phase 1) from compatibility check (Phase 2)
- **THEN** the documentation SHALL explain that Phase 1 requires P2996 while Phase 2 requires only C++17
- **AND** the documentation SHALL explain the persistent value of signature files (.sig.hpp) for version control and auditing
- **AND** the documentation SHALL note that the two-phase design retains value even after P2996 standardization

#### Scenario: FixedString performance boundary
- **GIVEN** the library uses FixedString<N> for compile-time string manipulation
- **THEN** the documentation SHALL document the performance boundary (~100 fields with default constexpr step limits)
- **AND** the documentation SHALL provide compiler flag recommendations for larger types
