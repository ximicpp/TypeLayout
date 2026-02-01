## MODIFIED Requirements

### Requirement: Serialization Safety Check API
The library SHALL provide a **single, unified** compile-time check for serialization safety: `is_serializable_v<T, PlatformSet>`.

#### Scenario: Unified API with platform parameter
- **GIVEN** a user needs to check if a type is safe for memcpy-based serialization
- **WHEN** using `is_serializable_v<T, P>`
- **THEN** the check SHALL validate against the specified platform set P
- **AND** return `true` only if the type can be safely serialized to that platform set

#### Scenario: Current platform shortcut
- **GIVEN** a user only needs to check for current platform
- **WHEN** using `is_serializable_v<T, PlatformSet::current()>`
- **THEN** the check SHALL validate against the current build platform

#### Scenario: Deprecated legacy API
- **GIVEN** the legacy `is_trivially_serializable<T>()` API
- **THEN** it SHALL be marked as `[[deprecated]]`
- **AND** the deprecation message SHALL suggest `is_serializable_v<T, PlatformSet::current()>`

#### Scenario: Complete serialization blockers
- **GIVEN** a type T being checked for serializability
- **THEN** the check SHALL reject types with:
  - Non-trivially-copyable types
  - Pointer members (direct or nested)
  - Reference members
  - Polymorphic types (has virtual functions)
  - Bit-fields (implementation-defined layout)
  - Platform-dependent size types (`long`, `unsigned long`)

### Requirement: Deprecated APIs
The following APIs SHALL be marked as deprecated:

#### Scenario: is_trivially_serializable deprecation
- **GIVEN** `is_trivially_serializable<T>()`
- **THEN** it SHALL be marked `[[deprecated("Use is_serializable_v<T, PlatformSet::current()>")]]`

#### Scenario: TriviallySerializable concept deprecation
- **GIVEN** `TriviallySerializable<T>` concept
- **THEN** it SHALL be marked `[[deprecated("Use is_serializable_v<T, P>")]]`

#### Scenario: Portable concept deprecation (already deprecated)
- **GIVEN** `Portable<T>` concept
- **THEN** it SHALL remain deprecated, pointing to `is_serializable_v<T, P>`
