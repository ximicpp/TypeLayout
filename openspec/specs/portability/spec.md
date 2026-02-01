# portability Specification

## Purpose
TBD - created by archiving change refactor-portability-naming. Update Purpose after archive.
## Requirements
### Requirement: Serialization Safety Check

The library SHALL provide a compile-time check `is_trivially_serializable<T>()` that returns `true` only if a type can be safely serialized via `memcpy` and transmitted across process boundaries.

A type is trivially serializable if and only if:
- It does not contain pointer types (`T*`, `void*`)
- It does not contain reference types (`T&`, `T&&`)
- It does not contain member pointers (`T C::*`)
- It does not contain `std::nullptr_t`
- It does not contain platform-dependent types (`long`, `size_t`, etc.)
- It does not contain bit-fields
- All nested members and base classes are also trivially serializable

#### Scenario: Primitive types are serializable
- **WHEN** checking `is_trivially_serializable<int32_t>()`
- **THEN** the result is `true`

#### Scenario: Pointer types are not serializable
- **WHEN** checking `is_trivially_serializable<int*>()`
- **THEN** the result is `false`

#### Scenario: Struct with pointer member is not serializable
- **WHEN** checking `is_trivially_serializable<StructWithPointer>()`
- **THEN** the result is `false`

#### Scenario: Pure data struct is serializable
- **WHEN** checking `is_trivially_serializable<PureDataStruct>()`
- **THEN** the result is `true`

### Requirement: Deprecated Alias for Backward Compatibility

The library SHALL provide deprecated aliases for the old API names to ease migration:
- `is_portable<T>()` → deprecated alias for `is_trivially_serializable<T>()`
- `is_portable_v<T>` → deprecated alias for `is_trivially_serializable_v<T>`
- `Portable<T>` → deprecated alias for `TriviallySerializable<T>`

#### Scenario: Deprecated alias compiles with warning
- **WHEN** user code uses `is_portable<int>()`
- **THEN** the code compiles successfully
- **AND** a deprecation warning is emitted (if compiler supports)

### Requirement: Architecture Documentation

The library documentation SHALL clearly explain the layered architecture:

1. **Core Layer (Layout Signature)**:
   - `get_layout_signature<T>()` generates signatures for ANY type
   - Pure layout verification, no semantic judgment
   - Guarantee: identical signature ⟺ identical memory layout

2. **Utility Layer (Serialization Check)**:
   - `is_trivially_serializable<T>()` checks if type is safe for cross-process transmission
   - Independent of signature system
   - Users choose to use based on their scenario

#### Scenario: Documentation exists
- **WHEN** user reads the architecture documentation
- **THEN** the layered design is clearly explained
- **AND** the distinction between layout and semantics is documented

