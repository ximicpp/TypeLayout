## MODIFIED Requirements

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

---

## ADDED Requirements

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

---

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

---

### Requirement: Physical Hash Functions

The library SHALL provide:
1. `get_physical_hash<T>()` — Returns 64-bit FNV-1a hash of Physical signature
2. `physical_hashes_match<T1, T2>()` — Compares Physical hashes of two types

#### Scenario: Hash based on Physical signature
- **GIVEN** `Derived` and `Flat` with matching Physical signatures
- **WHEN** their Physical hashes are compared
- **THEN** they SHALL match

---

### Requirement: Physical Layout Concepts

The library SHALL provide:
1. `PhysicalLayoutCompatible<T, U>` — Concept satisfied when Physical signatures match
2. `PhysicalHashCompatible<T, U>` — Concept satisfied when Physical hashes match

#### Scenario: Concept constraints
- **GIVEN** a function template `void transfer(PhysicalLayoutCompatible<Source> auto& dest, const Source& src)`
- **WHEN** called with layout-compatible types
- **THEN** it SHALL compile successfully

---

### Requirement: Physical Verification

The library SHALL provide:
1. `get_physical_verification<T>()` — Returns dual-hash verification based on Physical signature
2. `physical_verifications_match<T1, T2>()` — Compares Physical verifications

#### Scenario: Dual-hash physical verification
- **GIVEN** two types with matching Physical layout
- **WHEN** their Physical verifications are compared
- **THEN** both FNV-1a and DJB2 hashes SHALL match

---

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
