## MODIFIED Requirements
### Requirement: Layout Signature Flattening
The Layout engine SHALL recursively flatten non-opaque class-type fields
into a single offset-indexed list. However, when a class-type field has a
TypeSignature specialization with `is_opaque == true`, the engine MUST treat
it as a leaf node and emit the opaque signature directly at the field's offset
rather than descending into the type's data members and base classes.

#### Scenario: Normal class flattening
- **WHEN** a struct contains a field of a non-opaque class type
- **THEN** the Layout engine recursively flattens it into offset-indexed entries

#### Scenario: Opaque type as struct field
- **WHEN** a struct contains a field of an opaque-registered class type
- **THEN** the Layout signature includes the opaque descriptor (e.g., `xstring[s:32,a:1]`) as a leaf node at the correct offset

#### Scenario: Inheritance flattening
- **WHEN** a derived class has base classes
- **THEN** base class members are flattened into the derived class layout at their actual offsets

## ADDED Requirements
### Requirement: Opaque Signature Detection
The signature engine SHALL provide a compile-time concept `has_opaque_signature<T, Mode>`
that evaluates to true if and only if `TypeSignature<T, Mode>::is_opaque` exists and is true.
All TYPELAYOUT_OPAQUE_* macro expansions MUST define `static constexpr bool is_opaque = true`
in the generated TypeSignature specialization.

#### Scenario: Detection of opaque types
- **WHEN** a type is registered via TYPELAYOUT_OPAQUE_TYPE, TYPELAYOUT_OPAQUE_CONTAINER, or TYPELAYOUT_OPAQUE_MAP
- **THEN** `has_opaque_signature<Type, SignatureMode::Layout>` evaluates to true

#### Scenario: Non-opaque types
- **WHEN** a type has no opaque registration
- **THEN** `has_opaque_signature<Type, SignatureMode::Layout>` evaluates to false
