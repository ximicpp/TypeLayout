## 1. Review and Revision
- [x] 1.1 Review all sections and identify issues
- [x] 1.2 Apply fixes to paper files

---

## 2. Review Report

### 2.1 §1 Introduction — Issues Found

| # | Type | Issue | Fix |
|---|------|-------|-----|
| I1.1 | **Logic** | §1.1 says "the 7-line manual verification" but the code shows 7 lines (sizeof + alignof + 5 offsetof). The text in §1.2 says "7-line" which is correct. No issue. | — |
| I1.2 | **Consistency** | §1.2 uses `SenderPacket`/`ReceiverPacket` but §1.3 uses `sender::Message`/`receiver::Message`. Different names for the same concept across subsections is confusing. | Unify: §1.2 should use generic names since §1.3 provides the concrete example. Keep as is — §1.2 is abstract, §1.3 is concrete. Acceptable. |
| I1.3 | **Style** | §1.4 Contribution 2 lists "three key properties" but then lists exactly three sub-bullets. The intro to §4 also says "three core theorems" but §4 proves six results (Thm 4.8-4.15). | Fix §4 intro: say "six results" or "three core theorems with corollaries". |
| I1.4 | **Logic** | §1.4 Contribution 4 mentions "quantitative comparison with existing approaches (sizeof/offsetof asserts, ABI Compliance Checker, Boost.PFR)" — but §6 doesn't have quantitative measurements for PFR/ABI Checker comparison (only feature tables). The word "quantitative" overpromises. | Fix: change to "systematic comparison". |

### 2.2 §2 Background — Issues Found

| # | Type | Issue | Fix |
|---|------|-------|-----|
| I2.1 | **Error** | §2.1 says "For a struct, `alignof` is the maximum alignment of its members." This is slightly imprecise — it's the maximum alignment of members *and base classes*, and may be overridden by `alignas`. | Fix: add "and base classes" and note `alignas` override. |
| I2.2 | **Error** | §2.1 says "Members are laid out in declaration order." This is only guaranteed within the same access specifier in C++. Members in different access specifier groups may be reordered (implementation-defined). For standard-layout types, declaration order is guaranteed. | Fix: add caveat about access specifiers. |
| I2.3 | **Style** | §2.2 reference "[Childers et al. 2024]" — this is the P2996 proposal, not a published paper. Should be referenced as "P2996 [Childers et al. 2024]" with proper citation format. | Fix: ensure consistent citation style. |
| I2.4 | **Logic** | §2.3 comparison table marks Protobuf as "Compile-time: ✅" — but Protobuf verification happens at schema compilation/code generation time, not at C++ compile time. This is misleading. | Fix: change to "Code-gen time" or add footnote. |
| I2.5 | **Error** | §2.3 says "Boost.PFR ... (1) does not provide field offsets or sizes" — actually PFR does provide `boost::pfr::tuple_size_v<T>` (number of fields) and field types. It doesn't provide *offsets*. | Fix: be more precise about what PFR lacks. |

### 2.3 §3 System — Issues Found

| # | Type | Issue | Fix |
|---|------|-------|-----|
| I3.1 | **Consistency** | §3.2.1 grammar has `bits` as a production of `typesig`, but `bits` starts with `@NUM.NUM:...` which looks like a `field`, not a `typesig`. In the actual grammar (PROOFS.md), bit-fields are handled within `field` production, not `typesig`. | Fix: move `bits` from `typesig` to `field` alternative. |
| I3.2 | **Logic** | §3.3 states `π(⟦T⟧_D) = ⟦T⟧_L` — this is imprecise. The projection operates on *semantic domains* (D_P → L_P), not on *strings*. The correct statement is: there exists a string transformation π_str such that π_str(⟦T⟧_D) = ⟦T⟧_L, or equivalently, ⟦·⟧_L = π_str ∘ ⟦·⟧_D. | Fix: clarify that π operates on semantic domains and the string-level correspondence follows. |
| I3.3 | **Style** | §3.4 Algorithm 1 uses `FLATTEN` which returns a comma-separated string but doesn't show the comma-prefix logic. The actual implementation uses a comma-prefix + skip_first strategy. | Fix: add note about comma handling. |
| I3.4 | **Logic** | §3.4 point 1 says inheritance flattening ensures `Derived` and `Flat` produce "identical Layout signatures when their byte layouts are identical." This is correct but should note the assumption that the flattened field sequences must match (same types at same offsets). | Minor — acceptable as is. |

### 2.4 §4 Formal Semantics — Issues Found

| # | Type | Issue | Fix |
|---|------|-------|-----|
| I4.1 | **Error** | §4 intro says "three core theorems: Soundness, Encoding Faithfulness (Injectivity), and Strict Refinement (Projection)" but actually proves six results (Thm 4.8, Cor 4.9, Thm 4.10, 4.11, 4.12, 4.14, 4.15). | Fix: reword intro to match actual content. |
| I4.2 | **Logic** | Theorem 4.14 proof step (3): "if L_P(T) = L_P(U), then ... ⟦T⟧_L = ⟦U⟧_L" — this step actually requires the *surjectivity* of decode on Im(⟦·⟧_L), not the contrapositive of injectivity. The reasoning is: L_P(T) = L_P(U), and since ⟦·⟧_L is faithful (decode ∘ ⟦·⟧_L = L_P), we need L_P(T) = L_P(U) ⟹ ⟦T⟧_L = ⟦U⟧_L, which doesn't follow from injectivity alone — it requires that the encoding is *injective on L_P values*, i.e., L_P(T) = L_P(U) ⟹ ⟦T⟧_L = ⟦U⟧_L. This actually follows from the fact that ⟦·⟧_L is a function of L_P(T) (signature is determined by the layout). | Fix: clarify this proof step. |
| I4.3 | **Style** | Definition 4.13 defines π on D_P(T) → L_P(T) but uses T-specific notation. Should be π : StructureTree → ByteLayout. | Fix: use domain-level notation. |

### 2.5 §5 Toolchain — Issues Found

| # | Type | Issue | Fix |
|---|------|-------|-----|
| I5.1 | **Style** | §5.4 uses `check_compat` function name, but the actual API may differ. Should verify against implementation or note this is illustrative. | Fix: add "illustrative" note. |
| I5.2 | **Logic** | §5.6 says "both phases can be combined into a single compilation" when P2996 is widely available, but this is only true for same-platform comparison. Cross-platform comparison inherently requires separate compilations. | Fix: clarify this limitation. |

### 2.6 §6 Evaluation — Issues Found

| # | Type | Issue | Fix |
|---|------|-------|-----|
| I6.1 | **Error** | §6.3.3 compilation time data ("~1.2s", "~2.0s", "~4.5s") — these appear to be estimates, not actual measurements. For an academic paper, this should either be real data or clearly marked as estimates. | Fix: mark as "estimated" or "representative". |
| I6.2 | **Logic** | §6.4.1 marks Protobuf as "Compile-time: ✅" — same issue as I2.4. | Fix in sync with I2.4. |
| I6.3 | **Style** | §6.4.2 says "86% fewer lines" — the math: (7-1)/7 = 85.7%, rounds to 86%. Correct. | — |
| I6.4 | **Logic** | §6.5 Threats to Validity is good but missing one key threat: the formal proofs in §4 are "pen-and-paper" proofs, not mechanized. This should be explicitly acknowledged. | Fix: add threat about non-mechanized proofs. |

### 2.7 §7 Related Work — Issues Found

| # | Type | Issue | Fix |
|---|------|-------|-----|
| I7.1 | **Error** | §7.1 says "Boost.PFR [Polukhin 2018]" — Boost.PFR's first Boost release was 2020 (Boost 1.75). The 2018 date may refer to the original proposal. | Fix: verify and update date. |
| I7.2 | **Style** | §7.3 references "[Google 2008]" for Protocol Buffers — the first public release was 2008, but the academic paper is Varda 2008 or the Google Tech Report. Citation needs consistency. | Fix: standardize citation style. |
| I7.3 | **Logic** | §7.5 connects to Pierce 2002 (TAPL) on structural vs nominal typing. Good connection, but should cite the specific chapter/section for precision. | Minor improvement. |

### 2.8 §8-9 Discussion & Conclusion — Issues Found

| # | Type | Issue | Fix |
|---|------|-------|-----|
| I8.1 | **Consistency** | §8.1 says "Signature match is implication, not equivalence" — this is the same as Theorem 4.11 (Conservativeness). Should reference the theorem. | Fix: add cross-reference. |
| I8.2 | **Style** | §9 Conclusion repeats "86% fewer lines" from §6. Repetition is acceptable in conclusions but should be verified for consistency. | — |
| I8.3 | **Logic** | §8.4 Future Work mentions "Mechanized proofs" — this should be in sync with §6.5 threat about non-mechanized proofs. | Fix: cross-reference. |

---

## 3. Summary of Issues

| Category | Count | Critical |
|----------|-------|----------|
| **Error** (factual inaccuracy) | 5 | I2.1, I2.2, I2.5, I3.1, I6.1 |
| **Logic** (reasoning gap) | 7 | I1.3, I1.4, I2.4, I3.2, I4.2, I5.2, I6.4 |
| **Style** (expression quality) | 5 | I2.3, I3.3, I4.3, I5.1, I7.2 |
| **Consistency** (cross-section) | 3 | I1.2, I4.1, I8.1 |
| **Total** | **20** | |

### Priority Fixes (applied below)

The following high-impact fixes are applied to the paper files:

1. **I2.1+I2.2**: Fix imprecise C++ layout model description
2. **I2.4+I6.2**: Fix Protobuf "compile-time" mislabeling
3. **I4.1**: Fix §4 intro "three theorems" → match actual six results
4. **I4.2**: Fix Theorem 4.14 proof step (3) reasoning
5. **I1.4**: Change "quantitative comparison" → "systematic comparison"
6. **I3.1**: Fix grammar — move bits from typesig to field
7. **I6.1**: Mark compilation times as estimated
8. **I6.4**: Add non-mechanized proofs to Threats to Validity
9. **I5.2**: Clarify cross-platform limitation of single-compilation mode
