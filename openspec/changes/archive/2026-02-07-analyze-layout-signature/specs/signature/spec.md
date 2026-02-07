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
- **NOTE** The reverse does not hold: layout match does not imply definition match
