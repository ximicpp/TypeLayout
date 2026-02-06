# signature Specification

> **Layer**: Core (`<boost/typelayout.hpp>`)

## Purpose

Defines the layout signature generation system - the core capability of Boost.TypeLayout.
Layout signatures provide bit-accurate, human-readable descriptions of type memory layouts
for same-process and same-platform type verification.
## Requirements
### Requirement: Layout Signature Architecture
The library SHALL provide layout signature generation for compile-time memory layout analysis with support for large structs.

#### Scenario: Layout Compatibility
- **GIVEN** a user needs same-process or same-platform type checking
- **WHEN** using `get_layout_signature<T>()`
- **THEN** the signature SHALL reflect memory layout (size, alignment, field offsets)
- **AND** platform SHALL be implicitly the current build platform
- **AND** structs with up to 200 members SHALL be supported

#### Scenario: Scalability
- **GIVEN** a struct with many members
- **WHEN** generating layout signature
- **THEN** the implementation SHALL scale to handle large member counts
- **AND** constexpr step limits SHALL be managed through chunking

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

### Requirement: CppCon Proposal Readiness
The library SHALL be evaluated for CppCon conference proposal submission readiness.

#### Scenario: Technical innovation assessment
- **GIVEN** TypeLayout uses C++26 P2996 static reflection
- **WHEN** evaluating as a conference topic
- **THEN** the assessment SHALL identify unique value propositions
- **AND** the assessment SHALL compare with existing alternatives
- **AND** the assessment SHALL identify gaps and improvement areas

### Requirement: Demo Scenarios
The library documentation SHALL include demonstration scenarios suitable for conference presentations.

#### Scenario: Demo coverage
- **GIVEN** a conference presentation requirement
- **WHEN** preparing demonstration materials
- **THEN** demos SHALL cover basic usage patterns
- **AND** demos SHALL show bug detection capabilities
- **AND** demos SHALL explain P2996 internal mechanisms

### Requirement: Logic Clarity Analysis Report

The project SHALL include a logic clarity analysis report documenting:
- Code structure and module dependencies
- Execution flow from public API to internal implementation
- Boundary condition handling
- Logic clarity assessment with improvement recommendations

#### Scenario: Analysis report exists
- **WHEN** reviewing the project documentation
- **THEN** `doc/analysis/logic_clarity_report.md` exists
- **AND** contains sections for structure, flow, boundaries, and recommendations

### Requirement: Large Struct Support
The library SHALL support generating layout signatures for structs with up to 200 members.

#### Scenario: 50-member struct
- **GIVEN** a struct with 50 members
- **WHEN** calling `get_layout_signature<T>()` or `get_layout_hash<T>()`
- **THEN** compilation SHALL succeed without constexpr step limit errors

#### Scenario: 100-member struct
- **GIVEN** a struct with 100 members
- **WHEN** calling `get_layout_signature<T>()` or `get_layout_hash<T>()`
- **THEN** compilation SHALL succeed without constexpr step limit errors

#### Scenario: 200-member struct
- **GIVEN** a struct with 200 members
- **WHEN** calling `get_layout_signature<T>()` or `get_layout_hash<T>()`
- **THEN** compilation SHALL succeed without constexpr step limit errors

### Requirement: Chunked Signature Generation
The library SHALL use chunked processing to distribute constexpr evaluation work.

#### Scenario: Chunk size configuration
- **GIVEN** a user wants to tune chunk size for their compiler
- **WHEN** defining `BOOST_TYPELAYOUT_CHUNK_SIZE` before including headers
- **THEN** the library SHALL use that value for member processing chunk size
- **AND** the default SHALL be 8 if not defined

#### Scenario: Chunk processing isolation
- **GIVEN** a struct with N members where N > chunk size
- **WHEN** generating layout signature
- **THEN** members SHALL be processed in chunks of `BOOST_TYPELAYOUT_CHUNK_SIZE`
- **AND** each chunk SHALL be processed in a separate consteval context

### Requirement: Incremental Hash Computation
The library SHALL compute layout hashes incrementally without building full signature strings.

#### Scenario: Direct hash feeding
- **GIVEN** a type T
- **WHEN** calling `get_layout_hash<T>()`
- **THEN** the hash SHALL be computed by feeding layout data directly to the hash function
- **AND** the implementation SHALL NOT require building the full signature string first

#### Scenario: Hash consistency
- **GIVEN** any type T
- **WHEN** computing hash via incremental method
- **THEN** the result SHALL be consistent with hashing the structural signature
- **OR** the change in hash algorithm SHALL be documented as breaking change

### Requirement: Pre-sized Buffer Optimization
The library SHALL optimize string signature generation by pre-calculating required buffer size.

#### Scenario: Length pre-calculation
- **GIVEN** a type T requiring signature generation
- **WHEN** generating the layout signature string
- **THEN** the total signature length SHALL be calculated first
- **AND** the buffer SHALL be allocated at that size before writing

#### Scenario: Single-pass writing
- **GIVEN** a pre-sized signature buffer
- **WHEN** writing member information
- **THEN** each member SHALL be written directly to the final buffer
- **AND** no intermediate string copies SHALL be performed

### Requirement: Signature Mode Configuration
The library SHALL provide a `SignatureMode` enumeration with the following values:

1. **Physical** — Pure byte layout signature that flattens inheritance hierarchy and excludes C++ object model markers
2. **Structural** — Layout signature with C++ object model information including inheritance and polymorphism markers (default)
3. **Annotated** — Full signature including member names for debugging purposes

The default signature mode SHALL be `Structural` to maintain backward compatibility.

#### Scenario: Physical mode produces flattened inheritance signatures
- **GIVEN** a derived type `Derived : Base { double y; }` and a flat type `Flat { int x; double y; }` with identical memory layout
- **WHEN** Physical signatures are compared
- **THEN** they SHALL match because Physical mode flattens inheritance

#### Scenario: Structural mode preserves inheritance information
- **GIVEN** the same `Derived` and `Flat` types
- **WHEN** Structural signatures are compared
- **THEN** they SHALL NOT match because Structural mode encodes inheritance structure

#### Scenario: Default mode is Structural
- **WHEN** `get_layout_signature<T>()` is called without explicit mode
- **THEN** it SHALL use `SignatureMode::Structural`

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

### Requirement: Physical Signature Generation

The library SHALL provide `get_physical_signature<T>()` function that generates a pure byte layout signature.

Physical signatures SHALL:
1. Use `record` prefix uniformly for all class types (no `struct`/`class` distinction)
2. NOT include `polymorphic` or `inherited` markers
3. Flatten non-virtual base class sub-objects into the parent's field list
4. Skip virtual bases during recursive flattening (v1 limitation)
5. Normalize byte arrays (`char[]`, `unsigned char[]`, `std::byte[]`) to `bytes[]`

#### Scenario: Physical signature format
- **GIVEN** `struct Simple { int x; double y; };`
- **WHEN** physical signature is generated
- **THEN** it SHALL be `[64-le]record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}`

#### Scenario: Inheritance flattening
- **GIVEN** `struct Base { int x; }; struct Derived : Base { double y; };`
- **WHEN** physical signature of `Derived` is generated
- **THEN** fields from `Base` SHALL appear at their absolute offsets without `~base:` prefix

#### Scenario: Multi-level inheritance flattening
- **GIVEN** `struct A { int x; }; struct B : A { int y; }; struct C : B { int z; };`
- **WHEN** physical signature of `C` is generated
- **THEN** all fields (x, y, z) SHALL appear flattened with their absolute offsets

### Requirement: Physical Signature Comparison

The library SHALL provide `physical_signatures_match<T1, T2>()` function that compares Physical signatures of two types.

#### Scenario: Layout-identical types match
- **GIVEN** `Derived : Base` and `Flat` with identical memory layout
- **WHEN** `physical_signatures_match<Derived, Flat>()` is called
- **THEN** it SHALL return `true`

#### Scenario: Different layouts do not match
- **GIVEN** two types with different field types or offsets
- **WHEN** `physical_signatures_match` is called
- **THEN** it SHALL return `false`

### Requirement: Physical Hash Functions

The library SHALL provide:
1. `get_physical_hash<T>()` — Returns 64-bit FNV-1a hash of Physical signature
2. `physical_hashes_match<T1, T2>()` — Compares Physical hashes of two types

#### Scenario: Hash based on Physical signature
- **GIVEN** `Derived` and `Flat` with matching Physical signatures
- **WHEN** their Physical hashes are compared
- **THEN** they SHALL match

### Requirement: Physical Layout Concepts

The library SHALL provide:
1. `PhysicalLayoutCompatible<T, U>` — Concept satisfied when Physical signatures match
2. `PhysicalHashCompatible<T, U>` — Concept satisfied when Physical hashes match

#### Scenario: Concept constraints
- **GIVEN** a function template `void transfer(PhysicalLayoutCompatible<Source> auto& dest, const Source& src)`
- **WHEN** called with layout-compatible types
- **THEN** it SHALL compile successfully

### Requirement: Physical Verification

The library SHALL provide:
1. `get_physical_verification<T>()` — Returns dual-hash verification based on Physical signature
2. `physical_verifications_match<T1, T2>()` — Compares Physical verifications

#### Scenario: Dual-hash physical verification
- **GIVEN** two types with matching Physical layout
- **WHEN** their Physical verifications are compared
- **THEN** both FNV-1a and DJB2 hashes SHALL match

### Requirement: Physical Assertion Macros

The library SHALL provide:
1. `TYPELAYOUT_ASSERT_PHYSICAL_COMPATIBLE(Type1, Type2)` — Static assertion for Physical compatibility
2. `TYPELAYOUT_BIND_PHYSICAL(Type, ExpectedSig)` — Static assertion binding type to expected Physical signature

#### Scenario: Physical compatibility assertion
- **GIVEN** two layout-compatible types
- **WHEN** `TYPELAYOUT_ASSERT_PHYSICAL_COMPATIBLE` is used
- **THEN** compilation SHALL succeed

#### Scenario: Physical signature binding
- **GIVEN** a type and its expected Physical signature string
- **WHEN** `TYPELAYOUT_BIND_PHYSICAL` is used with matching signature
- **THEN** compilation SHALL succeed

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

