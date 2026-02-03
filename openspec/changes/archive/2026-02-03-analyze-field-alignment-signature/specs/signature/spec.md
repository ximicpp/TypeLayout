## ADDED Requirements

### Requirement: Field Alignment Redundancy Analysis
The library documentation SHALL include analysis of field alignment information redundancy in signatures.

#### Scenario: Analysis documented
- **GIVEN** the current signature format includes `[s:SIZE,a:ALIGN]` for each field type
- **WHEN** analyzing signature efficiency
- **THEN** the analysis SHALL document which cases are redundant
- **AND** the analysis SHALL document which cases require the information
- **AND** the analysis SHALL conclude that offset `@N` already captures alignment effects
- **AND** the decision SHALL be to retain the detailed format for self-documentation benefits

### Requirement: Signature Format Optimization Consideration
The library SHALL consider signature format optimizations to reduce redundancy while preserving layout verification accuracy.

#### Scenario: Optimization evaluation
- **GIVEN** a proposed signature format optimization
- **WHEN** evaluating the optimization
- **THEN** the evaluation SHALL consider backward compatibility
- **AND** the evaluation SHALL verify that layout mismatches are still detectable
- **AND** the evaluation SHALL measure signature length reduction

### Requirement: Signature Format Retention Decision
The library SHALL retain the current detailed signature format with per-field `[s:SIZE,a:ALIGN]` information.

#### Scenario: Format justification
- **GIVEN** field-level size/alignment is technically derivable from type names
- **WHEN** considering signature format changes
- **THEN** the format SHALL be retained for self-documentation
- **AND** the format SHALL be retained for debugging convenience
- **AND** the format SHALL be retained for backward compatibility