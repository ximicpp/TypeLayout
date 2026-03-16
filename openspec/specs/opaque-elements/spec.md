## ADDED Requirements

### Requirement: opaque_elements_safe default trait

TypeLayout SHALL provide a default trait `opaque_elements_safe<T>` in `fwd.hpp` that returns `false` for any type without an explicit specialization.

#### Scenario: Unspecialized type

- **WHEN** `T` has no `opaque_elements_safe` specialization (including hand-written `TypeSignature` with `is_opaque=true`)
- **THEN** `opaque_elements_safe<T>::value` SHALL be `false`

### Requirement: REGISTER_OPAQUE generates opaque_elements_safe true

The `TYPELAYOUT_REGISTER_OPAQUE` macro SHALL generate an `opaque_elements_safe<Type>` specialization that inherits from `std::true_type`. The type has no element types; it is a self-contained trivially_copyable blob.

#### Scenario: Registered opaque type elements safe

- **WHEN** `Type` is registered via `TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, HasPointer)`
- **THEN** `opaque_elements_safe<Type>::value` SHALL be `true`

### Requirement: TYPE_RELOCATABLE generates opaque_elements_safe true

The `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE` macro SHALL generate an `opaque_elements_safe<Type>` specialization that inherits from `std::true_type`. The type has no element types.

#### Scenario: Relocatable type elements safe

- **WHEN** `Type` is registered via `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE(Type, name)`
- **THEN** `opaque_elements_safe<Type>::value` SHALL be `true`

### Requirement: CONTAINER_RELOCATABLE generates recursive opaque_elements_safe

The `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE` macro SHALL generate an `opaque_elements_safe<Template<T_>>` specialization that returns `is_byte_copy_safe_v<T_>`, recursively checking the element type.

#### Scenario: Container with safe element type

- **WHEN** `Template<T_>` is registered via `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE` and `is_byte_copy_safe_v<T_>` is `true`
- **THEN** `opaque_elements_safe<Template<T_>>::value` SHALL be `true`

#### Scenario: Container with unsafe element type

- **WHEN** `Template<T_>` is registered via `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE` and `T_` contains pointers (e.g., `T_ = SomeStruct` with a `int*` member)
- **THEN** `opaque_elements_safe<Template<T_>>::value` SHALL be `false`

#### Scenario: Nested container

- **WHEN** `Template<Template<U>>` is instantiated and `is_byte_copy_safe_v<Template<U>>` is `true` (because `U` is safe)
- **THEN** `opaque_elements_safe<Template<Template<U>>>::value` SHALL be `true`

### Requirement: MAP_RELOCATABLE generates recursive opaque_elements_safe

The `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE` macro SHALL generate an `opaque_elements_safe<Template<K_, V_>>` specialization that returns `is_byte_copy_safe_v<K_> && is_byte_copy_safe_v<V_>`, recursively checking both key and value types.

#### Scenario: Map with safe key and value types

- **WHEN** `Template<K_, V_>` is registered via `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE` and both `is_byte_copy_safe_v<K_>` and `is_byte_copy_safe_v<V_>` are `true`
- **THEN** `opaque_elements_safe<Template<K_, V_>>::value` SHALL be `true`

#### Scenario: Map with unsafe value type

- **WHEN** `Template<K_, V_>` is registered and `K_` is safe but `V_` contains pointers
- **THEN** `opaque_elements_safe<Template<K_, V_>>::value` SHALL be `false`

## MODIFIED Requirements

### Requirement: TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE macro

The `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(Template, name)` macro SHALL generate both a `TypeSignature<Template<T_>>` specialization (existing behavior, unchanged) AND an `opaque_elements_safe<Template<T_>>` specialization (new behavior).

Existing signature generation behavior SHALL remain identical. The only addition is the `opaque_elements_safe` specialization.

#### Scenario: Signature generation unchanged

- **WHEN** `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(XVector, "xvec")` is invoked
- **THEN** `TypeSignature<XVector<T_>>::calculate()` SHALL produce the same signature as before this change

#### Scenario: opaque_elements_safe generated

- **WHEN** `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(XVector, "xvec")` is invoked
- **THEN** `opaque_elements_safe<XVector<T_>>` SHALL be a valid specialization returning `is_byte_copy_safe_v<T_>`

### Requirement: TYPELAYOUT_OPAQUE_MAP_RELOCATABLE macro

The `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(Template, name)` macro SHALL generate both a `TypeSignature<Template<K_, V_>>` specialization (existing behavior, unchanged) AND an `opaque_elements_safe<Template<K_, V_>>` specialization (new behavior).

Existing signature generation behavior SHALL remain identical. The only addition is the `opaque_elements_safe` specialization.

#### Scenario: Signature generation unchanged

- **WHEN** `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(XMap, "xmap")` is invoked
- **THEN** `TypeSignature<XMap<K_, V_>>::calculate()` SHALL produce the same signature as before this change

#### Scenario: opaque_elements_safe generated

- **WHEN** `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(XMap, "xmap")` is invoked
- **THEN** `opaque_elements_safe<XMap<K_, V_>>` SHALL be a valid specialization returning `is_byte_copy_safe_v<K_> && is_byte_copy_safe_v<V_>`