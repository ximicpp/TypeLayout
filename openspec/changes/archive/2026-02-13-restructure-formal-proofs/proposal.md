# Change: Restructure PROOFS.md with Layered Denotational Architecture

## Why

PROOFS.md has grown organically through multiple refinement rounds. While all
individual theorems are correct, the document's architecture has accumulated
structural debt: S1 overload (9 definitions + 2 grammars), grammar misplacement
(encoding property in type domain section), theorem scattering (Layout and
Definition Faithfulness separated by unrelated theorems), and Offset Correctness
placed with encoding theorems instead of structural verification.

## What Changes

Reorganized PROOFS.md from 6 sections to 7 sections following Layered Denotational
Semantics principle:

| New Section | Content | Key Change |
|-------------|---------|------------|
| S1 | Type Domain & Semantic Objects (Def 1.1-1.9) | Grammar removed |
| S2 | Encoding (Def 2.1-2.2 + Grammar 2.3-2.4) | Grammar moved from S1 |
| S3 | Encoding Properties (Thm 3.1-3.2) | Faithfulness theorems grouped |
| S4 | Safety Theorems (Thm 4.1-4.2) | Soundness/Conservativeness isolated |
| S5 | Refinement (Thm 5.4-5.5) | R_P promoted as architectural principle |
| S6 | Structural Verification (Thm 6.1 + 6.2-6.9) | Offsets + per-category merged |
| S7 | Summary & Reference (7.1-7.5) | Updated diagram and index |

All 9 theorems/corollaries preserved verbatim. No proofs changed. Cross-references
updated in PROOFS.md, APPLICATIONS.md, and openspec specs.

## Impact

- Affected docs: PROOFS.md, APPLICATIONS.md
- Affected specs: signature (theorem number references), documentation (lemma references)
- No code changes, no behavioral changes
