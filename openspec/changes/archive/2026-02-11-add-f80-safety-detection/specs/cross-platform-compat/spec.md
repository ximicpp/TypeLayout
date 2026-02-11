## MODIFIED Requirements

### Requirement: Runtime Compatibility Report
The library SHALL provide a runtime reporter for human-readable compatibility analysis. Code comments SHALL be concise -- decorative separator lines and redundant explanations SHALL be minimized while preserving all semantic information. The `classify_safety()` function SHALL detect all platform-dependent type markers in Layout signatures, including `wchar_t` (`wchar[`) and `long double` (`f80[`).

#### Scenario: Example output reflects ZST model
- **GIVEN** the example `compat_check.cpp` with 8 types including Safe, Conditional, and Unsafe types
- **WHEN** the `CompatReporter::print_report()` is executed across 3 platforms
- **THEN** the example output SHALL demonstrate all four verdicts:
  - "Serialization-free" for types with C1 (Layout MATCH) AND C2 (Safety = Safe)
  - "Layout OK (pointer values not portable)" for types with C1 AND Safety = Warning
  - "Layout OK (verify bit-fields manually)" for types with C1 AND Safety = Risk
  - "Needs serialization" for types without C1 (Layout DIFFER)
- **AND** the summary SHALL show separate counts for serialization-free (C1 AND C2) and layout-compatible (C1 only)
- **AND** the example comments SHALL distinguish between layout-match verification (C1) and zero-serialization safety (C1 AND C2)

#### Scenario: Comment readability
- **WHEN** a developer reads `compat_check.hpp`
- **THEN** each public API element has a brief doc comment, and no section has more than 3 consecutive comment-only lines

#### Scenario: long double classified as Risk
- **GIVEN** a Layout signature containing `f80[`
- **WHEN** calling `classify_safety(sig)`
- **THEN** the result SHALL be `SafetyLevel::Risk`
- **AND** `safety_reason()` SHALL mention `long double` in its Risk description

#### Scenario: Struct containing long double classified as Risk
- **GIVEN** a struct `struct S { int32_t x; long double y; }` with Layout signature containing `f80[`
- **WHEN** calling `classify_safety()` on its signature
- **THEN** the result SHALL be `SafetyLevel::Risk`