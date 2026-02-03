## ADDED Requirements

### Requirement: Alignment Information Completeness
The layout signature SHALL contain sufficient alignment information to reconstruct the complete memory layout.

#### Scenario: Alignment reconstruction
- **GIVEN** a type T with specific alignment requirements
- **WHEN** the layout signature is generated
- **THEN** the signature SHALL contain enough information to determine all padding locations

#### Scenario: User-specified alignment
- **GIVEN** a type with alignas(N) specification
- **WHEN** the layout signature is generated
- **THEN** the signature SHALL reflect the actual alignment value N
