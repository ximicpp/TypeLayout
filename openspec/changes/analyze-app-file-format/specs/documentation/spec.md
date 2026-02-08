## ADDED Requirements

### Requirement: File Format Application Scenario Documentation
The documentation SHALL include a dedicated section analyzing the file format compatibility scenario.

#### Scenario: Cross-platform file header verification
- **GIVEN** a binary file format with a fixed-size header structure
- **THEN** the documentation SHALL demonstrate how TypeLayout verifies that files written on one platform can be read on another
- **AND** the documentation SHALL show that Layout signature match guarantees safe fread/fwrite of the entire header

#### Scenario: Version evolution detection with Definition signatures
- **GIVEN** a file format header that evolves across versions (field renames, semantic changes)
- **THEN** the documentation SHALL demonstrate that Layout signatures may still match (same byte layout) while Definition signatures correctly detect structural changes
- **AND** the documentation SHALL present this as a key advantage of the two-layer system for file format maintenance
