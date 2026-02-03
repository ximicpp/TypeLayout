# signature Specification

> **Layer**: Core (`<boost/typelayout.hpp>`)

## Purpose

Defines the layout signature generation system - the core capability of Boost.TypeLayout.
Layout signatures provide bit-accurate, human-readable descriptions of type memory layouts
for same-process and same-platform type verification.
## Requirements
### Requirement: Layout Signature Architecture
The library SHALL provide layout signature generation for compile-time memory layout analysis.

#### Scenario: Layout Compatibility
- **GIVEN** a user needs same-process or same-platform type checking
- **WHEN** using `get_layout_signature<T>()`
- **THEN** the signature SHALL reflect memory layout (size, alignment, field offsets)
- **AND** platform SHALL be implicitly the current build platform

#### Scenario: Signature format
- **GIVEN** a type T
- **WHEN** generating layout signature
- **THEN** the signature SHALL include platform prefix (e.g., `[64-le]`)
- **AND** the signature SHALL include type category (struct/class/union/enum)
- **AND** the signature SHALL include size and alignment
- **AND** for aggregate types, member information SHALL be included

#### Scenario: Platform prefix format
- **GIVEN** the current build platform
- **WHEN** generating any layout signature
- **THEN** the prefix SHALL be `[BITS-ENDIAN]` where BITS is 32 or 64 and ENDIAN is `le` or `be`
- **AND** example: `[64-le]` for 64-bit little-endian platforms

### Requirement: Layout Hash Generation
The library SHALL provide 64-bit hash generation for layout signatures.

#### Scenario: Hash computation
- **GIVEN** a type T with layout signature S
- **WHEN** calling `get_layout_hash<T>()`
- **THEN** the result SHALL be a 64-bit FNV-1a hash of S

#### Scenario: Hash determinism
- **GIVEN** two types T and U with identical layout signatures
- **WHEN** computing hashes
- **THEN** `get_layout_hash<T>() == get_layout_hash<U>()` SHALL be true

#### Scenario: Hash for runtime use
- **GIVEN** a hash value
- **WHEN** used at runtime (e.g., in shared memory headers)
- **THEN** the hash SHALL be usable for quick layout verification

### Requirement: Layout Verification
The library SHALL provide dual-hash verification for collision resistance.

#### Scenario: Dual hash computation
- **GIVEN** a type T
- **WHEN** calling `get_layout_verification<T>()`
- **THEN** the result SHALL contain both FNV-1a and DJB2 hashes
- **AND** the result SHALL contain signature length

#### Scenario: Collision detection
- **GIVEN** two types T and U with different layouts
- **WHEN** their FNV-1a hashes happen to collide
- **THEN** `verifications_match<T, U>()` SHALL return false if DJB2 hashes differ

### Requirement: Signature Comparison
The library SHALL provide compile-time signature comparison utilities.

#### Scenario: Direct signature match
- **GIVEN** two types T and U
- **WHEN** calling `signatures_match<T, U>()`
- **THEN** the result SHALL be true if and only if their layout signatures are identical

#### Scenario: Hash-based match
- **GIVEN** two types T and U
- **WHEN** calling `hashes_match<T, U>()`
- **THEN** the result SHALL be true if and only if their layout hashes are identical

### Requirement: Layout Concepts
The library SHALL provide C++20 concepts for compile-time layout validation.

#### Scenario: LayoutSupported concept
- **GIVEN** a type T
- **WHEN** evaluating `LayoutSupported<T>`
- **THEN** the result SHALL be true if T can have its layout signature computed
- **AND** the result SHALL be false for void, function types, and incomplete types

#### Scenario: LayoutCompatible concept
- **GIVEN** two types T and U
- **WHEN** evaluating `LayoutCompatible<T, U>`
- **THEN** the result SHALL be true if `signatures_match<T, U>()` is true

#### Scenario: LayoutMatch concept
- **GIVEN** a type T and expected signature string S
- **WHEN** evaluating `LayoutMatch<T, S>`
- **THEN** the result SHALL be true if T's signature equals S

#### Scenario: LayoutHashMatch concept
- **GIVEN** a type T and expected hash value H
- **WHEN** evaluating `LayoutHashMatch<T, H>`
- **THEN** the result SHALL be true if `get_layout_hash<T>() == H`

### Requirement: Type Categories
The library SHALL distinguish different type categories in signatures.

#### Scenario: Struct type
- **GIVEN** a non-polymorphic class/struct without base classes
- **WHEN** generating signature
- **THEN** the category SHALL be `struct`
- **AND** format SHALL be `struct[s:SIZE,a:ALIGN]{FIELDS}`

#### Scenario: Class with inheritance
- **GIVEN** a class with base classes
- **WHEN** generating signature
- **THEN** the category SHALL be `class`
- **AND** `inherited` marker SHALL be present
- **AND** base class signatures SHALL be included

#### Scenario: Polymorphic class
- **GIVEN** a class with virtual functions
- **WHEN** generating signature
- **THEN** the category SHALL be `class`
- **AND** `polymorphic` marker SHALL be present

#### Scenario: Union type
- **GIVEN** a union type
- **WHEN** generating signature
- **THEN** the category SHALL be `union`
- **AND** all members SHALL have offset 0

#### Scenario: Enum type
- **GIVEN** an enum type
- **WHEN** generating signature
- **THEN** the category SHALL be `enum`
- **AND** underlying type signature SHALL be included

### Requirement: Field Information
The library SHALL include detailed field information for aggregate types.

#### Scenario: Field offset and name
- **GIVEN** a struct with fields
- **WHEN** generating signature
- **THEN** each field SHALL include `@OFFSET[NAME]:TYPE`
- **AND** OFFSET SHALL match the actual byte offset

#### Scenario: Bit-field format
- **GIVEN** a struct with bit-fields
- **WHEN** generating signature
- **THEN** bit-fields SHALL use format `@BYTE.BIT[NAME]:bits<WIDTH,UNDERLYING>`
- **AND** BYTE is byte offset, BIT is bit offset within that byte

#### Scenario: Anonymous members
- **GIVEN** a struct with anonymous union or struct members
- **WHEN** generating signature
- **THEN** anonymous members SHALL use `<anon:N>` placeholder
- **AND** N SHALL be a sequential index

### Requirement: Alignment Information Completeness
The layout signature SHALL contain sufficient alignment information to reconstruct the complete memory layout.

#### Scenario: Alignment reconstruction
- **GIVEN** a type T with specific alignment requirements
- **WHEN** the layout signature is generated
- **THEN** the signature SHALL contain enough information to determine all padding locations

#### Scenario: User-specified alignment
- **GIVEN** a type with alignas(N) specification
- **WHEN** the layout signature is generated
- **THEN** the signature SHALL reflect the actual alignment value N

### Requirement: User-Defined Types Support Documentation
The library documentation SHALL include a comprehensive guide for user-defined types support.

#### Scenario: User checks class support
- **GIVEN** a user wants to know if their custom class is supported
- **WHEN** they consult the type support documentation
- **THEN** they SHALL find clear guidance on supported class variants and any limitations

### Requirement: Analysis Placeholder
The library documentation SHALL include a value proposition and completeness analysis.

#### Scenario: Analysis complete
- **WHEN** all analysis tasks are completed
- **THEN** the results SHALL be documented in the proposal.md file

### Requirement: Field Alignment Redundancy Analysis
The library documentation SHALL include analysis of field alignment information redundancy in signatures.

#### Scenario: Analysis documented
- **GIVEN** the current signature format includes `[s:SIZE,a:ALIGN]` for each field type
- **WHEN** analyzing signature efficiency
- **THEN** the analysis SHALL document which cases are redundant
- **AND** the analysis SHALL document which cases require the information
- **AND** the analysis SHALL conclude that offset `@N` already captures alignment effects
- **AND** the decision SHALL be to retain the detailed format for self-documentation benefits

### Requirement: Signature Format Optimization Consideration
The library SHALL consider signature format optimizations to reduce redundancy while preserving layout verification accuracy.

#### Scenario: Optimization evaluation
- **GIVEN** a proposed signature format optimization
- **WHEN** evaluating the optimization
- **THEN** the evaluation SHALL consider backward compatibility
- **AND** the evaluation SHALL verify that layout mismatches are still detectable
- **AND** the evaluation SHALL measure signature length reduction

### Requirement: Signature Format Retention Decision
The library SHALL retain the current detailed signature format with per-field `[s:SIZE,a:ALIGN]` information.

#### Scenario: Format justification
- **GIVEN** field-level size/alignment is technically derivable from type names
- **WHEN** considering signature format changes
- **THEN** the format SHALL be retained for self-documentation
- **AND** the format SHALL be retained for debugging convenience
- **AND** the format SHALL be retained for backward compatibility

