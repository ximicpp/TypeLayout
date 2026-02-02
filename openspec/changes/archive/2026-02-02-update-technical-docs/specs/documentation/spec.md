## MODIFIED Requirements

### Requirement: Technical Documentation Accuracy
The technical documentation SHALL accurately reflect the current implementation state and API surface.

#### Scenario: API documentation matches implementation
- **GIVEN** the technical documentation (technical-report.md, design-rationale.md)
- **WHEN** a user reads the API descriptions
- **THEN** all mentioned functions, concepts, and macros SHALL exist in the actual codebase
- **AND** the function signatures SHALL match the documentation

#### Scenario: STL support documentation reflects transparent reflection
- **GIVEN** the STL type support documentation
- **WHEN** describing how STL types are handled
- **THEN** the documentation SHALL describe transparent reflection (reflecting internal members)
- **AND** the documentation SHALL NOT mention opaque specializations that no longer exist
