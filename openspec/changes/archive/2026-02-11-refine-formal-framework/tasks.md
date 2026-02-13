## 1. Critical Fixes (Category A1-A3: Opaque type formal gap)
- [x] 1.1 Add Section 1.0 "Type Domain" definition to PROOFS.md (explicit exclusion of void, T[], function types; explicit opaque predicate)
- [x] 1.2 Add "Opaque Annotation Correctness" axiom to PROOFS.md Section 1
- [x] 1.3 Extend Definition 1.3 (flatten) to include `opaque(T)` short-circuit for fields and bases
- [x] 1.4 Qualify Theorem 3.1 universality: restrict to reflectable types or add opaque caveat
- [x] 1.5 Fix `opaque.hpp` stale theorem reference (Thm 4.8 -> Thm 3.1)

## 2. Proof Logic Fixes (Category B)
- [x] 2.1 Add Lemma: "flatten is deterministic given D_P(T)" to close Theorem 4.2 gap
- [x] 2.2 Rename Definition 1.7 from "memcmp-compatibility" to "layout-compatibility"
- [x] 2.3 Label Lemma 1.8.1 proof as "proof sketch" or formalize the left-factoring
- [x] 2.4 Add constructive decode function (pseudocode recursive descent parser)

## 3. Implementation Alignment (Category A4-A5)
- [x] 3.1 Fix `type_map.hpp`: change `sizeof(T*)` to `sizeof(T&)` in reference specializations
- [x] 3.2 Enumerate all 7 byte-element types explicitly in Definition 2.1 Case 4

## 4. Coverage Gaps (Category C)
- [x] 4.1 Add int/short alias explanation with `requires` deduplication rationale
- [x] 4.2 Add opaque grammar extension or explicit exclusion note (Section 1.8)
- [x] 4.3 Add function pointer variant coverage note (noexcept, variadic)
- [x] 4.4 Add formal exclusion of void, T[], function types in Type Domain
- [x] 4.5 Add comma-prefix/skip_first() equivalence lemma

## 5. Spec Update
- [x] 5.1 Update signature spec "Formal Accuracy Guarantees" requirement to reflect opaque caveat and naming fix

## 6. Verify
- [x] 6.1 Build and run tests (5/5 pass, signature output unchanged)
- [x] 6.2 Cross-check all theorem references in PROOFS.md and source comments