## 1. Update Internal References

- [x] 1.1 Change `test/test_fixed_string.cpp` include from `core/fwd.hpp` to `fixed_string.hpp`

## 2. Add Deprecation Warnings to core/ Shims

- [x] 2.1 Add `#warning` to `core/fwd.hpp`
- [x] 2.2 Add `#warning` to `core/signature_detail.hpp`
- [x] 2.3 Add `#warning` to `core/signature.hpp`
- [x] 2.4 Add `#warning` to `core/opaque.hpp`

## 3. Create Migration Guide

- [x] 3.1 Create `docs/migration-guide.md` with header mapping table
- [x] 3.2 Document TYPELAYOUT_REGISTER_TYPES usage
- [x] 3.3 Document FixedString<N> semantic change (P2484 alignment)
- [x] 3.4 Document new directory structure overview

## 4. Verify

- [x] 4.1 Build and run all tests