## ADDED Requirements

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

## MODIFIED Requirements

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
