## MODIFIED Requirements

### Requirement: Serialization Safety Check
The library SHALL provide serialization safety checking as a **utility feature** built on top of the core layout signature functionality.

**Positioning**: This is a utility/application layer feature, not core functionality.

#### Scenario: Utility module location
- **GIVEN** a user needs serialization safety checking
- **WHEN** including the functionality
- **THEN** they SHALL use `<boost/typelayout/util/serialization_check.hpp>`
- **OR** the convenience header `<boost/typelayout/typelayout_util.hpp>`

#### Scenario: Dependency on core
- **GIVEN** the serialization check utility
- **THEN** it SHALL depend on core layout signature functionality
- **AND** core SHALL NOT depend on serialization utilities
- **AND** users can use core without loading utility code

#### Scenario: Unified API with platform parameter
- **GIVEN** a user needs to check if a type is safe for memcpy-based serialization
- **WHEN** using `is_serializable_v<T, P>`
- **THEN** the check SHALL validate against the specified platform set P
- **AND** return `true` only if the type can be safely serialized to that platform set

#### Scenario: Complete serialization blockers
- **GIVEN** a type T being checked for serializability
- **THEN** the check SHALL reject types with:
  - Non-trivially-copyable types
  - Pointer members (direct or nested)
  - Reference members
  - Polymorphic types (has virtual functions)
  - Bit-fields (implementation-defined layout)
  - Platform-dependent size types (`long`, `unsigned long`)

### Requirement: Platform Set Utility
The library SHALL provide platform set definitions as part of the utility module.

#### Scenario: Platform set location
- **GIVEN** a user needs platform set definitions
- **WHEN** including platform functionality
- **THEN** they SHALL use `<boost/typelayout/util/platform_set.hpp>`

#### Scenario: Predefined platform sets
- **GIVEN** common platform configurations
- **THEN** the following SHALL be available:
  - `PlatformSet::bits64_le()` - 64-bit little-endian
  - `PlatformSet::bits64_be()` - 64-bit big-endian
  - `PlatformSet::bits32_le()` - 32-bit little-endian
  - `PlatformSet::bits32_be()` - 32-bit big-endian
  - `PlatformSet::current()` - Current build platform
