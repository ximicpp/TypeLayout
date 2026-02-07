# signature Specification

> **Layer**: Core (`<boost/typelayout.hpp>`)

## Purpose

Defines the two-layer signature generation system — the core capability of Boost.TypeLayout.
Signatures provide deterministic, human-readable descriptions of type memory layouts
for compile-time type identity verification.

## Requirements

### Requirement: Two-Layer Signature Architecture
The library SHALL provide two complementary layers of compile-time type signatures.

#### Scenario: Layout signature (Layer 1)
- **GIVEN** a type T
- **WHEN** calling `get_layout_signature<T>()`
- **THEN** the result SHALL encode size, alignment, architecture prefix, and all leaf fields at their absolute byte offsets
- **AND** inheritance SHALL be flattened (base class fields appear at absolute offsets)
- **AND** composition SHALL be flattened (nested struct fields are recursively expanded)
- **AND** field names SHALL NOT be included
- **AND** polymorphic types SHALL include a `,vptr` marker

#### Scenario: Definition signature (Layer 2)
- **GIVEN** a type T
- **WHEN** calling `get_definition_signature<T>()`
- **THEN** the result SHALL encode size, alignment, architecture prefix, field names, and type structure
- **AND** inheritance SHALL be preserved as `~base<QualifiedName>:record{...}`
- **AND** virtual bases SHALL be marked as `~vbase<QualifiedName>:record{...}`
- **AND** polymorphic types SHALL include a `,polymorphic` marker
- **AND** base class names SHALL use fully qualified names (namespace::Name)
- **AND** enum types SHALL include their fully qualified name

#### Scenario: Projection relationship
- **GIVEN** two types T and U
- **WHEN** `definition_signatures_match<T,U>()` returns true
- **THEN** `layout_signatures_match<T,U>()` SHALL also return true

### Requirement: Signature Comparison
The library SHALL provide compile-time signature comparison functions.

#### Scenario: Layout comparison
- **GIVEN** two types T and U
- **WHEN** calling `layout_signatures_match<T, U>()`
- **THEN** the result SHALL be true if and only if their Layout signatures are identical

#### Scenario: Definition comparison
- **GIVEN** two types T and U
- **WHEN** calling `definition_signatures_match<T, U>()`
- **THEN** the result SHALL be true if and only if their Definition signatures are identical

### Requirement: Layout Signature Flattening
The Layout layer SHALL flatten all structural hierarchy to pure byte identity.

#### Scenario: Inheritance flattening
- **GIVEN** `struct Base { int32_t x; }; struct Derived : Base { double y; };`
- **AND** `struct Flat { int32_t x; double y; };`
- **WHEN** Layout signatures are compared
- **THEN** `layout_signatures_match<Derived, Flat>()` SHALL return true

#### Scenario: Multi-level inheritance flattening
- **GIVEN** `A { int x; }; B : A { int y; }; C : B { int z; };`
- **AND** `Flat { int x; int y; int z; };`
- **WHEN** Layout signatures are compared
- **THEN** `layout_signatures_match<C, Flat>()` SHALL return true

#### Scenario: Composition flattening
- **GIVEN** `struct Inner { int a; int b; }; struct Composed { Inner x; };`
- **AND** `struct Flat { int a; int b; };`
- **WHEN** Layout signatures are compared
- **THEN** `layout_signatures_match<Composed, Flat>()` SHALL return true

#### Scenario: Virtual base inclusion
- **GIVEN** a type with virtual base classes
- **WHEN** Layout signature is generated
- **THEN** virtual base class fields SHALL be included at their correct offsets

### Requirement: Polymorphic Type Safety
The library SHALL distinguish polymorphic from non-polymorphic types in Layout signatures.

#### Scenario: vptr marker in Layout
- **GIVEN** a polymorphic type `struct P { virtual void f(); int x; };`
- **WHEN** Layout signature is generated
- **THEN** the signature SHALL contain `,vptr` marker
- **AND** the signature SHALL NOT match a non-polymorphic type with the same fields

### Requirement: Qualified Names in Definition
The Definition layer SHALL use fully qualified names for disambiguation.

#### Scenario: Base class qualified names
- **GIVEN** `namespace ns1 { struct Tag { int id; }; }` and `namespace ns2 { struct Tag { int id; }; }`
- **AND** `struct A : ns1::Tag {}; struct B : ns2::Tag {};`
- **WHEN** Definition signatures are compared
- **THEN** `definition_signatures_match<A, B>()` SHALL return false

#### Scenario: Enum qualified names
- **GIVEN** `namespace ns { enum class Color : uint8_t {}; enum class Shape : uint8_t {}; }`
- **WHEN** Definition signatures are compared
- **THEN** `definition_signatures_match<Color, Shape>()` SHALL return false
- **AND** `layout_signatures_match<Color, Shape>()` SHALL return true

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

### Requirement: Architecture Prefix
Every signature SHALL include an architecture prefix.

#### Scenario: 64-bit little-endian
- **WHEN** compiling on a 64-bit little-endian platform
- **THEN** signatures SHALL be prefixed with `[64-le]`

#### Scenario: Platform-dependent types
- **GIVEN** `long` or `wchar_t` types
- **WHEN** signature is generated
- **THEN** the signature SHALL reflect the actual size on the current platform

### Requirement: Empty Type Handling
The library SHALL correctly handle empty types.

#### Scenario: Empty base optimization
- **GIVEN** `struct Empty {}; struct WithEmpty : Empty { int x; double y; };`
- **AND** `struct Plain { int x; double y; };`
- **WHEN** Layout signatures are compared
- **THEN** `layout_signatures_match<WithEmpty, Plain>()` SHALL return true

### Requirement: Alignment Support
The library SHALL capture alignment information in signatures.

#### Scenario: Custom alignment
- **GIVEN** `struct alignas(16) Aligned { int a; int b; };`
- **WHEN** signature is generated
- **THEN** the signature SHALL reflect `a:16` alignment
