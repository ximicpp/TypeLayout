## MODIFIED Requirements

### Requirement: is_transfer_safe precondition

`is_transfer_safe<T>(remote_sig)` SHALL use `is_byte_copy_safe_v<T>` as its compile-time local precondition instead of `is_local_serialization_free_v<T>`.

#### Scenario: Trivially copyable type, matching signature

- **WHEN** `T` is trivially_copyable, has no pointer, and remote_sig matches local signature
- **THEN** `is_transfer_safe<T>(remote_sig)` SHALL return `true`
- **NOTE** This is existing behavior, preserved unchanged

#### Scenario: Trivially copyable type, mismatched signature

- **WHEN** `T` is trivially_copyable, has no pointer, but remote_sig differs from local signature
- **THEN** `is_transfer_safe<T>(remote_sig)` SHALL return `false`
- **NOTE** Existing behavior, preserved unchanged

#### Scenario: Type with pointer

- **WHEN** `T` contains a pointer member (`is_byte_copy_safe_v<T>` is false)
- **THEN** `is_transfer_safe<T>(remote_sig)` SHALL return `false` regardless of remote_sig
- **NOTE** Existing behavior, preserved unchanged

#### Scenario: Relocatable opaque type, matching signature

- **WHEN** `T` is a registered relocatable opaque type (`is_byte_copy_safe_v<T>` is true) and remote_sig matches local signature
- **THEN** `is_transfer_safe<T>(remote_sig)` SHALL return `true`
- **NOTE** NEW behavior: previously always returned false because T is not trivially_copyable

#### Scenario: Relocatable opaque type, mismatched signature

- **WHEN** `T` is a registered relocatable opaque type but remote_sig differs from local signature
- **THEN** `is_transfer_safe<T>(remote_sig)` SHALL return `false`

#### Scenario: Composite struct with opaque members, matching signature

- **WHEN** `T` is a struct containing relocatable opaque members (`is_byte_copy_safe_v<T>` is true) and remote_sig matches local signature
- **THEN** `is_transfer_safe<T>(remote_sig)` SHALL return `true`

#### Scenario: Opaque type with HasPointer=true

- **WHEN** `T` is an opaque type registered with `HasPointer=true` (`is_byte_copy_safe_v<T>` is false)
- **THEN** `is_transfer_safe<T>(remote_sig)` SHALL return `false` regardless of remote_sig

### Requirement: SignatureRegistry accepts byte-copy-safe types

`SignatureRegistry::register_local<T>()` and `SignatureRegistry::register_local<T>(key)` SHALL use `is_byte_copy_safe_v<T>` in their static_assert instead of `is_local_serialization_free_v<T>`.

#### Scenario: Register relocatable opaque type

- **WHEN** `T` is a registered relocatable opaque type (`is_byte_copy_safe_v<T>` is true)
- **THEN** `register_local<T>()` SHALL compile successfully

#### Scenario: Register struct with opaque members

- **WHEN** `T` is a struct containing relocatable opaque members (`is_byte_copy_safe_v<T>` is true)
- **THEN** `register_local<T>()` SHALL compile successfully

#### Scenario: Register type with pointer still fails

- **WHEN** `T` contains a pointer (`is_byte_copy_safe_v<T>` is false)
- **THEN** `register_local<T>()` SHALL fail at compile time with a static_assert

### Requirement: is_local_serialization_free unchanged

`is_local_serialization_free_v<T>` SHALL NOT change its behavior or semantics.

#### Scenario: Existing behavior preserved

- **WHEN** any type `T` is checked with `is_local_serialization_free_v<T>`
- **THEN** the result SHALL be identical to the pre-change behavior

### Requirement: serialization_free_assert unchanged

`serialization_free_assert<T>` SHALL NOT change its behavior or semantics. It remains a strict C++ POD safety check.

#### Scenario: Assert behavior preserved

- **WHEN** `serialization_free_assert<T>` is used on any type
- **THEN** the behavior SHALL be identical to the pre-change behavior

### Requirement: serialization_free.hpp header comment update

The header comment SHALL be updated to reflect that `is_transfer_safe` now accepts byte-copy-safe types, not just trivially_copyable + no pointer types.

### Requirement: Opaque tag matching documentation

`opaque.hpp` SHALL document the opaque tag matching assumption: types registered with the same tag, sizeof, and alignof are assumed to have identical internal binary layout. This is the user's responsibility.