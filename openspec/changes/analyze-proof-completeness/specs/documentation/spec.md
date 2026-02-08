## ADDED Requirements

### Requirement: Formal Proof Completeness

The PROOFS.md document SHALL provide formally complete proofs covering all three Core Values (V1 Layout Reliability, V2 Definition Precision, V3 Layer Projection), with every proof chain grounded in explicit grammar definitions and structural induction.

#### Scenario: V2 proof chain is complete
- **GIVEN** the formal proofs document (PROOFS.md)
- **WHEN** verifying V2 (Definition Precision) coverage
- **THEN** the document SHALL include:
  - Definition 1.9 (Definition Signature Grammar) with full BNF productions
  - Lemma 1.9.1 (Definition grammar unambiguity) with LL(1) argument
  - Theorem 3.5 (Definition Encoding Faithfulness) with decode_D construction
  - Corollary 3.5.1 (Definition Soundness) as direct consequence

#### Scenario: Primitive type verification is exhaustive
- **GIVEN** the §5.1 verification table in PROOFS.md
- **WHEN** comparing against the implementation's type specializations
- **THEN** the table SHALL cover all primitive type categories: fixed-width integers, floating point, platform-dependent integers, character types, boolean, byte, nullptr, pointers, references, member pointers, and function pointers

#### Scenario: Implementation mechanical details are documented
- **GIVEN** the signature generation uses a comma-prefix accumulation strategy
- **WHEN** reading the denotation definitions in §2
- **THEN** the document SHALL include an implementation note explaining that `skip_first()` is a purely mechanical string transformation equivalent to `join(",", fields)` and does not affect encoding faithfulness
