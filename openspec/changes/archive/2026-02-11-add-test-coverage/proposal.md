# Change: Add Test Coverage for FixedString and SigExporter

## Why

The design review (D5.2a, D5.2b) identified two significant test gaps:

1. `FixedString<N>` is the foundation type for the entire library but has
   zero direct unit tests. All validation is indirect through signature
   generation. Buffer boundary issues, cross-size equality edge cases,
   and empty string handling could go undetected.

2. `SigExporter::write()` generates .sig.hpp files that are consumed by
   `compat_check.hpp`. This Phase 1 -> Phase 2 handoff point has no
   verification test. A malformed export would silently break the
   cross-platform pipeline.

## What Changes

- Add `test/test_fixed_string.cpp` with direct unit tests for all
  `FixedString` public operations
- Add `test/test_sig_export.cpp` testing SigExporter output structure
- Update `CMakeLists.txt` with new test targets

## Impact

- Affected specs: `signature`
- New files: `test/test_fixed_string.cpp`, `test/test_sig_export.cpp`
- Modified: `CMakeLists.txt`
