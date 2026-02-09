## MODIFIED Requirements

### Requirement: Compile-Time Layout Comparison
The system SHALL provide constexpr comparison functions (`sig_match`, `layout_match`, `definition_match`) for use in `static_assert`. The `layout_match` and `definition_match` functions SHALL delegate to `sig_match` and serve as semantic aliases. All shared preprocessor machinery (FOR_EACH macros) SHALL be defined in a single `detail/foreach.hpp` header, included by both `sig_export.hpp` and `compat_auto.hpp`.

#### Scenario: Macro deduplication
- **WHEN** both `sig_export.hpp` and `compat_auto.hpp` are included in the same translation unit
- **THEN** the FOR_EACH macro definitions come from `detail/foreach.hpp` with no redefinition warnings

#### Scenario: Dead code removal
- **WHEN** `compat_auto.hpp` is compiled
- **THEN** no unused macro stubs (`TYPELAYOUT_DETAIL_ASSERT_FIRST`, `TYPELAYOUT_DETAIL_ASSERT_VS_FIRST`, `TYPELAYOUT_ASSERT_COMPAT_2`) are present

### Requirement: Runtime Compatibility Reporter
The system SHALL provide a `CompatReporter` class that produces a formatted compatibility matrix with zero-serialization transfer (ZST) verdicts. Code comments SHALL be concise — decorative separator lines and redundant explanations SHALL be minimized while preserving all semantic information.

#### Scenario: Comment readability
- **WHEN** a developer reads `compat_check.hpp`
- **THEN** each public API element has a brief doc comment, and no section has more than 3 consecutive comment-only lines
