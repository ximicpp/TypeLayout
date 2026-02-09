## MODIFIED Requirements

### Requirement: Runtime Compatibility Report
The library SHALL provide a runtime reporter for human-readable compatibility analysis.

#### Scenario: Example output reflects ZST model
- **GIVEN** the example `compat_check.cpp` with 8 types including Safe, Conditional, and Unsafe types
- **WHEN** the `CompatReporter::print_report()` is executed across 3 platforms
- **THEN** the example output SHALL demonstrate all four verdicts:
  - "Serialization-free" for types with C1 (Layout MATCH) AND C2 (Safety = Safe)
  - "Layout OK (pointer values not portable)" for types with C1 AND Safety = Warning
  - "Layout OK (verify bit-fields manually)" for types with C1 AND Safety = Risk
  - "Needs serialization" for types without C1 (Layout DIFFER)
- **AND** the summary SHALL show separate counts for serialization-free (C1 ∧ C2) and layout-compatible (C1 only)
- **AND** the example comments SHALL distinguish between layout-match verification (C1) and zero-serialization safety (C1 ∧ C2)
