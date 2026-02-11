## ADDED Requirements
### Requirement: Opaque Type Signature Registration
The library SHALL provide macro-based registration for types whose internal layout is opaque (not introspectable via P2996 reflection), generating fixed-size/alignment signatures without requiring member-level reflection.

#### Scenario: Non-template opaque type
- **WHEN** a user registers a non-template type via `TYPELAYOUT_OPAQUE_TYPE(Type, name, size, align)`
- **THEN** the generated signature SHALL be `name[s:size,a:align]`
- **AND** the signature SHALL be identical for both Layout and Definition modes

#### Scenario: Single-parameter template container
- **WHEN** a user registers a single-type-parameter template via `TYPELAYOUT_OPAQUE_CONTAINER(Template, name, size, align)`
- **THEN** the generated signature SHALL be `name[s:size,a:align]<element_signature>`
- **AND** the element signature SHALL be recursively computed via `TypeSignature<T, Mode>::calculate()`

#### Scenario: Two-parameter template map
- **WHEN** a user registers a two-type-parameter template via `TYPELAYOUT_OPAQUE_MAP(Template, name, size, align)`
- **THEN** the generated signature SHALL be `name[s:size,a:align]<key_signature,value_signature>`
- **AND** both key and value signatures SHALL be recursively computed

### Requirement: Fixed Enum Detection
The library SHALL provide a compile-time function `is_fixed_enum<T>()` that determines whether an enum type has a fixed underlying type suitable for cross-process binary transfer.

#### Scenario: Scoped enum detection
- **GIVEN** a scoped enum (`enum class E : uint32_t { ... }`)
- **WHEN** `is_fixed_enum<E>()` is evaluated
- **THEN** the result SHALL be `true`

#### Scenario: Unscoped enum with explicit underlying type
- **GIVEN** an unscoped enum with explicit type (`enum E : int { ... }`)
- **WHEN** `is_fixed_enum<E>()` is evaluated
- **THEN** the result SHALL be `true`

#### Scenario: Non-enum type rejection
- **GIVEN** a non-enum type T
- **WHEN** `is_fixed_enum<T>()` is instantiated
- **THEN** compilation SHALL fail with a static_assert message