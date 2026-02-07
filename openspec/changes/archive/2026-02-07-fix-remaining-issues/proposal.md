# Change: Fix remaining issues from code quality and definition signature audits

## Why
Two rounds of审计 (review-code-quality, analyze-definition-signature) identified 6 remaining issues:
1 P0 correctness defect and 5 test coverage gaps. The P0 bug in `qualified_name_for`
can cause false-positive signature matches when types reside in deep (>=3 level) namespaces.

## What Changes
- **BREAKING**: Fix `qualified_name_for` to fully recurse the `parent_of` chain instead of stopping at grandparent level
- Add 5 missing Definition-layer test cases: virtual inheritance (`~vbase<>`), multiple inheritance Definition format, deeply nested struct Definition, union Definition field names, deep namespace (>=3 level) qualified name verification

## Impact
- Affected specs: signature (Requirement: Qualified Names in Definition)
- Affected code:
  - `include/boost/typelayout/core/signature_detail.hpp` (lines 25-45: `qualified_name_for`)
  - `test/test_two_layer.cpp` (new test cases)
