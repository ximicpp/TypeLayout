## MODIFIED Requirements

### Requirement: Formal Accuracy Guarantees
The library SHALL provide formally proven accuracy guarantees for both signature layers, using a rigorous framework based on denotational semantics and refinement theory.

#### Scenario: Layout signature soundness (no false positives)
- **GIVEN** two types T and U on the same platform P
- **WHEN** `layout_signatures_match<T, U>()` returns true
- **THEN** the layout denotation ⟦T⟧_L SHALL equal ⟦U⟧_L
- **AND** by the Encoding Faithfulness theorem (decode ∘ ⟦·⟧_L = L_P), L_P(T) SHALL equal L_P(U)
- **AND** the types SHALL be memcmp-compatible (T ≅_mem U)

#### Scenario: Layout signature conservativeness (intentional false negatives)
- **GIVEN** two types T and U that have identical byte layout (e.g., `int32_t[3]` and `struct{int32_t x,y,z;}`)
- **WHEN** their signatures are compared
- **THEN** the signatures MAY differ despite identical byte layout
- **AND** ker(⟦·⟧_L) ⊂ ≅_byte (the equivalence kernel is strictly smaller than byte identity)

#### Scenario: Compiler-verified offset correctness
- **GIVEN** any struct or class type T with nesting depth d
- **WHEN** Layout or Definition signature is generated
- **THEN** all field offsets SHALL be proven correct by structural induction on d
- **AND** the base case (d=0) uses P2996 `offset_of` directly
- **AND** the inductive step accumulates OffsetAdj correctly

#### Scenario: Definition-to-Layout projection correctness
- **GIVEN** two types T and U
- **WHEN** `definition_signatures_match<T, U>()` returns true
- **THEN** there SHALL exist a well-defined erasure function π with four deterministic steps
- **AND** π(⟦T⟧_D) = ⟦T⟧_L SHALL hold for all T (Lemma 4.1.2)
- **AND** ker(⟦·⟧_D) ⊊ ker(⟦·⟧_L) (strict refinement)

#### Scenario: Per-category accuracy classification
- **GIVEN** the library supports multiple type categories
- **THEN** correctness SHALL be proven by structural induction on the type AST grammar
- **AND** the grammar SHALL define 8 constructors: Primitive, Record, Record+bases, PolyRecord, Array, Union, Enum, BitField

#### Scenario: Signature grammar unambiguity
- **GIVEN** the signature string follows a deterministic grammar
- **THEN** each valid signature string SHALL have exactly one parse tree
- **AND** a decode function SHALL exist as the inverse of the encoding

#### Scenario: CV-qualification erasure
- **GIVEN** a type T with const or volatile qualifiers
- **WHEN** a signature is generated
- **THEN** ⟦const T⟧ = ⟦volatile T⟧ = ⟦const volatile T⟧ = ⟦T⟧

#### Scenario: Formal methodology
- **GIVEN** the proof system for signature correctness
- **THEN** it SHALL be based on denotational semantics (signatures as denotations of types)
- **AND** it SHALL use refinement theory for the V3 projection relationship
- **AND** it SHALL use structural induction for per-category verification
- **AND** it SHALL reference established formal verification methodologies (CompCert semantic preservation, seL4 refinement)
