## ADDED Requirements

### Requirement: is_byte_copy_safe compile-time predicate

TypeLayout SHALL provide a compile-time predicate `is_byte_copy_safe<T>` (and convenience variable `is_byte_copy_safe_v<T>`) that determines whether type T is safe for byte-level copying (memcpy) given all registered opaque type guarantees.

The predicate SHALL be `consteval` and require P2996 reflection. It SHALL be placed in a new header `include/boost/typelayout/admission.hpp`.

#### Scenario: Trivially copyable type without pointers

- **WHEN** `T` is `trivially_copyable` and contains no pointers
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `true`

#### Scenario: Trivially copyable type with pointers

- **WHEN** `T` is `trivially_copyable` but contains a pointer member
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `false`

#### Scenario: Struct containing only registered relocatable opaque members

- **WHEN** `T` is a non-trivially-copyable struct whose every member and base is either `is_byte_copy_safe` or a registered relocatable opaque type with `pointer_free = true` and `opaque_elements_safe = true`
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `true`

#### Scenario: Struct containing a non-safe member

- **WHEN** `T` is a struct that contains at least one member where `is_byte_copy_safe_v<MemberType>` is `false` (e.g., `std::string` which contains pointers)
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `false`

#### Scenario: Polymorphic type

- **WHEN** `T` is a polymorphic type (`std::is_polymorphic_v<T>` is `true`)
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `false`, regardless of member safety

#### Scenario: Union type -- trivially copyable without pointers

- **WHEN** `T` is a union type that is trivially_copyable and contains no pointers
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `true` (via `is_local_serialization_free` path)

#### Scenario: Union type -- non-trivially copyable

- **WHEN** `T` is a union type that is not trivially_copyable (e.g., contains an opaque member)
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `false`

#### Scenario: Opaque type with pointer

- **WHEN** `T` is a registered opaque type where `has_pointer` is `true` (i.e., `pointer_free = false`)
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `false`

#### Scenario: Opaque type without pointer, elements safe

- **WHEN** `T` is a registered opaque type where `has_pointer` is `false` and `opaque_elements_safe<T>::value` is `true`
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `true`

#### Scenario: Opaque type without pointer, elements not safe

- **WHEN** `T` is a registered opaque container where `has_pointer` is `false` but `opaque_elements_safe<T>::value` is `false` (e.g., element type contains pointers)
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `false`

#### Scenario: Array of safe elements

- **WHEN** `T` is a fixed-size array `U[N]` where `is_byte_copy_safe_v<U>` is `true`
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `true`

#### Scenario: Array of unsafe elements

- **WHEN** `T` is a fixed-size array `U[N]` where `is_byte_copy_safe_v<U>` is `false`
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `false`

#### Scenario: Nested struct recursion

- **WHEN** `T` is a struct containing a member of struct type `S`, and `S` itself contains only safe members
- **THEN** `is_byte_copy_safe_v<T>` SHALL recursively verify `S` and return `true`

#### Scenario: Inherited bases

- **WHEN** `T` inherits from base class `B`, and `B` contains an unsafe member
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `false`

### Requirement: is_byte_copy_safe subsumes is_local_serialization_free

`is_byte_copy_safe_v<T>` SHALL always return `true` when `is_local_serialization_free_v<T>` is `true`. The reverse SHALL NOT be guaranteed.

#### Scenario: Subsumption holds

- **WHEN** `is_local_serialization_free_v<T>` is `true`
- **THEN** `is_byte_copy_safe_v<T>` MUST also be `true`

#### Scenario: Broader acceptance

- **WHEN** a struct contains only registered relocatable opaque members (non-trivially-copyable but byte-copy safe)
- **THEN** `is_byte_copy_safe_v<T>` SHALL return `true` while `is_local_serialization_free_v<T>` returns `false`

### Requirement: is_local_serialization_free unchanged

`is_local_serialization_free_v<T>` SHALL NOT change its behavior or semantics. It SHALL remain `trivially_copyable(T) && !has_pointer(T)`.

#### Scenario: Existing behavior preserved

- **WHEN** any type `T` is checked with `is_local_serialization_free_v<T>`
- **THEN** the result SHALL be identical to the pre-change behavior

### Requirement: admission.hpp header

The `is_byte_copy_safe` predicate SHALL be defined in a new header file `include/boost/typelayout/admission.hpp`. This header SHALL be automatically included by the umbrella header `typelayout.hpp`.

#### Scenario: Header exists and is included

- **WHEN** user includes `<boost/typelayout/typelayout.hpp>`
- **THEN** `is_byte_copy_safe_v<T>` SHALL be available without additional includes

#### Scenario: Header can be included standalone

- **WHEN** user includes `<boost/typelayout/admission.hpp>` directly
- **THEN** all necessary dependencies SHALL be resolved via transitive includes