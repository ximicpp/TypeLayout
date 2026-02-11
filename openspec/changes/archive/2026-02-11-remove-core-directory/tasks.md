## 1. Update References
- [x] 1.1 Update `test/test_util.hpp` include from `core/fwd.hpp` to root headers
- [x] 1.2 Update `PROOFS.md` file path reference

## 2. Delete core/ Directory
- [x] 2.1 Remove `include/boost/typelayout/core/fwd.hpp`
- [x] 2.2 Remove `include/boost/typelayout/core/signature_detail.hpp`
- [x] 2.3 Remove `include/boost/typelayout/core/signature.hpp`
- [x] 2.4 Remove `include/boost/typelayout/core/opaque.hpp`

## 3. Update Documentation
- [x] 3.1 Update `docs/migration-guide.md` to mark Phase 2 complete

## 4. Verify
- [x] 4.1 Build and run tests (5/5 pass)
- [x] 4.2 Grep for any remaining `core/` references in non-archive files