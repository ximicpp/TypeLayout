## Why

TypeLayout has a conceptual contradiction in its trust model:

- `admission.hpp` provides `is_byte_copy_safe_v<T>`, accepting relocatable opaque types (XVector, XString, etc.) based on user declarations
- `opaque.hpp` provides `TYPELAYOUT_OPAQUE_*_RELOCATABLE` macros, declaring these types as byte-copy safe
- `layout_traits<T>::has_pointer` returns false for these types (offset_ptr is not a raw pointer)

But `serialization_free.hpp` uses `is_local_serialization_free_v<T>` as the precondition for `is_transfer_safe<T>(remote_sig)` -- which requires `trivially_copyable`. All relocatable opaque types are NOT trivially_copyable, so `is_transfer_safe` returns false for them unconditionally.

This means TypeLayout's own cross-platform verification API is completely unusable for types that TypeLayout itself recognizes as byte-copy safe. The trust model is applied inconsistently: `is_byte_copy_safe` trusts user declarations, but `is_transfer_safe` does not.

## What Changes

### 1. `is_transfer_safe<T>` unifies local safety precondition

Replace `is_local_serialization_free_v<T>` with `is_byte_copy_safe_v<T>` as the precondition in `is_transfer_safe`. This makes the trust model consistent: if TypeLayout trusts a type for byte-copy, it also trusts it for cross-endpoint transfer (given signature match).

### 2. `SignatureRegistry::register_local<T>` unifies static_assert

Both `register_local` overloads use `is_local_serialization_free_v<T>` in their static_assert. Change to `is_byte_copy_safe_v<T>` to allow registering relocatable opaque types.

### 3. Document opaque tag matching assumption

Add documentation in `opaque.hpp` explaining the user's responsibility: types registered with the same tag, sizeof, and alignof are assumed to have identical internal binary layout. TypeLayout does NOT verify internal field layout of opaque types.

### 4. Preserve `is_local_serialization_free_v`

Not deleted. It remains as a strict C++ POD safety query for scenarios that do not involve relocatable containers.

## Capabilities

### Modified Capabilities
- `type-signature`: `is_transfer_safe` now accepts all types where `is_byte_copy_safe_v<T> == true`

## Impact

- **Files**: `serialization_free.hpp` (API semantics change), `opaque.hpp` (documentation)
- **API change**: `is_transfer_safe<T>` semantics broadened -- from "trivially_copyable + no pointer + sig match" to "byte-copy safe + sig match"
- **Backward compatible**: Types that previously returned true still return true (`is_local_serialization_free_v` is a subset of `is_byte_copy_safe_v`). Newly accepted types are relocatable opaque types (previously always false).
- **Tests**: Need to add tests verifying relocatable opaque types work with `is_transfer_safe` and `SignatureRegistry`

## Open Questions

None -- all design decisions were resolved during explore session.