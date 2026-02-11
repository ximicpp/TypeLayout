# Change: Deprecate core/ Shim Headers and Create Migration Guide

## Why

After the `refactor-header-architecture` change, the `core/` directory contains
4 backward-compatibility shim headers that simply `#include` the new root-level
headers. These shims:

1. Add no value -- they are pure forwarding includes.
2. Confuse new users about which headers to use.
3. Create a false impression of an extra abstraction layer.
4. Are only referenced by 1 file in the entire codebase
   (`test/test_fixed_string.cpp` includes `core/fwd.hpp`).

Library-internal code (including all detail/ and tools/ headers) already uses
the new root-level paths exclusively.

## What Changes

### Phase 1: Immediate (this change)

1. **Update the last internal reference**: Change `test/test_fixed_string.cpp`
   from `core/fwd.hpp` to `fixed_string.hpp`.
2. **Add deprecation warnings** to all 4 `core/` shim headers via
   `#pragma message` or `[[deprecated]]` comments.
3. **Create `docs/migration-guide.md`** documenting:
   - Old include path -> New include path mapping
   - `TYPELAYOUT_REGISTER_TYPES` as the new non-main alternative
   - FixedString<N> semantic change (N = char count, not buffer size)

### Phase 2: Future (separate change, after downstream migration)

4. Delete the `core/` directory entirely.

## Deprecated Headers Mapping

| Old Path (deprecated) | New Path |
|---|---|
| `core/fwd.hpp` | `fwd.hpp` + `fixed_string.hpp` |
| `core/signature_detail.hpp` | `detail/type_map.hpp` |
| `core/signature.hpp` | `signature.hpp` |
| `core/opaque.hpp` | `opaque.hpp` |

## Impact

- Affected specs: `signature`
- **BREAKING**: None (shims remain functional with deprecation warning)
- Affected code: `test/test_fixed_string.cpp`, 4 `core/` headers, new docs file
