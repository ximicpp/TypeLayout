## ADDED Requirements

### Requirement: Standard Alignment Tracking
The library SHALL document known standard/proposal alternatives for each
custom utility via TODO comments in source code, enabling future migration
when standard equivalents become available.

#### Scenario: TODO comments present for tracked proposals
- **WHEN** a contributor reads a custom utility (e.g., `always_false`, `qualified_name_for`)
- **THEN** a TODO comment SHALL reference the relevant C++ proposal number
- **AND** the comment SHALL describe the replacement path
