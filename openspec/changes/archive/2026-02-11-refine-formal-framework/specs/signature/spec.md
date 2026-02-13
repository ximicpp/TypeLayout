## MODIFIED Requirements

### Requirement: Formal Accuracy Guarantees
The library SHALL provide formally proven accuracy guarantees for both signature layers, using a rigorous framework based on denotational semantics and refinement theory. The formal model SHALL explicitly define the type domain, handle opaque types as a distinct category, and use precise terminology for equivalence relations.

#### Scenario: Layout signature soundness (no false positives)
- **GIVEN** two reflectable types T and U on the same platform P (excluding opaque-registered types)
- **WHEN** `layout_signatures_match<T, U>()` returns true
- **THEN** the layout denotation [T]_L SHALL equal [U]_L
- **AND** by the Encoding Faithfulness theorem (decode o [.]_L = L_P), L_P(T) SHALL equal L_P(U)
- **AND** the types SHALL be layout-compatible (T ~=_layout U)

#### Scenario: Opaque type formal boundary
- **GIVEN** a type T registered via TYPELAYOUT_OPAQUE_TYPE, TYPELAYOUT_OPAQUE_CONTAINER, or TYPELAYOUT_OPAQUE_MAP
- **WHEN** the formal model evaluates [T]_L or [T]_D
- **THEN** the Encoding Faithfulness theorem SHALL apply only under the Opaque Annotation Correctness axiom
- **AND** the formal model SHALL document that `decode` cannot recover internal structure for opaque types
- **AND** the Layout flattening function SHALL treat opaque types as leaf nodes (not recursed into)

#### Scenario: Explicit type domain definition
- **GIVEN** the formal proof system in PROOFS.md
- **THEN** the type domain Types_P SHALL be explicitly defined as the set of complete C++ types excluding void, unbounded arrays T[], and bare function types
- **AND** the opaque predicate opaque(T) SHALL be formally defined

#### Scenario: Layout signature conservativeness (intentional false negatives)
- **GIVEN** two types T and U that have identical byte layout (e.g., `int32_t[3]` and `struct{int32_t x,y,z;}`)
- **WHEN** their signatures are compared
- **THEN** the signatures MAY differ despite identical byte layout
- **AND** ker([.]_L) is a subset of the byte-equivalence relation (the equivalence kernel is strictly smaller than byte identity)

#### Scenario: Compiler-verified offset correctness
- **GIVEN** any struct or class type T with nesting depth d
- **WHEN** Layout or Definition signature is generated
- **THEN** all field offsets SHALL be proven correct by structural induction on d
- **AND** the base case (d=0) uses P2996 `offset_of` directly
- **AND** the inductive step accumulates OffsetAdj correctly

#### Scenario: Definition-to-Layout projection correctness
- **GIVEN** two types T and U
- **WHEN** `definition_signatures_match<T, U>()` returns true
- **THEN** a flatten-determinism lemma SHALL establish that D_P(T) = D_P(U) implies fields_P(T) = fields_P(U)
- **AND** ker([.]_D) SHALL be a proper subset of ker([.]_L) (strict refinement)

#### Scenario: Per-category accuracy classification
- **GIVEN** the library supports multiple type categories
- **THEN** correctness SHALL be proven by structural induction on the type AST grammar
- **AND** the grammar SHALL define 8 constructors: Primitive, Record, Record+bases, PolyRecord, Array, Union, Enum, BitField

#### Scenario: Signature grammar unambiguity
- **GIVEN** the signature string follows a deterministic grammar
- **THEN** each valid signature string SHALL have exactly one parse tree
- **AND** a constructive decode function (pseudocode recursive descent parser) SHALL be provided
- **AND** the proof SHALL be labeled as a proof sketch where left-factoring is informal

#### Scenario: CV-qualification erasure
- **GIVEN** a type T with const or volatile qualifiers
- **WHEN** a signature is generated
- **THEN** [const T] = [volatile T] = [const volatile T] = [T]

#### Scenario: Formal methodology
- **GIVEN** the proof system for signature correctness
- **THEN** it SHALL be based on denotational semantics (signatures as denotations of types)
- **AND** it SHALL use refinement theory for the V3 projection relationship
- **AND** it SHALL use structural induction for per-category verification
- **AND** it SHALL reference established formal verification methodologies (CompCert semantic preservation, seL4 refinement)

#### Scenario: Equivalence relation naming precision
- **GIVEN** the formal model defines multiple equivalence relations
- **THEN** "layout-compatible" (L_P equality) SHALL be distinguished from "byte-equivalent" (raw memory identity)
- **AND** the term "memcmp-compatible" SHALL NOT be used for L_P equality since L_P includes type-kind information beyond byte identity
