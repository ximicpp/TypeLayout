# Change: Remove core/ Directory (Phase 2 Deprecation)

## Why

Phase 1 (change `deprecate-core-headers`) added `#pragma message` deprecation
warnings to the 4 shim headers under `include/boost/typelayout/core/` and
created a migration guide. The cooling-off period has elapsed. It is time to
physically remove the `core/` directory so the repository has exactly one
canonical include path per header.

Keeping dead shim files:
- Inflates the repository
- Confuses `find` / `grep` results with stale paths
- May mislead new contributors into thinking `core/` is active

## What Changes

1. **Update last remaining reference** in `test/test_util.hpp`:
   `core/fwd.hpp` -> `fwd.hpp` + `fixed_string.hpp`.
2. **Update `PROOFS.md`** path reference from `core/signature_detail.hpp`
   to `detail/signature_impl.hpp` + `detail/type_map.hpp`.
3. **Delete** `include/boost/typelayout/core/` directory (4 shim files).
4. **Update `docs/migration-guide.md`** to mark Phase 2 as complete.
5. **Update spec**: Remove the backward-compatibility shim requirement and
   its scenario from the signature spec (those shims no longer exist).

## Impact

- Affected specs: `signature` (remove shim requirement)
- **BREAKING**: Code that still includes `core/` paths will get a hard
  compile error. Migration guide documents the new paths.
- Affected code: `test/test_util.hpp`, `PROOFS.md`, `docs/migration-guide.md`,
  4 deleted files
