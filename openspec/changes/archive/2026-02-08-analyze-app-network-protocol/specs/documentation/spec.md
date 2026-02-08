## ADDED Requirements

### Requirement: Network Protocol Application Scenario Documentation
The documentation SHALL include a dedicated section analyzing the network protocol wire-format verification scenario.

#### Scenario: Wire-format verification workflow
- **GIVEN** a binary network protocol with fixed-size header structures
- **THEN** the documentation SHALL demonstrate how TypeLayout verifies wire-format consistency between client and server platforms
- **AND** the documentation SHALL contrast this with manual offsetof/sizeof assertions showing TypeLayout's automatic completeness

#### Scenario: Byte order boundary documentation
- **GIVEN** TypeLayout encodes endianness in the architecture prefix
- **THEN** the documentation SHALL explain that endianness mismatch is detected (signatures differ) but byte-order conversion is the user's responsibility
- **AND** the documentation SHALL describe this as intentional separation of concerns
