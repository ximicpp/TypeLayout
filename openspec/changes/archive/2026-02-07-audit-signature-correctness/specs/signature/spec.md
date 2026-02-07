## MODIFIED Requirements

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
- **AND** union members SHALL NOT be recursively flattened (each member retains its type signature as an atomic unit)

#### Scenario: Projection relationship
- **GIVEN** two types T and U
- **WHEN** `definition_signatures_match<T,U>()` returns true
- **THEN** `layout_signatures_match<T,U>()` SHALL also return true
- **NOTE** The reverse does not hold: layout match does not imply definition match

### Requirement: Layout Signature Flattening
The Layout layer SHALL flatten all structural hierarchy to pure byte identity.

#### Scenario: Union members not flattened
- **GIVEN** `struct Inner { int a; int b; }; union U { Inner x; double y; };`
- **WHEN** Layout signature is generated
- **THEN** union member `x` SHALL appear as its complete type signature (e.g., `record[s:8,a:4]{...}`) rather than being recursively flattened into individual fields

### Requirement: Type Categories
The library SHALL support the following type categories in signatures.

#### Scenario: Enum types
- **GIVEN** an enum type with underlying type U
- **WHEN** Layout signature is generated
- **THEN** the format SHALL be `enum[s:SIZE,a:ALIGN]<underlying_sig>`
- **WHEN** Definition signature is generated
- **THEN** the format SHALL be `enum<QualifiedName>[s:SIZE,a:ALIGN]<underlying_sig>`
