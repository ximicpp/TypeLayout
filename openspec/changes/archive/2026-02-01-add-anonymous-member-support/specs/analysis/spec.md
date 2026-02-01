## ADDED Requirements

### Requirement: Anonymous Member Detection
The library SHALL detect anonymous members (anonymous unions, anonymous structs) before calling `identifier_of()`.

#### Scenario: Anonymous union member detection
- **GIVEN** a struct containing an anonymous union
- **WHEN** generating the layout signature
- **THEN** the system detects the anonymous member and does not call `identifier_of()`

#### Scenario: Anonymous struct member detection
- **GIVEN** a class containing an anonymous struct
- **WHEN** generating the layout signature
- **THEN** the system detects the anonymous member and handles it appropriately

### Requirement: Anonymous Member Placeholder Names
The library SHALL generate unique placeholder names for anonymous members in the format `<anon:N>` where N is the member index.

#### Scenario: Single anonymous union
- **GIVEN** `struct S { int x; union { int a; float b; }; int y; }`
- **WHEN** generating the layout signature
- **THEN** the anonymous union is represented as `@offset[<anon:1>]:union[...]`

#### Scenario: Multiple anonymous members
- **GIVEN** a struct with multiple anonymous unions at different offsets
- **WHEN** generating the layout signature
- **THEN** each anonymous member gets a unique placeholder: `<anon:0>`, `<anon:1>`, etc.

### Requirement: std::optional Layout Signature
The library SHALL successfully generate layout signatures for `std::optional<T>` types.

#### Scenario: optional with primitive type
- **GIVEN** `std::optional<int>`
- **WHEN** calling `get_layout_signature<std::optional<int>>()`
- **THEN** a valid layout signature is returned including size and alignment

#### Scenario: optional with user-defined type
- **GIVEN** `std::optional<MyStruct>`
- **WHEN** calling `get_layout_signature<std::optional<MyStruct>>()`
- **THEN** a valid layout signature is returned

### Requirement: std::variant Layout Signature
The library SHALL successfully generate layout signatures for `std::variant<Ts...>` types.

#### Scenario: variant with multiple types
- **GIVEN** `std::variant<int, float, std::string>`
- **WHEN** calling `get_layout_signature<std::variant<int, float, std::string>>()`
- **THEN** a valid layout signature is returned including size and alignment

## ADDED Requirements

### Requirement: Field Signature Generation
The library SHALL generate field signatures for all non-static data members including anonymous members.

#### Scenario: Named member signature
- **GIVEN** a struct with named member `int x` at offset 0
- **WHEN** generating field signature
- **THEN** the output is `@0[x]:i32[s:4,a:4]`

#### Scenario: Anonymous member signature
- **GIVEN** a struct with anonymous union at offset 4 (index 1)
- **WHEN** generating field signature
- **THEN** the output is `@4[<anon:1>]:union[s:N,a:M]`

#### Scenario: Bit-field in anonymous union
- **GIVEN** an anonymous union containing bit-fields
- **WHEN** generating field signature
- **THEN** bit-fields are correctly represented with bit offset and width
