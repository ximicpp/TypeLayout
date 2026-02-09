## MODIFIED Requirements

### Requirement: Compile-Time Compatibility Check
The library SHALL provide constexpr functions (`sig_match`, `layout_match`, `definition_match`) for comparing signatures across platforms. The `layout_match` and `definition_match` functions SHALL delegate to `sig_match` and serve as semantic aliases. All shared preprocessor machinery (FOR_EACH macros) SHALL be defined in a single `detail/foreach.hpp` header, included by both `sig_export.hpp` and `compat_auto.hpp`.

#### Scenario: Layout match check
- **GIVEN** two `constexpr const char[]` layout signatures from different platforms
- **WHEN** calling `constexpr sig_match(sig_a, sig_b)`
- **THEN** the result SHALL be `true` if and only if the string contents are identical
- **AND** the function SHALL be usable in `static_assert`

#### Scenario: Static assert workflow
- **GIVEN** `.sig.hpp` headers from platform A and platform B
- **WHEN** both are `#include`-d in a single translation unit
- **THEN** `static_assert(sig_match(plat_a::Type_layout, plat_b::Type_layout))` SHALL compile if and only if the signatures match
- **AND** a clear error message SHALL be provided on mismatch

#### Scenario: Macro deduplication
- **WHEN** both `sig_export.hpp` and `compat_auto.hpp` are included in the same translation unit
- **THEN** the FOR_EACH macro definitions come from `detail/foreach.hpp` with no redefinition warnings

#### Scenario: Dead code removal
- **WHEN** `compat_auto.hpp` is compiled
- **THEN** no unused macro stubs (`TYPELAYOUT_DETAIL_ASSERT_FIRST`, `TYPELAYOUT_DETAIL_ASSERT_VS_FIRST`, `TYPELAYOUT_ASSERT_COMPAT_2`) are present

### Requirement: Runtime Compatibility Report
The library SHALL provide a runtime reporter for human-readable compatibility analysis. Code comments SHALL be concise — decorative separator lines and redundant explanations SHALL be minimized while preserving all semantic information.

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

#### Scenario: Comment readability
- **WHEN** a developer reads `compat_check.hpp`
- **THEN** each public API element has a brief doc comment, and no section has more than 3 consecutive comment-only lines