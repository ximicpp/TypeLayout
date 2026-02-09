# Change: Optimize and simplify code and comments

## Why
The codebase accumulated redundancy during iterative development of the
cross-platform compatibility toolchain. Macro definitions are duplicated,
unused macro stubs remain, analysis documents overlap significantly, and
some comments are overly verbose. Cleaning these up improves maintainability,
readability, and reduces confusion for new contributors.

## What Changes
1. **Extract shared FOR_EACH macros** — move the duplicated `TYPELAYOUT_DETAIL_FE_*` /
   `TYPELAYOUT_DETAIL_FOR_EACH` machinery from `sig_export.hpp` and `compat_auto.hpp`
   into a new `detail/foreach.hpp`, included by both.
2. **Remove unused ASSERT macro stubs** — delete `TYPELAYOUT_DETAIL_ASSERT_FIRST`,
   `TYPELAYOUT_DETAIL_ASSERT_VS_FIRST`, `TYPELAYOUT_ASSERT_COMPAT_2`, and the
   reference to `TYPELAYOUT_DETAIL_ASSERT_REF_NS` in `compat_auto.hpp` (dead code
   from an earlier design iteration).
3. **Simplify `compat_check.hpp` comments** — trim decorative separator lines,
   merge adjacent comment blocks, reduce repeated explanations while preserving
   semantic content.
4. **Simplify `example/compat_check.cpp` comments** — condense the header
   block and inline comments to essential information.
5. **Deduplicate analysis docs** — trim `CROSS_PLATFORM_COLLECTION.md` sections
   that duplicate `ZERO_SERIALIZATION_TRANSFER.md` content (safety classification
   algorithm, remediation patterns) and add cross-references instead.
6. **Keep `layout_match()` / `definition_match()` aliases** — these provide
   semantic clarity for users and cost nothing at compile time; only add a
   brief note that they delegate to `sig_match()`.

## Impact
- Affected specs: `cross-platform-compat`, `documentation`
- Affected code:
  - `include/boost/typelayout/tools/compat_check.hpp` (comment simplification)
  - `include/boost/typelayout/tools/compat_auto.hpp` (dead code removal)
  - `include/boost/typelayout/tools/sig_export.hpp` (macro extraction)
  - `include/boost/typelayout/tools/detail/foreach.hpp` (new file)
  - `example/compat_check.cpp` (comment simplification)
  - `docs/analysis/CROSS_PLATFORM_COLLECTION.md` (dedup)
- No breaking API changes. No behaviour changes. No new dependencies.
