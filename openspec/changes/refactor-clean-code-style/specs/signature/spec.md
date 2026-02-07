## MODIFIED Requirements

### Requirement: Signature Generation
The library SHALL generate deterministic layout and definition signatures
for supported C++ types at compile time via P2996 static reflection.

#### Scenario: Internal code style
- **WHEN** a developer reads the source code
- **THEN** comments are minimal and professional, with no banner separators, emoji, or AI-style verbosity
- **AND** no dead code or unused helpers exist
