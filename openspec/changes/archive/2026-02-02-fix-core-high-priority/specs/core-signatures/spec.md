## ADDED Requirements

### Requirement: std::array Type Signature Support

The library SHALL provide explicit `TypeSignature` specialization for `std::array<T, N>`.

The signature format SHALL be: `std_array[s:SIZE,a:ALIGN]<ELEMENT_SIG,COUNT>`

#### Scenario: Basic std::array signature
- **WHEN** user requests signature for `std::array<int32_t, 4>`
- **THEN** the signature SHALL be `[64-le]std_array[s:16,a:4]<i32[s:4,a:4],4>`

#### Scenario: Nested std::array signature
- **WHEN** user requests signature for `std::array<std::array<int32_t, 2>, 3>`
- **THEN** the signature SHALL correctly represent nested array structure

### Requirement: std::pair Type Signature Support

The library SHALL provide explicit `TypeSignature` specialization for `std::pair<T1, T2>`.

The signature format SHALL be: `pair[s:SIZE,a:ALIGN]{FIRST_SIG,SECOND_SIG}`

#### Scenario: Basic std::pair signature
- **WHEN** user requests signature for `std::pair<int32_t, double>`
- **THEN** the signature SHALL include both element types with correct offsets

### Requirement: std::span Type Signature Support (C++20)

The library SHALL provide explicit `TypeSignature` specialization for `std::span<T, Extent>` when available.

#### Scenario: Static extent span
- **WHEN** user requests signature for `std::span<int32_t, 10>`
- **THEN** the signature SHALL reflect the static extent

#### Scenario: Dynamic extent span
- **WHEN** user requests signature for `std::span<int32_t>`
- **THEN** the signature SHALL indicate dynamic extent

## MODIFIED Requirements

### Requirement: Unsupported Type Diagnostics

The library SHALL provide clear and informative compile-time error messages when encountering unsupported types.

Error messages SHALL include:
1. The fact that the type is not supported
2. Suggested type categories that ARE supported
3. Guidance on how to add custom specializations

#### Scenario: User attempts to use unsupported type
- **WHEN** user calls `get_layout_signature<UnsupportedType>()`
- **THEN** the compiler SHALL emit a static_assert with helpful guidance message

### Requirement: Virtual Inheritance Documentation

The library documentation SHALL clearly state the limitations of virtual inheritance signatures.

Documentation SHALL explain:
1. Virtual base class offsets may be dynamic (vtable-dependent)
2. Signatures are for type identification, not ABI guarantees
3. Cross-compiler compatibility is not guaranteed for virtual inheritance

#### Scenario: User reads inheritance documentation
- **WHEN** user consults the inheritance user guide
- **THEN** they SHALL find explicit virtual inheritance limitations section
