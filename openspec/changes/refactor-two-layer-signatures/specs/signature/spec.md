## REMOVED Requirements

### Requirement: Signature Mode Configuration
**Reason**: 三模式系统（Physical/Structural/Annotated）被两层系统（Layout/Definition）取代
**Migration**: `Physical` → `Layout`；`Structural` + `Annotated` → `Definition`

### Requirement: Physical Signature Generation
**Reason**: 被 Layout Signature 取代（功能等价，仅命名变更）
**Migration**: `get_physical_signature<T>()` → `get_layout_signature<T>()`

### Requirement: Physical Signature Comparison
**Reason**: 被 Layout Signature Comparison 取代
**Migration**: `physical_signatures_match<T, U>()` → `layout_signatures_match<T, U>()`

### Requirement: Physical Hash Functions
**Reason**: 被 Layout Hash Functions 取代
**Migration**: `get_physical_hash<T>()` → `get_layout_hash<T>()`

### Requirement: Physical Layout Concepts
**Reason**: 被新的两层 Concepts 取代
**Migration**: `PhysicalLayoutCompatible` → `LayoutCompatible`

### Requirement: Physical Verification
**Reason**: 被 Layout Verification 取代
**Migration**: `get_physical_verification<T>()` → `get_layout_verification<T>()`

### Requirement: Physical Assertion Macros
**Reason**: 被新的两层宏取代
**Migration**: `TYPELAYOUT_ASSERT_PHYSICAL_COMPATIBLE` → `TYPELAYOUT_ASSERT_LAYOUT_COMPATIBLE`

---

## ADDED Requirements

### Requirement: Two-Layer Signature Mode

The library SHALL provide a `SignatureMode` enumeration with exactly two values:

1. **Layout** — Pure byte layout signature that flattens inheritance hierarchy, uses `record` prefix, and excludes all C++ object model markers and names
2. **Definition** — Complete type definition signature that preserves inheritance tree structure, includes field names and base class names, uses `record` prefix, and includes `polymorphic` marker where applicable

#### Scenario: Layout mode produces flattened signatures
- **GIVEN** `struct Base { int id; }; struct Derived : Base { double value; };`
- **WHEN** Layout signature of `Derived` is generated
- **THEN** it SHALL be `[64-le]record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}`
- **AND** it SHALL NOT contain field names, base class names, or structural markers

#### Scenario: Definition mode preserves type structure with names
- **GIVEN** the same `Base` and `Derived` types
- **WHEN** Definition signature of `Derived` is generated
- **THEN** it SHALL be `[64-le]record[s:16,a:8]{~base<Base>:record[s:4,a:4]{@0[id]:i32[s:4,a:4]},@8[value]:f64[s:8,a:8]}`
- **AND** it SHALL contain field names in `[name]` brackets
- **AND** it SHALL contain base class names in `<Name>` after `~base`

#### Scenario: Definition mode does not include outer type name
- **GIVEN** any class type T
- **WHEN** Definition signature is generated
- **THEN** the signature SHALL use `record[s:N,a:M]{...}` prefix
- **AND** it SHALL NOT include the type name of T itself (e.g., NOT `record<T>[...]`)

---

### Requirement: Unified Record Prefix

The library SHALL use the `record` prefix uniformly for all class/struct types in both Layout and Definition signatures. The library SHALL NOT distinguish between `struct` and `class` keywords.

#### Scenario: No struct/class distinction
- **GIVEN** a type declared with `struct` keyword and another with `class` keyword, both having identical members
- **WHEN** their signatures are generated in either mode
- **THEN** both SHALL use `record` prefix (not `struct` or `class`)

---

### Requirement: Layout Signature Generation

The library SHALL provide `get_layout_signature<T>()` that generates a pure byte layout signature.

Layout signatures SHALL:
1. Use `record` prefix for all class types
2. Flatten all non-virtual base class fields into the top-level field list with absolute offsets
3. NOT include field names, base class names, or any structural markers
4. NOT include `polymorphic` or `inherited` markers
5. Skip virtual bases during flattening (v1 limitation)
6. Normalize byte arrays to `bytes[s:N,a:1]`
7. Order fields by byte offset

#### Scenario: Derived and flat types produce identical Layout signatures
- **GIVEN** `struct Base { int x; }; struct Derived : Base { double y; };` and `struct Flat { int x; double y; };`
- **WHEN** their Layout signatures are compared
- **THEN** they SHALL be identical

#### Scenario: Multi-level inheritance flattening
- **GIVEN** `struct A { int x; }; struct B : A { int y; }; struct C : B { int z; };`
- **WHEN** Layout signature of C is generated
- **THEN** all fields SHALL appear flattened at absolute offsets

---

### Requirement: Definition Signature Generation

The library SHALL provide `get_definition_signature<T>()` that generates a complete type definition signature.

Definition signatures SHALL:
1. Use `record` prefix for all class types
2. Include field names as `@OFF[name]:TYPE`
3. Preserve base class subobjects as `~base<Name>:record[...]{...}` subtrees
4. Preserve virtual base class subobjects as `~vbase<Name>:record[...]{...}`
5. Include `polymorphic` marker for types with virtual functions
6. NOT include `inherited` marker (presence of `~base` implies inheritance)
7. NOT include the outer type's own name
8. Use `<anon:N>` for anonymous members
9. Normalize byte arrays to `bytes[s:N,a:1]`
10. Obtain base class names via P2996 `identifier_of` (short name, no namespace)

#### Scenario: Polymorphic type
- **GIVEN** a class with virtual functions
- **WHEN** Definition signature is generated
- **THEN** it SHALL include `polymorphic` in the metadata markers
- **AND** the `polymorphic` marker SHALL appear inside `record[s:N,a:M,polymorphic]{...}`

#### Scenario: Non-polymorphic inheritance
- **GIVEN** `struct Base { int x; }; struct Derived : Base { double y; };`
- **WHEN** Definition signature of Derived is generated
- **THEN** it SHALL NOT contain `inherited` marker
- **AND** it SHALL contain `~base<Base>:record[...]{...}`

#### Scenario: Virtual base class
- **GIVEN** `struct A { int x; }; struct B : virtual A { int y; };`
- **WHEN** Definition signature of B is generated
- **THEN** virtual base SHALL appear as `~vbase<A>:record[...]{...}`

---

### Requirement: Projection Invariant

The library SHALL guarantee the mathematical projection relationship between Definition and Layout signatures:

- Definition signatures match ⟹ Layout signatures match
- Layout signatures match does NOT imply Definition signatures match

#### Scenario: Definition match implies Layout match
- **GIVEN** two types T and U where `definition_signatures_match<T, U>()` is true
- **THEN** `layout_signatures_match<T, U>()` SHALL also be true

#### Scenario: Layout match does not imply Definition match
- **GIVEN** `struct Base { int x; }; struct D : Base { double y; };` and `struct F { int x; double y; };`
- **THEN** `layout_signatures_match<D, F>()` SHALL be true
- **AND** `definition_signatures_match<D, F>()` SHALL be false

---

### Requirement: Layout Signature Comparison and Hashing

The library SHALL provide:
1. `layout_signatures_match<T, U>()` — Compare Layout signatures
2. `get_layout_hash<T>()` — 64-bit FNV-1a hash of Layout signature
3. `layout_hashes_match<T, U>()` — Compare Layout hashes

#### Scenario: Hash consistency
- **GIVEN** two types with identical Layout signatures
- **WHEN** their Layout hashes are compared
- **THEN** they SHALL match

---

### Requirement: Definition Signature Comparison and Hashing

The library SHALL provide:
1. `definition_signatures_match<T, U>()` — Compare Definition signatures
2. `get_definition_hash<T>()` — 64-bit FNV-1a hash of Definition signature
3. `definition_hashes_match<T, U>()` — Compare Definition hashes

#### Scenario: Field name affects Definition match
- **GIVEN** `struct A { int x; double y; };` and `struct B { int a; double b; };`
- **WHEN** their Definition signatures are compared
- **THEN** they SHALL NOT match (different field names)
- **BUT** their Layout signatures SHALL match (same byte layout)

---

### Requirement: Two-Layer Concepts

The library SHALL provide:
1. `LayoutSupported<T>` — Type can be analyzed
2. `LayoutCompatible<T, U>` — Layout signatures match
3. `DefinitionCompatible<T, U>` — Definition signatures match
4. `LayoutHashCompatible<T, U>` — Layout hashes match
5. `DefinitionHashCompatible<T, U>` — Definition hashes match

#### Scenario: Concept-constrained function
- **GIVEN** `template<LayoutCompatible<Source> Dest> void transfer(Dest&, const Source&);`
- **WHEN** called with types having matching Layout signatures
- **THEN** compilation SHALL succeed

---

### Requirement: Two-Layer Verification

The library SHALL provide dual-hash verification for both layers:
1. `get_layout_verification<T>()` — FNV-1a + DJB2 + length for Layout
2. `get_definition_verification<T>()` — FNV-1a + DJB2 + length for Definition
3. `layout_verifications_match<T, U>()`
4. `definition_verifications_match<T, U>()`

#### Scenario: Dual-hash collision resistance
- **GIVEN** two types with different layouts
- **WHEN** their verification results are compared
- **THEN** a mismatch SHALL be detected even if one hash algorithm collides

---

### Requirement: Two-Layer Assertion Macros

The library SHALL provide:
1. `TYPELAYOUT_ASSERT_LAYOUT_COMPATIBLE(T1, T2)` — Static assertion for Layout compatibility
2. `TYPELAYOUT_ASSERT_DEFINITION_COMPATIBLE(T1, T2)` — Static assertion for Definition compatibility
3. `TYPELAYOUT_BIND_LAYOUT(Type, Sig)` — Bind type to expected Layout signature
4. `TYPELAYOUT_BIND_DEFINITION(Type, Sig)` — Bind type to expected Definition signature

#### Scenario: Layout assertion succeeds
- **GIVEN** two types with matching byte layout but different definitions
- **WHEN** `TYPELAYOUT_ASSERT_LAYOUT_COMPATIBLE` is used
- **THEN** compilation SHALL succeed

#### Scenario: Definition assertion fails
- **GIVEN** two types with matching byte layout but different definitions
- **WHEN** `TYPELAYOUT_ASSERT_DEFINITION_COMPATIBLE` is used
- **THEN** compilation SHALL fail with a descriptive error

## MODIFIED Requirements

### Requirement: Layout Signature Architecture
The library SHALL provide layout signature generation for compile-time memory layout analysis with support for large structs.

The library provides two layers of signatures:
- **Layout Signature**: Pure byte-level layout (flattened, no names)
- **Definition Signature**: Complete type definition tree (with names, inheritance structure)

Both layers support structs with up to 200 members.

#### Scenario: Layout Compatibility
- **GIVEN** a user needs same-process or same-platform type checking
- **WHEN** using `get_layout_signature<T>()` for byte-level comparison
- **OR** using `get_definition_signature<T>()` for definition-level comparison
- **THEN** the signature SHALL reflect the appropriate level of detail
- **AND** platform SHALL be implicitly the current build platform
- **AND** structs with up to 200 members SHALL be supported

#### Scenario: Scalability
- **GIVEN** a struct with many members
- **WHEN** generating layout or definition signature
- **THEN** the implementation SHALL scale to handle large member counts
- **AND** constexpr step limits SHALL be managed through chunking

### Requirement: Layout Hash Generation
The library SHALL provide 64-bit hash generation for both Layout and Definition signatures.

#### Scenario: Layout hash computation
- **GIVEN** a type T
- **WHEN** calling `get_layout_hash<T>()`
- **THEN** the result SHALL be a 64-bit FNV-1a hash of the Layout signature

#### Scenario: Definition hash computation
- **GIVEN** a type T
- **WHEN** calling `get_definition_hash<T>()`
- **THEN** the result SHALL be a 64-bit FNV-1a hash of the Definition signature

#### Scenario: Hash determinism
- **GIVEN** two types T and U with identical byte layouts (regardless of names or structure)
- **WHEN** computing Layout hashes
- **THEN** `get_layout_hash<T>() == get_layout_hash<U>()` SHALL be true

### Requirement: Signature Comparison
The library SHALL provide compile-time signature comparison utilities for both layers.

#### Scenario: Layout signature match
- **GIVEN** two types T and U
- **WHEN** calling `layout_signatures_match<T, U>()`
- **THEN** the result SHALL be true if and only if their Layout signatures are identical

#### Scenario: Definition signature match
- **GIVEN** two types T and U
- **WHEN** calling `definition_signatures_match<T, U>()`
- **THEN** the result SHALL be true if and only if their Definition signatures are identical

### Requirement: Layout Concepts
The library SHALL provide C++20 concepts for compile-time layout validation using the two-layer model.

#### Scenario: LayoutSupported concept
- **GIVEN** a type T
- **WHEN** evaluating `LayoutSupported<T>`
- **THEN** the result SHALL be true if T can have its layout signature computed
- **AND** the result SHALL be false for void, function types, and incomplete types

#### Scenario: LayoutCompatible concept
- **GIVEN** two types T and U
- **WHEN** evaluating `LayoutCompatible<T, U>`
- **THEN** the result SHALL be true if `layout_signatures_match<T, U>()` is true

#### Scenario: DefinitionCompatible concept
- **GIVEN** two types T and U
- **WHEN** evaluating `DefinitionCompatible<T, U>`
- **THEN** the result SHALL be true if `definition_signatures_match<T, U>()` is true

### Requirement: Type Categories
The library SHALL use `record` prefix for all class/struct types in both signature layers.

#### Scenario: Record type
- **GIVEN** any class or struct type (with or without base classes)
- **WHEN** generating signature in either layer
- **THEN** the category SHALL be `record`
- **AND** format SHALL be `record[s:SIZE,a:ALIGN]{FIELDS}` (Layout) or `record[s:SIZE,a:ALIGN,MARKERS]{FIELDS}` (Definition)

#### Scenario: Polymorphic type in Definition mode
- **GIVEN** a class with virtual functions
- **WHEN** generating Definition signature
- **THEN** the category SHALL be `record`
- **AND** `polymorphic` marker SHALL be present in metadata

#### Scenario: Polymorphic type in Layout mode
- **GIVEN** a class with virtual functions
- **WHEN** generating Layout signature
- **THEN** the category SHALL be `record`
- **AND** NO `polymorphic` marker SHALL be present

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
The library SHALL include field information appropriate to each signature layer.

#### Scenario: Layout mode field format
- **GIVEN** a record with fields
- **WHEN** generating Layout signature
- **THEN** each field SHALL use format `@OFFSET:TYPE`
- **AND** fields SHALL be ordered by offset
- **AND** member names SHALL NOT be included

#### Scenario: Definition mode field format
- **GIVEN** a record with fields
- **WHEN** generating Definition signature
- **THEN** each field SHALL use format `@OFFSET[NAME]:TYPE`
- **AND** NAME SHALL be the member identifier

#### Scenario: Definition mode base class format
- **GIVEN** a record with base classes
- **WHEN** generating Definition signature
- **THEN** non-virtual bases SHALL use `~base<NAME>:record[...]{...}`
- **AND** virtual bases SHALL use `~vbase<NAME>:record[...]{...}`
- **AND** NAME SHALL be the base class short name (via `identifier_of`)

#### Scenario: Bit-field format
- **GIVEN** a record with bit-fields
- **WHEN** generating signature
- **THEN** bit-fields SHALL use format `@BYTE.BIT:bits<WIDTH,UNDERLYING>` (Layout)
- **OR** `@BYTE.BIT[NAME]:bits<WIDTH,UNDERLYING>` (Definition)

#### Scenario: Anonymous members
- **GIVEN** a record with anonymous union or struct members
- **WHEN** generating Definition signature
- **THEN** anonymous members SHALL use `<anon:N>` placeholder
