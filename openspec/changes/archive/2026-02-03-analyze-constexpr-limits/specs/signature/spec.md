## ADDED Requirements

### Requirement: Constexpr Step Limit Documentation

The TypeLayout library SHALL document known constexpr evaluation limits and provide guidance for users encountering these limits.

#### Scenario: User encounters constexpr step limit
- **WHEN** a user attempts to generate a signature for a type with 100+ members
- **AND** compilation fails with "constexpr evaluation hit maximum step limit"
- **THEN** the documentation SHALL clearly explain:
  - The cause of the limitation (compiler constexpr step limit)
  - Recommended compiler flags to increase the limit
  - Suggested minimum values for different type complexities
