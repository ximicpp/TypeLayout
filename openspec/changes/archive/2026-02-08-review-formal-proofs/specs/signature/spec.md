## MODIFIED Requirements

### Requirement: Formal Accuracy Guarantees
The library SHALL provide formally proven accuracy guarantees for both signature layers, using a rigorous framework based on denotational semantics and refinement theory. All definitions, lemmas, and theorems SHALL be internally consistent, match the implementation, and cover all supported type categories.

#### Scenario: Complete denotation definition
- **GIVEN** the denotation functions ⟦·⟧_L and ⟦·⟧_D
- **THEN** they SHALL be defined as case-split recursive functions covering all 8 type constructors: Primitive, Record, Record+bases, PolyRecord, Array, Union, Enum, BitField
- **AND** no type constructor SHALL be left with only informal coverage in per-category sections

#### Scenario: Complete primitive type coverage
- **GIVEN** the primitive type signature function σ
- **THEN** it SHALL cover all types with explicit specializations in the implementation
- **AND** this SHALL include char, bool, wchar_t, char8_t, char16_t, char32_t, std::byte, std::nullptr_t, ref, rref, memptr, fnptr in addition to fixed-width integers and floats

#### Scenario: Correct conservativeness characterization
- **GIVEN** the conservativeness theorem
- **THEN** it SHALL distinguish between byte-level equivalence (≅_byte) and layout-identity equivalence (≅_mem = L_P equality)
- **AND** the counterexample SHALL demonstrate ∃ T,U: T ≅_byte U ∧ ⟦T⟧_L ≠ ⟦U⟧_L
- **AND** soundness SHALL be stated with respect to ≅_mem (the stronger relation)

#### Scenario: V3 projection without pure string erasure
- **GIVEN** the V3 projection theorem (⟦T⟧_D = ⟦U⟧_D ⟹ ⟦T⟧_L = ⟦U⟧_L)
- **THEN** the proof SHALL NOT rely on an erasure function π : Σ* → Σ* that requires out-of-band information (base offsets)
- **AND** it SHALL instead use a semantic argument based on shared P2996 reflection data

#### Scenario: Flatten function completeness
- **GIVEN** the flatten function defining leaf field sequences
- **THEN** it SHALL handle bit-field members as a distinct case
- **AND** it SHALL use the full signature function (not just σ) for non-class leaf fields including arrays, enums, and unions

#### Scenario: Layout signature soundness (no false positives)
- **GIVEN** two types T and U on the same platform P
- **WHEN** `layout_signatures_match<T, U>()` returns true
- **THEN** the layout denotation ⟦T⟧_L SHALL equal ⟦U⟧_L
- **AND** by the Encoding Faithfulness theorem (decode ∘ ⟦·⟧_L = L_P), L_P(T) SHALL equal L_P(U)
- **AND** the types SHALL be memcmp-compatible (T ≅_mem U)

#### Scenario: Formal methodology
- **GIVEN** the proof system for signature correctness
- **THEN** it SHALL be based on denotational semantics (signatures as denotations of types)
- **AND** it SHALL use refinement theory for the V3 projection relationship
- **AND** it SHALL use structural induction for per-category verification
- **AND** it SHALL reference established formal verification methodologies (CompCert semantic preservation, seL4 refinement)
