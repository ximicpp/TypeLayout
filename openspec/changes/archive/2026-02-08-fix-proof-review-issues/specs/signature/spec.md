## MODIFIED Requirements

### Requirement: Formal Accuracy Guarantees
The library SHALL provide formally proven accuracy guarantees for both signature layers.

#### Scenario: Layout signature soundness (no false positives)
- **GIVEN** two types T and U on the same platform P
- **WHEN** `layout_signatures_match<T, U>()` returns true
- **THEN** sizeof(T) SHALL equal sizeof(U)
- **AND** alignof(T) SHALL equal alignof(U)
- **AND** every leaf field at byte offset `o_i` in T SHALL have the same primitive type signature as the field at byte offset `o_i` in U
- **AND** the types SHALL be memcpy-compatible (identical byte layout)
- **NOTE** This is the contrapositive of the injectivity theorem: different layouts produce different signatures

#### Scenario: Layout signature conservativeness (intentional false negatives)
- **GIVEN** two types T and U that have identical byte layout (e.g., `int32_t[3]` and `struct{int32_t x,y,z;}`)
- **WHEN** their signatures are compared
- **THEN** the signatures MAY differ despite identical byte layout
- **AND** this is an intentional design choice to preserve semantic type boundaries (array vs struct)
- **NOTE** The signature function is a refinement of layout identity, not an equivalence

#### Scenario: Compiler-verified offset correctness
- **GIVEN** any struct or class type T
- **WHEN** Layout or Definition signature is generated
- **THEN** all field offsets SHALL be obtained from `std::meta::offset_of()` (P2996 compiler intrinsic)
- **AND** no offset SHALL be manually calculated or assumed
- **AND** recursive flattening SHALL correctly accumulate absolute offsets via the OffsetAdj parameter

#### Scenario: Definition-to-Layout projection correctness
- **GIVEN** two types T and U
- **WHEN** `definition_signatures_match<T, U>()` returns true
- **THEN** `layout_signatures_match<T, U>()` SHALL also return true
- **AND** there SHALL exist a deterministic erasure function π that maps any Definition signature to its corresponding Layout signature
- **AND** π SHALL be well-defined: each transformation step (name removal, inheritance flattening, marker replacement, qualified name removal) is a deterministic syntactic operation
- **AND** the Definition signature's equivalence kernel SHALL be a strict subset of the Layout signature's equivalence kernel

#### Scenario: Per-category accuracy classification
- **GIVEN** the library supports multiple type categories
- **THEN** the documentation SHALL classify accuracy for each category:
  - Fixed-width scalars: exact on all platforms
  - Platform-dependent scalars (long, wchar_t): exact per-platform, signatures naturally differ cross-platform
  - POD structs, inheritance, arrays, unions, enums: exact
  - Polymorphic types: existence marking (vptr presence, not vtable layout); vptr space is included in sizeof but not enumerated as a leaf field
  - Bit-fields: exact within same compiler, impl-defined across compilers

#### Scenario: Signature grammar unambiguity
- **GIVEN** the signature string follows a deterministic grammar with unique prefix keywords and balanced delimiters
- **THEN** each valid signature string SHALL have exactly one parse tree
- **AND** the encoding SHALL be injective on strings: different data produces different strings

#### Scenario: CV-qualification erasure
- **GIVEN** a type T with const or volatile qualifiers
- **WHEN** a signature is generated for const T, volatile T, or const volatile T
- **THEN** the signature SHALL be identical to the signature of the unqualified type T
- **AND** this is correct because CV qualifiers do not affect memory layout
