## MODIFIED Requirements

### Requirement: Runtime Compatibility Report
The library SHALL provide a runtime reporter for human-readable compatibility analysis.

#### Scenario: Multi-platform report
- **GIVEN** signature data from N platforms registered via `CompatReporter::add_platform()`
- **WHEN** calling `CompatReporter::print_report()`
- **THEN** the output SHALL include:
  - A list of compared platforms with their metadata
  - A per-type compatibility matrix showing MATCH/DIFFER for layout and definition
  - A per-type safety classification (Safe/Warning/Risk)
  - A per-type verdict based on the C1 ∧ C2 ZST model:
    - "Serialization-free" when Layout MATCH AND Safety = Safe (C1 ∧ C2)
    - "Layout OK (pointer values not portable)" when Layout MATCH AND Safety = Warning (C1 ∧ ¬C2)
    - "Layout OK (verify bit-fields manually)" when Layout MATCH AND Safety = Risk (C1 ∧ ¬C2)
    - "Needs serialization" when Layout DIFFER (¬C1)
  - Summary statistics with two separate counts:
    - Serialization-free types: count of types satisfying C1 ∧ C2
    - Layout-compatible types: count of types satisfying C1 (regardless of C2)
  - Assumptions note: IEEE 754 floating-point, same endianness across platforms
