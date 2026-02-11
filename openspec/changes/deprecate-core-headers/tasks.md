## 1. Update Internal References

- [ ] 1.1 Change `test/test_fixed_string.cpp` include from `core/fwd.hpp` to `fixed_string.hpp`

## 2. Add Deprecation Warnings to core/ Shims

- [ ] 2.1 Add `#warning` to `core/fwd.hpp`
- [ ] 2.2 Add `#warning` to `core/signature_detail.hpp`
- [ ] 2.3 Add `#warning` to `core/signature.hpp`
- [ ] 2.4 Add `#warning` to `core/opaque.hpp`

## 3. Create Migration Guide

- [ ] 3.1 Create `docs/migration-guide.md` with header mapping table
- [ ] 3.2 Document TYPELAYOUT_REGISTER_TYPES usage
- [ ] 3.3 Document FixedString<N> semantic change (P2484 alignment)
- [ ] 3.4 Document new directory structure overview

## 4. Verify

- [ ] 4.1 Build and run all tests
