# signature Specification

> **Layer**: Core (`<boost/typelayout.hpp>`)

## Purpose
Defines the layout signature generation system - the core capability of Boost.TypeLayout.
Layout signatures provide bit-accurate, human-readable descriptions of type memory layouts.
## Requirements
### Requirement: Two-Layer Signature Architecture
The library SHALL provide two distinct signature layers: Layout Compatibility (Layer 1) and Serialization Compatibility (Layer 2).

#### Scenario: Layer 1 - Layout Compatibility
- **GIVEN** a user needs same-process or same-platform type checking
- **WHEN** using `get_layout_signature<T>()`
- **THEN** the signature SHALL reflect memory layout (size, alignment, field offsets)
- **AND** platform SHALL be implicitly the current build platform

#### Scenario: Layer 2 - Serialization Compatibility
- **GIVEN** a user needs cross-process/cross-machine memcpy-safe data transfer
- **WHEN** using `get_serialization_signature<T, PlatformSet>()`
- **THEN** a platform set P SHALL be explicitly specified
- **AND** the type SHALL be validated against Layer 1 (Layout) requirements first
- **AND** additional serialization constraints SHALL be applied

#### Scenario: Layer hierarchy relationship
- **GIVEN** the two layers
- **THEN** Serialization compatibility(P) SHALL imply Layout compatibility
- **AND** Layout compatibility SHALL NOT imply Serialization compatibility(P)
- **AND** the relationship SHALL be: `Serialization(P) ⊂ Layout`

### Requirement: Platform-Relative Serialization Compatibility
Serialization compatibility SHALL NOT exist as a universal concept. It SHALL always be defined relative to a specific, user-specified platform set.

#### Scenario: No universal serialization check
- **GIVEN** a type T
- **WHEN** user attempts to check serialization without specifying a platform set
- **THEN** a compile-time error SHALL be raised

#### Scenario: Platform set required
- **GIVEN** a user defines platform set P as "64-bit little-endian"
- **WHEN** checking `is_serializable<T, P>`
- **THEN** the check SHALL validate T against all platforms in set P

#### Scenario: long/unsigned long always rejected
- **GIVEN** a struct using `long` or `unsigned long` member
- **WHEN** checking serialization for any platform set
- **THEN** the type SHALL NOT be serializable
- **AND** the blocker reason SHALL be "platform" (HasPlatformDependentSize)
- **RATIONALE** `long` is 4 bytes on Windows (LLP64) but 8 bytes on Linux (LP64), making it unsafe for cross-platform serialization

### Requirement: Serialization Compatibility Check
The library SHALL provide compile-time detection of types that are safe for memcpy-based serialization.

#### Scenario: Trivially copyable requirement
- **GIVEN** a type T
- **WHEN** checking serialization compatibility
- **THEN** T MUST be trivially copyable (std::is_trivially_copyable_v<T>)

#### Scenario: No pointer members
- **GIVEN** a struct with pointer or reference members
- **WHEN** checking serialization compatibility
- **THEN** the type SHALL be marked as not serializable with reason "ptr"

#### Scenario: No polymorphic types
- **GIVEN** a class with virtual functions (is_polymorphic)
- **WHEN** checking serialization compatibility
- **THEN** the type SHALL be marked as not serializable with reason "poly"

#### Scenario: Recursive member check
- **GIVEN** a struct with nested struct members
- **WHEN** checking serialization compatibility
- **THEN** all nested members SHALL be recursively checked for serialization compatibility

### Requirement: Serialization Signature Generation
The library SHALL generate serialization signatures that include serializability status.

#### Scenario: Serializable type signature
- **GIVEN** a POD struct `struct Point { int x; int y; };`
- **WHEN** calling `get_serialization_signature<Point>()`
- **THEN** the signature SHALL include `serial` marker: `struct[s:8,a:4,serial]{...}`

#### Scenario: Non-serializable type signature
- **GIVEN** a struct with pointer `struct Node { int val; Node* next; };`
- **WHEN** calling `get_serialization_signature<Node>()`
- **THEN** the signature SHALL include `!serial:ptr` marker

#### Scenario: Polymorphic type signature
- **GIVEN** a class with virtual function `class Base { virtual void f(); };`
- **WHEN** calling `get_serialization_signature<Base>()`
- **THEN** the signature SHALL include `!serial:poly` marker

### Requirement: Platform Constraints (Required)
The library SHALL REQUIRE users to specify target platform constraints for serialization compatibility. Serialization signatures are only valid within the specified platform set.

#### Scenario: Platform constraint is mandatory
- **GIVEN** a user wants to check serialization compatibility
- **WHEN** no platform constraint is specified
- **THEN** a compile-time error SHALL be raised

#### Scenario: Single platform target
- **GIVEN** user specifies `PlatformSet::x64_le` (64-bit little-endian)
- **WHEN** generating serialization signature
- **THEN** the signature prefix SHALL be `[64-le,serial]`
- **AND** types with platform-dependent sizes SHALL be validated against 64-bit

#### Scenario: Platform-dependent type rejection
- **GIVEN** a struct with `long` member (4 bytes on Windows LLP64, 8 bytes on Linux LP64)
- **AND** user specifies a platform set that includes both Windows and Linux 64-bit
- **WHEN** checking serialization compatibility
- **THEN** the type SHALL be marked as not serializable with reason "platform-size"

#### Scenario: Fixed-width types preferred
- **GIVEN** a struct using only fixed-width types (int32_t, uint64_t, etc.)
- **WHEN** checking serialization compatibility across any platform set
- **THEN** the type SHALL pass the platform size check

#### Scenario: Endianness constraint
- **GIVEN** user specifies little-endian only platforms
- **WHEN** the current build platform is big-endian
- **THEN** serialization signature generation SHALL fail with platform mismatch error

### Requirement: Predefined Platform Sets
The library SHALL provide predefined platform sets based on bitwidth and endianness (not microarchitecture).

#### Scenario: Common platform sets
- **GIVEN** the library's predefined platform sets
- **THEN** the following SHALL be available:
  - `PlatformSet::bits64_le()` - 64-bit little-endian (x64, arm64, etc.)
  - `PlatformSet::bits64_be()` - 64-bit big-endian
  - `PlatformSet::bits32_le()` - 32-bit little-endian (x86, arm32, etc.)
  - `PlatformSet::bits32_be()` - 32-bit big-endian
  - `PlatformSet::current()` - Current build platform

#### Scenario: Platform abstraction rationale
- **GIVEN** x64 and arm64 on 64-bit little-endian
- **THEN** they SHALL use the same platform set `bits64_le()`
- **RATIONALE** Standard C++ types have identical sizes on both architectures

#### Scenario: Custom platform set
- **GIVEN** user needs a custom platform combination
- **WHEN** creating a custom `PlatformSet`
- **THEN** user SHALL be able to specify `BitWidth` (32/64) and `Endianness` (Little/Big)

### Requirement: is_serializable Trait
The library SHALL provide a compile-time trait `is_serializable<T>` for checking serialization compatibility.

#### Scenario: POD type check
- **GIVEN** `struct Point { int x; int y; };`
- **WHEN** evaluating `is_serializable<Point>::value`
- **THEN** the result SHALL be `true`

#### Scenario: Pointer-containing type check
- **GIVEN** `struct Node { int val; Node* next; };`
- **WHEN** evaluating `is_serializable<Node>::value`
- **THEN** the result SHALL be `false`

#### Scenario: Nested serializable type check
- **GIVEN** `struct Rect { Point tl; Point br; };` where Point is serializable
- **WHEN** evaluating `is_serializable<Rect>::value`
- **THEN** the result SHALL be `true`

### Requirement: Serialization Blocker Diagnostic
The library SHALL provide information about what prevents a type from being serializable.

#### Scenario: Single blocker
- **GIVEN** a struct with one pointer member
- **WHEN** querying `serialization_blocker<T>`
- **THEN** the result SHALL identify the blocking member and reason

#### Scenario: Multiple blockers
- **GIVEN** a polymorphic class with pointer members
- **WHEN** querying `serialization_blocker<T>`
- **THEN** all blocking reasons SHALL be reported (poly, ptr)

