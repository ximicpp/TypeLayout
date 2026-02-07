## MODIFIED Requirements
### Requirement: Type Categories
The library SHALL support the following type categories in signatures.

#### Scenario: Fundamental types
- **GIVEN** fixed-width integers, floats, chars, bool
- **WHEN** signature is generated
- **THEN** each SHALL produce a deterministic signature with `[s:SIZE,a:ALIGN]`

#### Scenario: Record types (struct/class)
- **GIVEN** a struct or class type
- **WHEN** signature is generated
- **THEN** the prefix SHALL be `record`

#### Scenario: Enum types
- **GIVEN** an enum type with underlying type U
- **WHEN** Layout signature is generated
- **THEN** the format SHALL be `enum[s:SIZE,a:ALIGN]<underlying_sig>`
- **WHEN** Definition signature is generated
- **THEN** the format SHALL be `enum<QualifiedName>[s:SIZE,a:ALIGN]<underlying_sig>`

#### Scenario: Union types
- **GIVEN** a union type
- **WHEN** signature is generated
- **THEN** the prefix SHALL be `union` with all members at their offsets

#### Scenario: Array types
- **GIVEN** a fixed-size array `T[N]`
- **WHEN** signature is generated
- **THEN** the format SHALL be `array[s:SIZE,a:ALIGN]<element_sig,N>`
- **AND** byte arrays (char[], uint8_t[], std::byte[]) SHALL normalize to `bytes[s:N,a:1]`

#### Scenario: Bit-field types
- **GIVEN** a struct with bit-fields
- **WHEN** signature is generated
- **THEN** the format SHALL be `@BYTE.BIT:bits<WIDTH,underlying_sig>` (Layout)
- **OR** `@BYTE.BIT[NAME]:bits<WIDTH,underlying_sig>` (Definition)

#### Scenario: Anonymous members
- **GIVEN** a struct with anonymous members
- **WHEN** Definition signature is generated
- **THEN** anonymous members SHALL use `<anon:N>` placeholder

#### Scenario: Function pointer types
- **GIVEN** any function pointer type including noexcept and variadic variants
- **WHEN** signature is generated
- **THEN** all variants SHALL produce `fnptr[s:SIZE,a:ALIGN]`
