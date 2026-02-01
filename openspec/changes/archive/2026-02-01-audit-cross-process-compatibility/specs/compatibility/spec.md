## ADDED Requirements

### Requirement: Cross-Process Compatibility Guarantee

The library SHALL provide guarantees for cross-process type compatibility verification, covering the following dimensions:

1. **Architecture Identification**: Pointer size (32/64-bit) and byte order (LE/BE)
2. **ABI Identification**: Data model specific differences (LP64/LLP64/ILP32)
3. **Type Layout**: Field offsets, sizes, alignments, and padding
4. **Platform-Specific Types**: `long`, `wchar_t`, `long double` size variations

#### Scenario: Same architecture, same binary
- **WHEN** two processes on the same machine use the same struct definition
- **THEN** `get_layout_hash<T>()` returns identical values
- **AND** data can be safely shared via shared memory

#### Scenario: Same architecture, different struct version
- **WHEN** two processes have different versions of a struct (field added/removed)
- **THEN** `get_layout_hash<T>()` returns different values
- **AND** the mismatch is detected before data corruption

#### Scenario: Different architecture (64-bit vs 32-bit)
- **WHEN** a 64-bit process and 32-bit process compare the same struct
- **THEN** the architecture prefix differs (`[64-le]` vs `[32-le]`)
- **AND** compatibility check fails early

#### Scenario: Cross-platform network communication
- **WHEN** Windows (LLP64) and Linux (LP64) exchange data over network
- **THEN** types containing `long` produce different signatures
- **AND** only fixed-width types (`int32_t`, `uint64_t`) produce compatible signatures

### Requirement: Portable Type Validation

The `Portable<T>` concept SHALL correctly identify types safe for cross-process/cross-machine transmission:

1. **Excluded Types**: Pointers, references, `long`, `wchar_t`, `long double`
2. **Included Types**: Fixed-width integers, `float`, `double`, `char`, arrays of portable types, structs of portable types

#### Scenario: Portable concept rejects long
- **WHEN** `Portable<long>` is evaluated
- **THEN** it evaluates to `false`
- **AND** the type is correctly identified as non-portable across LP64/LLP64

#### Scenario: Portable concept accepts fixed-width types
- **WHEN** `Portable<int32_t>` is evaluated
- **THEN** it evaluates to `true`
- **AND** the type is guaranteed to have identical layout on all platforms

#### Scenario: Portable concept checks nested types
- **WHEN** a struct contains a `long` field
- **THEN** `Portable<MyStruct>` evaluates to `false`
- **AND** the non-portable field is correctly detected

### Requirement: Signature Completeness

The layout signature SHALL contain sufficient information to detect all incompatibilities:

1. **Field Information**: Offset, name, type signature
2. **Type Information**: Size, alignment
3. **Structural Information**: Inheritance, polymorphism markers
4. **Platform Information**: Architecture prefix with pointer size and endianness

#### Scenario: Field order change detection
- **WHEN** two structs have same fields but different order
- **THEN** signatures differ due to different field offsets
- **AND** incompatibility is detected

#### Scenario: Alignment difference detection
- **WHEN** same struct is compiled with different `#pragma pack`
- **THEN** signatures differ due to different alignments and offsets
- **AND** incompatibility is detected
