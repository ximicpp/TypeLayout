# Change: Add f80 (long double) detection to classify_safety()

## Why

`classify_safety()` scans Layout signatures to classify types as
Safe/Warning/Risk for cross-platform shared memory. It currently detects
`wchar_t` as a platform-dependent Risk type, but misses `long double`
(`f80`). On Windows `sizeof(long double) == 8`, on Linux/macOS x64
`sizeof(long double) == 16`, and on 32-bit x86 `sizeof(long double) == 12`.
A struct containing `long double` is falsely reported as "Safe" when it
should be "Risk".

## What Changes

- Add `f80[` substring detection in `classify_safety()`, classifying it as `SafetyLevel::Risk`
- Update `safety_reason()` to include `long double` in the Risk description
- Add test coverage in `test_compat_check.cpp`

## Impact

- Affected specs: `cross-platform-compat`
- Affected code: `tools/compat_check.hpp`, `test/test_compat_check.cpp`