## 1. Analysis — Opaque Macros
- [x] 1.1 Analyze TYPELAYOUT_OPAQUE_TYPE design and correctness
  - [OK] Signature format correct: `name[s:size,a:align]`
  - (!) P1: No static_assert to verify size/align match sizeof/alignof
- [x] 1.2 Analyze TYPELAYOUT_OPAQUE_CONTAINER design and correctness
  - [OK] Element type recursion correct, Mode_ correctly forwarded
  - (!) P2: "constant size for all T" assumption not compiler-enforced
- [x] 1.3 Analyze TYPELAYOUT_OPAQUE_MAP design and correctness
  - [OK] Same structure as CONTAINER, dual type params correct
  - (!) Same P1/P2 risks
- [x] 1.4 Evaluate consistency with signature grammar and formal semantics
  - (!) P4: Grammar 3.2 needs new `opaque`/`opaque_tmpl` productions
  - (!) P7: Thm 4.8 (Encoding Faithfulness) now requires user-annotation correctness axiom
  - [OK] Thm 4.10 (Soundness) conditionally holds if size/align correct
  - [OK] Thm 4.14 (Projection) trivially holds (Layout == Definition for opaque)

## 2. Analysis — is_fixed_enum
- [x] 2.1 Analyze is_fixed_enum<T>() correctness and edge cases
  - [OK] Correct for scoped enums and explicitly-typed unscoped enums
  - (!) P3: Cannot distinguish explicit vs compiler-inferred underlying type (language limitation)
  - [OK] bool exclusion is reasonable
- [x] 2.2 Evaluate placement and API surface
  - (!) Placed in signature_detail.hpp but not used by any TypeSignature specialization
  - Consider: move to public API or mark as internal

## 3. Cross-Cutting Concerns
- [x] 3.1 Impact on formal proofs (4) -> P7: need Opaque Annotation Correctness axiom
- [x] 3.2 Impact on signature grammar (3) -> P4: need opaque/opaque_tmpl productions
- [x] 3.3 Missing test coverage -> P5: no tests for opaque macros or is_fixed_enum
- [x] 3.4 Documentation gaps -> P6: not in README, API docs, or paper

## 4. Summary and Recommendations
- [x] 4.1 Produce final analysis report

---

## Issue Summary

| # | Issue | Severity | Recommendation |
|---|-------|----------|----------------|
| P1 | ~~Opaque macros lack `static_assert(sizeof(Type)==size)` cross-validation~~ | [FIXED] | static_assert added in all 3 macros |
| P2 | ~~Constant-size assumption not compiler-enforced~~ | [FIXED] | Covered by P1 fix: static_assert inside calculate() fires per-instantiation |
| P3 | ~~`is_fixed_enum` can't distinguish explicit vs inferred underlying type~~ | [DOCUMENTED] | Language limitation; added detailed NOTE in signature_detail.hpp |
| P4 | Signature grammar not updated for opaque types | [DEFERRED] | Future paper update: add opaque/opaque_tmpl productions to 3.2 |
| P5 | ~~No test coverage for new code~~ | [FIXED] | Created test/test_opaque.cpp with 13 static_assert tests |
| P6 | ~~No documentation for new APIs~~ | [FIXED] | Added DESIGN NOTE to opaque.hpp with correctness boundary |
| P7 | ~~Encoding Faithfulness theorem premise changed~~ | [DOCUMENTED] | Opaque Annotation Correctness axiom documented in opaque.hpp header |