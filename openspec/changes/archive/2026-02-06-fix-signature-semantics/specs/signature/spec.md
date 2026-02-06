## MODIFIED Requirements

### Requirement: Layout Signature Architecture
The library SHALL provide layout signature generation for compile-time memory layout analysis with configurable signature modes.

#### Scenario: Layout Compatibility
- **GIVEN** a user needs same-process or same-platform type checking
- **WHEN** using `get_layout_signature<T>()`
- **THEN** the signature SHALL reflect memory layout (size, alignment, field offsets)
- **AND** platform SHALL be implicitly the current build platform
- **AND** member names SHALL NOT be included by default (Structural mode)

#### Scenario: Signature format - Structural mode (default)
- **GIVEN** a type T
- **WHEN** generating layout signature with default mode
- **THEN** the signature SHALL include platform prefix (e.g., `[64-le]`)
- **AND** the signature SHALL include type category (struct/class/union/enum)
- **AND** the signature SHALL include size and alignment
- **AND** for aggregate types, member offset and type information SHALL be included
- **AND** member names SHALL NOT be included

#### Scenario: Signature format - Annotated mode
- **GIVEN** a type T
- **WHEN** generating layout signature with `SignatureMode::Annotated`
- **THEN** the signature SHALL include all Structural mode information
- **AND** member names SHALL be included in `@OFFSET[NAME]:TYPE` format
- **AND** type names SHALL be included for nested types

#### Scenario: Platform prefix format
- **GIVEN** the current build platform
- **WHEN** generating any layout signature
- **THEN** the prefix SHALL be `[BITS-ENDIAN]` where BITS is 32 or 64 and ENDIAN is `le` or `be`
- **AND** example: `[64-le]` for 64-bit little-endian platforms

## ADDED Requirements

### Requirement: Signature Mode Configuration
The library SHALL provide a `SignatureMode` enum to control signature content.

#### Scenario: Mode enumeration
- **GIVEN** a user wants to choose signature content level
- **WHEN** examining available modes
- **THEN** `SignatureMode::Structural` SHALL be available (layout-only, no names)
- **AND** `SignatureMode::Annotated` SHALL be available (includes names)
- **AND** `Structural` SHALL be the default mode

#### Scenario: API with mode parameter
- **GIVEN** a type T
- **WHEN** calling `get_layout_signature<T, SignatureMode::Structural>()`
- **THEN** the result SHALL exclude member and type names
- **AND** when calling `get_layout_signature<T, SignatureMode::Annotated>()`
- **THEN** the result SHALL include member and type names

#### Scenario: Convenience aliases
- **GIVEN** a type T
- **WHEN** calling `get_structural_signature<T>()`
- **THEN** the result SHALL equal `get_layout_signature<T, SignatureMode::Structural>()`
- **AND** when calling `get_annotated_signature<T>()`
- **THEN** the result SHALL equal `get_layout_signature<T, SignatureMode::Annotated>()`

### Requirement: Formal Layout Identity Guarantee
The library SHALL guarantee that identical structural signatures imply identical memory layouts.

#### Scenario: Core guarantee formalization
- **GIVEN** two types T and U
- **WHEN** their structural signatures are compared
- **THEN** `get_layout_signature<T>() == get_layout_signature<U>()` SHALL be true
- **IF AND ONLY IF** `sizeof(T) == sizeof(U)` AND `alignof(T) == alignof(U)` AND all corresponding member offsets, sizes, and type categories match

#### Scenario: Name independence
- **GIVEN** two types T and U with identical layouts but different member names
- **WHEN** comparing their structural signatures
- **THEN** `signatures_match<T, U>()` SHALL return true
- **AND** `get_layout_hash<T>() == get_layout_hash<U>()` SHALL be true

#### Scenario: Namespace independence
- **GIVEN** two types in different namespaces with identical layouts
- **WHEN** comparing their structural signatures
- **THEN** `signatures_match<T, U>()` SHALL return true

## MODIFIED Requirements

### Requirement: Layout Hash Generation
The library SHALL provide 64-bit hash generation for structural layout signatures.

#### Scenario: Hash computation
- **GIVEN** a type T with structural layout signature S
- **WHEN** calling `get_layout_hash<T>()`
- **THEN** the result SHALL be a 64-bit FNV-1a hash of S
- **AND** the hash SHALL ALWAYS be computed from Structural mode signature

#### Scenario: Hash determinism
- **GIVEN** two types T and U with identical memory layouts (regardless of names)
- **WHEN** computing hashes
- **THEN** `get_layout_hash<T>() == get_layout_hash<U>()` SHALL be true

#### Scenario: Hash for runtime use
- **GIVEN** a hash value
- **WHEN** used at runtime (e.g., in shared memory headers)
- **THEN** the hash SHALL be usable for quick layout verification

### Requirement: Field Information
The library SHALL include detailed field information for aggregate types based on signature mode.

#### Scenario: Structural mode field format
- **GIVEN** a struct with fields
- **WHEN** generating Structural signature
- **THEN** each field SHALL use format `@OFFSET:TYPE`
- **AND** OFFSET SHALL match the actual byte offset
- **AND** member name SHALL NOT be included

#### Scenario: Annotated mode field format
- **GIVEN** a struct with fields
- **WHEN** generating Annotated signature
- **THEN** each field SHALL include `@OFFSET[NAME]:TYPE`
- **AND** OFFSET SHALL match the actual byte offset
- **AND** NAME SHALL be the member identifier

#### Scenario: Bit-field format
- **GIVEN** a struct with bit-fields
- **WHEN** generating signature
- **THEN** bit-fields SHALL use format `@BYTE.BIT:bits<WIDTH,UNDERLYING>` (Structural)
- **OR** `@BYTE.BIT[NAME]:bits<WIDTH,UNDERLYING>` (Annotated)
- **AND** BYTE is byte offset, BIT is bit offset within that byte

#### Scenario: Anonymous members
- **GIVEN** a struct with anonymous union or struct members
- **WHEN** generating Annotated signature
- **THEN** anonymous members SHALL use `<anon:N>` placeholder
- **AND** N SHALL be a sequential index
