## MODIFIED Requirements

### Requirement: Compile-Time Compatibility Check
The library SHALL provide constexpr functions (`sig_match`, `layout_match`, `definition_match`) for comparing signatures across platforms. The `layout_match` and `definition_match` functions SHALL delegate to `sig_match` and serve as semantic aliases. All shared preprocessor machinery (FOR_EACH macros) SHALL be defined in a single `detail/foreach.hpp` header, included by both `sig_export.hpp` and `compat_auto.hpp`. Code comments SHALL use terse, developer-oriented style without marketing language or academic jargon.

#### Scenario: Layout match check
- **GIVEN** two `constexpr const char[]` layout signatures from different platforms
- **WHEN** calling `constexpr sig_match(sig_a, sig_b)`
- **THEN** the result SHALL be `true` if and only if the string contents are identical
- **AND** the function SHALL be usable in `static_assert`

#### Scenario: Static assert workflow
- **GIVEN** `.sig.hpp` headers from platform A and platform B
- **WHEN** both are `#include`-d in a single translation unit
- **THEN** `static_assert(sig_match(plat_a::Type_layout, plat_b::Type_layout))` SHALL compile if and only if the signatures match

#### Scenario: Comment style
- **WHEN** a developer reads any tool header file
- **THEN** comments SHALL be terse and factual, without decorative separator lines (`=====`), marketing phrases, or over-explanatory phrasing

#### Scenario: Macro deduplication
- **WHEN** both `sig_export.hpp` and `compat_auto.hpp` are included in the same translation unit
- **THEN** the FOR_EACH macro definitions come from `detail/foreach.hpp` with no redefinition warnings

#### Scenario: Dead code removal
- **WHEN** `compat_auto.hpp` is compiled
- **THEN** no unused macro stubs (`TYPELAYOUT_DETAIL_ASSERT_FIRST`, `TYPELAYOUT_DETAIL_ASSERT_VS_FIRST`, `TYPELAYOUT_ASSERT_COMPAT_2`) are present
