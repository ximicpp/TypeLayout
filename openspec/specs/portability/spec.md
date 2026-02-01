# Serializability Specification

> **Layer**: Utility (`<boost/typelayout/typelayout_util.hpp>`)

## Purpose
This specification defines the Utility layer serializability checking API, which determines whether types are safe for binary serialization and cross-process transmission. This is built on top of the Core layout signature engine.

## Requirements

### Requirement: Serialization Safety Check

The library SHALL provide a compile-time variable template `is_serializable_v<T, PlatformSet>` that returns `true` only if a type can be safely serialized via `memcpy` and transmitted across process boundaries.

A type is serializable if and only if:
- It does not contain pointer types (`T*`, `void*`)
- It does not contain reference types (`T&`, `T&&`)
- It does not contain member pointers (`T C::*`)
- It does not contain `std::nullptr_t`
- It does not contain platform-dependent types (`long`, `wchar_t`, `long double`)
- It does not contain bit-fields
- All nested members and base classes are also serializable

#### Scenario: Primitive types are serializable
- **WHEN** checking `is_serializable_v<int32_t, PlatformSet::current()>`
- **THEN** the result is `true`

#### Scenario: Pointer types are not serializable
- **WHEN** checking `is_serializable_v<int*, PlatformSet::current()>`
- **THEN** the result is `false`

#### Scenario: Struct with pointer member is not serializable
- **WHEN** checking `is_serializable_v<StructWithPointer, PlatformSet::bits64_le()>`
- **THEN** the result is `false`

#### Scenario: Pure data struct is serializable
- **WHEN** checking `is_serializable_v<PureDataStruct, PlatformSet::bits64_le()>`
- **THEN** the result is `true`

### Requirement: Serializable Concept

The library SHALL provide a C++20 concept `Serializable<T>` that checks serializability for the current platform.

#### Scenario: Concept constrains template functions
- **GIVEN** a function `template<Serializable T> void send(const T&)`
- **WHEN** called with a type containing a pointer
- **THEN** compilation fails with a clear constraint error

### Requirement: Serialization Status

The library SHALL provide a function `serialization_status<T, PlatformSet>()` that returns a diagnostic string explaining why a type is or is not serializable.

#### Scenario: Status string for serializable type
- **WHEN** calling `serialization_status<int32_t, PlatformSet::current()>()`
- **THEN** the result contains "serial" (indicating serializable)

#### Scenario: Status string for pointer type
- **WHEN** calling `serialization_status<int*, PlatformSet::current()>()`
- **THEN** the result contains "!serial:ptr" (indicating blocked by pointer)

### Requirement: Architecture Documentation

The library documentation SHALL clearly explain the layered architecture:

1. **Layer 1 (Layout Signature)**:
   - `get_layout_signature<T>()` generates signatures for ANY type
   - Pure layout verification, no semantic judgment
   - Guarantee: identical signature ⟺ identical memory layout

2. **Layer 2 (Serialization Status)**:
   - `is_serializable_v<T, PlatformSet>` checks if type is safe for cross-process transmission
   - Rejects pointers, references, bit-fields, and platform-dependent types
   - Users choose to use based on their scenario

#### Scenario: Documentation exists
- **WHEN** user reads the architecture documentation
- **THEN** the layered design is clearly explained
- **AND** the distinction between layout and serializability is documented

