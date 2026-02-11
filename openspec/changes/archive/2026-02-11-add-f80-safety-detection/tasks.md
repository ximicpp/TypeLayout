## 1. Core Fix

- [x] 1.1 Add `f80[` detection in `classify_safety()` in `compat_check.hpp`
- [x] 1.2 Update `safety_reason()` Risk description to mention `long double`

## 2. Test

- [x] 2.1 Add test case in `test_compat_check.cpp` verifying `f80` signature triggers Risk
- [x] 2.2 Add test case for a struct containing `long double` triggers Risk

## 3. Verify

- [x] 3.1 Build and run all tests locally