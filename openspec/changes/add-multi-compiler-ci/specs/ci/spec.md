## ADDED Requirements

### Requirement: Multi-Compiler CI

The project SHALL be tested against multiple compiler versions in CI.

#### Scenario: CI matrix includes P2996 compiler
- **WHEN** reviewing CI configuration
- **THEN** Bloomberg Clang with P2996 support is included

#### Scenario: All matrix builds pass
- **WHEN** CI runs on pull request
- **THEN** all compiler/platform combinations pass
