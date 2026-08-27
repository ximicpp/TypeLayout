# CppCon 2026 TypeLayout Deck Redesign

**Status:** Approved narrative design, synchronized with the deck implementation plan

**Date:** 2026-08-23

**Repository baseline:** `201f06f8a9dd20323ffd8af836c545ef2380e82d`

**Source deck:** `cppcon2026_typelayout_36slide_main_argument_map_editorial_no_audio_optimized.pptx`

**Talk title:** *Can I memcpy this type across a boundary?*

**Audience:** Experienced C++ developers attending CppCon 2026

**Format assumption:** 60-minute session, targeting 52–55 minutes of prepared content plus Q&A; the section-level estimates span 50–57 minutes depending on delivery pace

## 1. Communication Job

By the end of the talk, experienced C++ developers should be able to decide when native object bytes may cross a declared boundary, understand how C++26 reflection produces inspectable representation evidence, and recognize exactly where the resulting permit ends.

The central takeaway is:

> Native bytes are a controlled optimization inside a closed, continuously verified contract—not a universal wire format.

The deck educates through a cumulative proof. It is neither an ABI survey nor a TypeLayout API tour.

## 2. Scope and Explicit Non-Goals

The main argument uses one strict transfer profile:

- ordinary object copy;
- zero fixup;
- source-address-independent bytes;
- finite declared type and build sets;
- trusted producer objects;
- representation verification rather than semantic schema verification.

The published Sched abstract uses `architecture and endianness` as shorthand for representation-relevant target facts. At repository baseline `201f06f`, the signature's global `arch-prefix` explicitly encodes pointer width and endianness; leaf tokens, sizes, and alignments carry further representation facts, while exact compiler and target identity belong to CI provenance. The deck must explain this mapping once and must not claim that the signature prefix encodes a complete CPU architecture or ISA identity.

The main permit excludes:

- relocation of non-trivially-copyable objects;
- process-local pointers, references, member pointers, and function pointers;
- application semantics, units, enum meaning, handles, and invariants;
- open-ended platform portability;
- untrusted-input validation;
- runtime lifetime, storage, alignment, synchronization, framing, and crash consistency;
- deterministic canonical bytes when padding or multiple value representations are possible.

Relocation remains an appendix-only, separate contract. The deck must not imply that `is_byte_copy_safe_v<T>` alone grants ordinary `memcpy` permission.

## 3. Approved Seven-Claim Throughline

1. **Boundary**

   > Across a boundary, object representation stops being an implementation detail—it becomes a contract.

2. **Local checks are insufficient**

   > Local copyability is a one-build property, and equal `sizeof` is only a coarse observation; cross-build compatibility requires representation evidence from both builds.

3. **Signature generation**

   > On each build, C++26 reflection exposes the compiler’s layout; TypeLayout recursively normalizes the supported object representation into a deterministic compile-time signature—and fails closed when the required evidence is unavailable.

4. **Agreement**

   > For a registered boundary type, exact equality of canonical signatures establishes representation Agreement on one declared build edge.

5. **Admission**

   > Agreement is not permission: under the talk’s zero-fixup, source-address-independent profile, Structural Admission requires ordinary byte-copy legality, no detected source-context dependency, and complete representation evidence.

6. **Closed CI**

   > CI closes each registered type's claim over a finite build contract: every declared build emits evidence; CI accepts it only when it is fresh and attributable; and that type receives a Permit only when Admission passes on every node and Agreement is established on every required transfer edge.

7. **Operating boundary**

   > A representation permit is deliberately narrow: it approves native object representation inside one closed contract; the application still owns meaning, lifetime, storage, synchronization, and trust—otherwise serialize or convert.

## 4. Narrative Architecture

The approved narrative is proof-first and signature-centered:

```text
opening question
→ define the boundary and transfer profile
→ derive the two independent proof obligations
→ show why local checks cannot discharge them
→ generate canonical per-build evidence
→ establish Agreement on one build edge
→ establish Admission on both endpoint nodes
→ derive one edge-level decision rule
→ close the finite contract in CI
→ authorize one real raw-byte type set and reject two nearby alternatives
→ state the operating boundary
→ restate the problem, method, and decision rule as the final takeaway
```

The main deck contains 45 slides. The appendix contains 16 slides, for 61 slides total.

| Section | Main slides | Expected time | Communication job |
|---|---:|---:|---|
| Boundary and scope | 1–9 | 7–8 min | Define the question, two axes, three scenarios, and strict profile |
| Local checks | 10–11 | 2 min | Prove that local traits and total size cannot establish a relation |
| Signature engine | 12–19 | 12–14 min | Make the core technology trustworthy and inspectable |
| Agreement | 20–22 | 4–5 min | Define exact edge equality and its limits |
| Admission | 23–28 | 7–8 min | Define the profile-aware node predicate and structural limits |
| Closed CI | 29–34 | 7–8 min | Produce provenance-bound per-key decisions over the finite build graph |
| Apply | 35–39 | 5–6 min | Enable one useful raw-byte contract, reject one Admission failure and one Agreement failure, and resolve the opening example |
| Operating boundary | 40–42 | 3 min | State what the Permit proves, what remains application-owned, and when to serialize |
| Conclusion and takeaway | 43–45 | 3 min | Restate the problem, compress the complete method, and leave one actionable decision rule |

### 4.1 Narrative Layers and Detail Admission

The seven stages use three explicit information layers:

1. **Main causal spine** — the inference every audience member must retain;
2. **Required proof evidence** — the minimum technical detail needed to justify the current inference;
3. **Deferred detail** — implementation variants, edge cases, and grammar that belong in speaker notes or the appendix.

A detail stays in the main deck only when it does at least one of the following:

- closes a proof gap in the current claim;
- prevents a likely and consequential misunderstanding;
- enables the next inference in the cumulative argument.

Otherwise, defer it. Every stage follows the same speaking rhythm:

```text
Claim
→ Evidence
→ Limitation
→ Next question
```

This keeps the technical review constraints subordinate to the audience-facing argument. The main-deck flow is `why evidence is needed → how one build produces evidence → how the two gates decide one edge → how CI closes the finite contract → how that model enables one useful raw-byte contract → what the resulting Permit does and does not authorize → the complete decision rule the audience should retain`; it must not become a boundary catalog, signature grammar tour, Admission API inventory, provenance-field inventory, graph-optimization tutorial, or demo-implementation walkthrough.

Stages 1–7 preserve exactly two compatibility gates: Admission on build nodes and Agreement on build edges. CI provenance validates that evidence belongs to the declared producer and run; it is an input-validity condition, not a third compatibility gate.

### 4.2 Stage 1 — Why Evidence Is Needed (Slides 1–11)

**Communication job:** put the audience in the reviewer's chair and establish that a cross-boundary permit requires evidence that local checks do not provide.

The main causal spine is:

```text
native bytes cross a boundary
→ build identity, address-space identity, or both may be lost
→ select one strict transfer profile over a finite declared set
→ two independent proof obligations arise
→ local traits and total size cannot satisfy both
→ each build must describe what it actually produced
```

Keep in the main deck:

- one opening type and one unanswered `PERMIT?` decision;
- the build-identity × address-space-identity matrix;
- one compact example for each lost assumption and one combined stored-bytes case;
- the strict profile: ordinary copy, zero fixup, source-address-independent bytes, finite declared build set;
- the two natural-language questions: whether the bytes may stand independently and whether declared builds produce matching representation evidence;
- one `trivially_copyable` counterargument and one equal-`sizeof` counterexample.

The cumulative chain on slide 9 is a brief orientation beat, not a technical agenda. It may preview the remaining inference, but it must not explain downstream formulas, APIs, or CI machinery before the local-check gap has been established.

Compress or defer:

- exhaustive compiler/flag/header/ABI drift lists;
- operational details of IPC, plugins, and storage;
- versioning, trust, framing, lifetime, synchronization, and crash consistency beyond one boundary footer.

The stage exits with:

> `Each build must describe the representation it actually produced.`

The next question is: `How can one build produce trustworthy representation evidence?`

### 4.3 Stage 2 — How One Build Produces Evidence (Slides 12–19)

**Communication job:** establish a qualified per-build certificate, not merely display a generated string.

The stage contract is:

```text
Input:  ordinary C++ type T compiled by build B
Output: Signature_B(T) within the explicitly supported signature domain
Failure: compile-time rejection
```

The main causal spine is:

```text
define what a trustworthy signature requires
→ obtain the compiler's actual layout facts through reflection
→ recursively normalize the supported representation
→ encode exposed structure, name explicit trust, reject hidden unsupported machinery
→ assemble one inspectable consteval certificate
→ audit the certificate against the trust requirements
```

Introduce the four requirements before the implementation sequence, using audience-facing questions:

- **Coverage:** did we record every representation fact used by this proof?
- **Canonicality:** do the same supported facts produce the same normalized form?
- **Discrimination:** does every encoded difference change the certificate?
- **Fail-closed behavior:** if TypeLayout cannot completely encode a fact required by this proof, does signature generation fail instead of producing partial evidence?

Maintain these distinctions throughout the stage:

```text
compiler + reflection → layout facts
TypeLayout            → normalization policy
FixedString           → per-build certificate
later CI stages       → export, freshness, and provenance
```

Keep in the main deck:

- one declaration beside the compiler-produced byte map;
- only the reflection operations necessary to explain enumerate → type → position → classify → recurse;
- the category policy at one-action granularity;
- canonical leaf tokens, size/alignment, and root-relative absolute offsets;
- the Sched abstract's `architecture and endianness` wording mapped precisely to the implementation: the signature prefix records pointer width plus endianness, not CPU architecture identity, while exact target identity belongs to provenance;
- one compact encode / explicit-trust / reject policy;
- one concrete fail-closed rejection, with virtual inheritance preferred;
- one complete, decoded `PacketHeader` certificate.

Compress or defer:

- the complete primitive-token table and platform assumptions;
- full array/enum/union/bit-field/EBO/`no_unique_address` rules;
- opaque macro variants and relocation contracts;
- the complete signature grammar and recursive engine pseudocode.

Opaque is not presented as fully reflected structure: it is an explicitly named trust contract. The stage must not claim that the certificate describes every compiler fact or every C++ type. It certifies the encoded facts within the declared signature domain.

The stage exits with:

> `Each build now has its own inspectable representation certificate.`

The certificate supplies Agreement evidence. The same recursive inspection also supplies the per-build structural facts used later by Admission; the Admission policy itself remains in Stage 3.

The next question is: `What does exact equality between two certificates establish—and what does it not establish?`

### 4.4 Stage 3 — How the Two Gates Decide One Edge (Slides 20–28)

**Communication job:** distinguish a build-edge relation from a build-node predicate and derive the decision rule for one declared edge without claiming that the whole build set is already closed.

The audience model is:

```text
Build A node ───── Agreement edge ───── Build B node
  Admission                              Admission
```

The main causal spine is:

```text
contract-key equality + certificate equality
→ Agreement on one declared build edge
→ a matching-pointer counterexample exposes Agreement's limit
→ profile-aware structural Admission judges each build node
→ an integer-handle counterexample exposes Admission's semantic limit
→ Admission on both nodes + Agreement on the edge
→ that one declared edge satisfies the two-gate decision rule
```

Keep in the main deck:

- the registered-contract-key plus exact-signature Agreement predicate;
- one readable signature match and one readable divergence;
- one compact statement of the three Agreement limits: source identity, application meaning, and source-context independence;
- one staged pointer counterexample;
- the strict profile reminder and the three conceptual Admission conditions;
- the ordinary-copy composition `std::is_trivially_copyable_v<T> && is_byte_copy_safe_v<T>`;
- one compact integer-disguised-handle warning;
- the node/edge decision matrix and one-edge decision formula.

Compress or defer:

- exhaustive pointer-like-token lists;
- the full recursive Admission algorithm;
- opaque relocation API variants;
- parser, reporter, and exported-artifact implementation.

Use `NoDetectedStructuralContextDependency`, not an unqualified claim of context independence. `RepresentationEvidenceComplete` means every reachable component is encoded or covered by an explicitly named trust contract; it does not mean opaque internals were reflected. If a required component is unsupported, Stage 2 rejects signature generation, so Admission and `EdgePass` cannot be established. Agreement is equality of encoded representation certificates within the declared signature domain, not a proof of source identity or semantics.

For one contract key and one build edge, define:

```text
EdgePass_P(K,A,B)
= Admission_P(K,A)
  and Admission_P(K,B)
  and Agreement(K,A,B)
```

`EdgePass` is a scoped decision predicate under the assumption that both input artifacts are valid and correctly attributed. It is not the final per-key Permit over the complete declared build graph.

The stage exits with:

> `One declared edge can satisfy both gates. That does not yet close the declared build set.`

The next question is: `How does CI close the claim over every declared type, build node, and required edge?`

### 4.5 Stage 4 — How CI Closes the Finite Contract (Slides 29–34)

**Communication job:** quantify the Stage 3 node and edge predicates for every registered contract key over one explicitly declared finite build contract, while establishing that every input artifact belongs to the build it claims to describe.

The stage contract is:

```text
Input:  C = (R,V,E,P) plus evidence emitted by every declared build
Output: one closed PERMIT / REJECT decision for every K in R
Failure: CI verification cannot complete and issues no permit when required evidence is missing or invalid
```

Here `R` is the registered contract-key set, `V` is the exact participating build set, `E` is the required transfer-edge set, and `P` is the transfer profile. The set is finite and explicit; this stage makes no claim about an unnamed compiler, target, configuration, or future build.

The main causal spine is:

```text
one edge-level decision does not cover the declared build set
→ declare the finite contract C = (R,V,E,P)
→ every actual build independently emits its own evidence
→ CI binds each artifact to its exact producer and run
→ for each K, check Admission on every declared build node
→ for each K, establish Agreement on every required transfer edge
→ any missing, invalid, or failing fact prevents a Permit for that K
→ all required facts for K pass, so ClosedPermit_C(K) is established
→ repeat until every K in R has a closed decision
```

Keep in the main deck:

- the four parts of the finite contract, preferably as a small visual rather than a set-theory detour;
- one build graph in which every node visibly emits its own signature and Admission-related evidence;
- the distinction `artifact says what was observed; CI establishes who produced it and when`;
- provenance explicitly framed as evidence validation before the two gates, not as a third gate;
- Admission on every declared node and Agreement on every required transfer edge;
- the per-key closed-contract formula;
- one missing-required-build example showing that an absent fact never counts as a pass.

Compress or defer:

- the complete `TypeEntry` and platform-metadata field lists;
- the full provenance envelope and the choice between `.sig.hpp` fields and an external attestation;
- equality transitivity, spanning-tree comparison reduction, and other graph optimizations;
- the full diagnostic taxonomy for missing, stale, unattributable, Admission-failing, and Agreement-failing evidence;
- reporter APIs and convenience-macro discussion.

Under the precondition that all required evidence is present, fresh, and attributable, define:

```text
ClosedPermit_C(K)
= Admission_P(K,B) for every B in V
  and Agreement(K,A,B) for every (A,B) in E
```

Evidence validity is not conjoined as a third compatibility predicate: invalid evidence prevents CI from evaluating the two-gate claim in the first place. In the main narrative, every missing, stale, unattributable, or failing required fact has the same operational result for the affected key: CI verification does not issue a Permit. A project may additionally require every `K` in `R` to receive a Permit before the overall workflow passes, but that aggregate policy is distinct from the per-type Permit shown in the examples and belongs in notes or the appendix.

The stage exits with:

> `Every declared type receives a closed decision over the exact build set; only types that pass every required node and edge receive a Permit.`

The next question is: `Can this closed model authorize a useful raw-byte type set—and reject nearby designs that violate either gate?`

### 4.6 Stage 5 — Apply the Model to a Real Raw-Byte Contract (Slides 35–39)

**Communication job:** turn the closed-decision model into one recognizable engineering outcome: a coherent set of native C++ types may cross a declared persistent-storage boundary without field-by-field serialization, while two plausible nearby designs are rejected for independent reasons.

The stage contract is:

```text
Input:  the closed two-gate model plus one completed portable-capture demonstration
Output: four per-key Permits, one Admission rejection, and one Agreement rejection
```

The positive example is a fixed-size telemetry capture block written and read by any build in the declared set:

```text
R_capture = {
  PacketHeader,
  MeasurementSample,
  CaptureTrailer,
  CaptureBlock
}

V = {
  Linux x86-64 / GCC 16,
  Linux x86-64 / Clang P2996,
  Apple ARM64 / Clang P2996
}

E = all three pairwise build edges; either endpoint may write or read
P = ordinary object copy, zero fixup, source-address-independent bytes

C_capture = (R_capture, V, E, P)
```

The three displayed build names are audience labels for the exact build identities already established in Stage 4. `PacketHeader`, `MeasurementSample`, and `CaptureTrailer` each occupy 16 bytes in the demo; `CaptureBlock` contains the header, four samples, and the trailer. The boundary model uses only fixed-width integer leaves, fixed extents, and nested records.

The main causal spine is:

```text
the closed-decision rule is still abstract
→ instantiate one persistent-storage boundary and declare C_capture
→ the complete run establishes four independent per-key Permits for R_capture
→ ClosedPermit_C(CaptureBlock) authorizes native object representation for the whole-block raw-I/O path
→ adding a cached pointer yields Agreement MATCH but Admission FAIL
→ replacing the fixed-width value with long double yields Admission PASS but Agreement DIFFER
→ one working contract and the two candidate failures demonstrate why both gates are necessary
```

Here “without serialization” has a deliberately narrow meaning: the demonstrated path copies the complete object representation with `memcpy` or an equivalent raw binary write/read; it performs no per-field encoding, endian conversion, fixup, or semantic transformation. It is authorized only for the declared types and builds. It is not a universal file format and does not establish schema evolution, semantic meaning, runtime validity, or support for an undeclared ABI.

Keep in the main deck:

- one recorder build → capture file/persistent bytes → later analyzer build scenario;
- the four positive boundary types as one coherent capture-block composition;
- the exact three-build set and the fact that all writer/reader edges are required;
- one compact result: every positive key passes every required node and edge;
- one whole-object raw write/read line as the practical consequence of `ClosedPermit_C(CaptureBlock)`;
- `UnsafeWithPointer` as a nearby in-memory convenience type rejected by Admission;
- `Measurement { uint64_t id; long double value; }` as a nearby high-precision type rejected by Agreement;
- one final matrix that contrasts the permitted set with the two independent failure modes.

Compress or defer:

- complete declarations, generated signatures, CMake targets, artifact filenames, and CI commands;
- deterministic fixture construction, byte dumps, hashes, and field-by-field round-trip assertions;
- provenance-manifest fields and artifact-retention mechanics;
- padding analysis beyond stating that the demo deliberately chooses padding-free positive layouts;
- alternative sample counts, framing formats, checksums, and domain-specific telemetry semantics.

The positive production set and the negative fixtures have different roles. `R_capture` contains only the four intended native-byte boundary types. For a proposed candidate key `K`, define `C_candidate(K) = (R_capture ∪ {K}, V, E, P)`. `UnsafeWithPointer` and `Measurement` are checked separately as candidates under that same `V`, `E`, and `P`; their closed decisions fail, so neither enters the production allowlist. Demo CI succeeds because the four positive decisions and both exact negative-result shapes are observed.

The stage exits with:

> `Inside C_capture, CaptureBlock may use native bytes as its representation layer; both non-conforming candidates stay off that path.`

The next question is: `What exactly does that representation Permit authorize—and what remains the application's responsibility?`

### 4.7 Stage 6 — Bound the Permit (Slides 40–42)

**Communication job:** prevent the Stage 5 Permit from being overgeneralized and identify when the native-byte path is no longer the right tool.

The main causal spine is:

```text
Stage 5 establishes a representation Permit inside one closed contract
→ the Permit proves representation compatibility, not complete I/O correctness
→ lifetime, storage, synchronization, and failure handling remain application obligations
→ semantic compatibility, schema evolution, or a broader build set requires explicit conversion
```

Keep this stage short. Slide 40 separates what the Permit proves from what it does not prove. Slide 41 names only the runtime obligations needed to prevent unsafe overreach. Slide 42 presents the decision boundary between the closed native-byte path and serialization.

Do not reopen signature generation, CI mechanics, or demo internals. This stage introduces no new gate and no new technical mechanism; it only bounds the conclusion already established. It must not carry the burden of the final recap.

The stage exits with:

> `A representation Permit is useful only within its declared operating boundary.`

The next question is: `What problem did we solve, how did we solve it, and what rule should the audience carry into its next design review?`

### 4.8 Stage 7 — Summarize the Problem, Method, and Takeaway (Slides 43–45)

**Communication job:** restore the positive result after the limitations, reconnect it to the opening question, and leave the audience with a complete but compact mental model it can use in the next design review.

The main causal spine is:

```text
return to “Can I memcpy this type across a boundary?”
→ reveal that the question is incomplete until the boundary contract is named
→ declare C and K, then restate the two independent obligations: transportable bytes and matching representations
→ every declared build evaluates Admission and emits a reflection-derived representation signature
→ CI accepts only complete, fresh, attributable evidence inputs
→ Admission on V plus Agreement on E closes the claim for K over C
→ state the reusable rule: Permit native bytes only inside C; convert explicitly when the required contract is broader
```

Use three slides because the conclusion has three distinct jobs:

- **Slide 43 — problem recap:** distinguish the local operation question from the cross-boundary contract question, and reconnect the opening `Measurement` result to the missing contract dimensions.
- **Slide 44 — method recap:** show one uninterrupted chain from declared `C` and `K` through per-build Admission/signature evidence, Agreement, and CI closure to `ClosedPermit_C(K)` or rejection.
- **Slide 45 — actionable takeaway:** give the design-review checklist and final operating rule.

This is a rhetorical summary, not another proof stage. Do not repeat signature grammar, demo layouts, ABI numbers, reporter output, or CI artifact mechanics. Each item in the recap must point back to an inference already established in Stages 1–6.

The talk exits with:

> `Permit native bytes only inside a closed contract; convert explicitly when the required contract is broader.`

## 5. Main Deck: Slide-by-Slide Design

Each slide has one narrative job and one primary claim. Titles are audience-facing assertions, not topic labels.

### 1. Can I memcpy this type across a boundary?

- Preserve the existing minimal title treatment and byte-strip visual.
- Subtitle: `Verifying native-byte compatibility at compile time with C++26 reflection`.
- Do not add agenda items or a library feature inventory.

### 2. Would you approve these bytes across every declared build?

- Show `Measurement { uint64_t id; long double value; }` and the local `trivially_copyable` assertion.
- Show four reassuring local facts: trivially copyable, no pointers, no ownership, no virtual functions.
- End with `PERMIT?`; do not answer.
- Spoken transition: “The difficult word is not `memcpy`. It is `boundary`.”

### 3. Across a boundary, object representation becomes a contract

- Visible thesis: claim 1.
- Define a boundary as any case where the consumer cannot assume the producer’s build identity or address space.
- Show one producer → native bytes → consumer line.
- Move from “implementation detail” on the producer side to “contractual evidence” at the boundary.

### 4. Build identity and address space are independent assumptions

- Show a 2×2 matrix:

  | | Same address space | Different address space |
  |---|---|---|
  | Same build | local case | Process A / Process B |
  | Different build | Plugin / Host | stored bytes / cross-target |

- De-emphasize the same-build/same-address cell.
- Highlight the other three cells in sequence.
- Visible conclusion: `A boundary may lose either assumption—or both.`

### 5. A new process preserves representation—but not referents

- Use the same executable/build on both sides.
- Mark build identity as retained and address-space identity as lost.
- Show identical pointer bits referring to a real object in A and an unknown location in B.
- Do not introduce the Admission term yet.

### 6. A shared address space does not make two builds layout-compatible

- Show Plugin build P and Host build H in one process.
- Mark address-space identity as retained and build identity as lost.
- List only ABI-relevant drift sources: compiler, flags, headers, packing, standard-library ABI.
- Visible conclusion: `Pointers may still be meaningful—but layout Agreement is no longer automatic.`
- State orally that the talk later selects a stricter address-independent profile.

### 7. Stored bytes outlive both the build and the address space

- Show writer build A → file/persistent region → later reader build B.
- Mark both assumptions as lost.
- Small footer: versioning, trust, and crash consistency remain separate obligations.

### 8. One strict profile makes the three scenarios comparable

- Funnel Process, Plugin, and Stored into one strict profile.
- Show four conditions: ordinary object copy, zero fixup, source-address-independent bytes, finite declared build set.
- Visible statement: `Even when a particular boundary shares an address space, this talk asks whether the bytes can stand without relying on that fact.`

### 9. Seven claims turn the question into a decision

- Replace the dense six-stage map with a single cumulative chain:

  ```text
  Boundary
  → Local checks cannot compare builds
  → Generate canonical evidence
  → Establish Agreement
  → Establish Admission
  → Close the declared set in CI
  → Issue a narrow representation permit
  ```

- Future section markers show only the current part of the chain.
- Treat this as a brief orientation beat. Do not explain downstream predicates, formulas, artifact formats, or CI mechanics here.
- Do not label the slide `Agenda`.

### 10. Trivially copyable permits a local operation—it compares no builds

- Show the trait evaluating independently on Build A and Build B.
- Do not visually connect the two results.
- Visible conclusion: `Two local truths do not establish a cross-build relation.`
- Preview that trivial copyability later belongs to Admission.

### 11. `sizeof` can reject compatibility—but equal size cannot establish it

- Show the rule: different total size definitely differs; equal total size remains unknown.
- Use the 16-byte `wchar_t` counterexample with different member positions.
- Visible conclusion: equal total storage can hide offsets, padding placement, bit allocation, and leaf representation differences.
- Transition: `Each build must describe the representation it actually produced.`

### 12. A useful signature must earn our trust

- Define four properties as audience-facing questions:
  - coverage: did we record every representation fact used by this proof?
  - canonicality: do the same supported facts produce the same normalized form?
  - discrimination: does every encoded difference change the certificate?
  - fail-closed behavior: when a fact required by this proof cannot be completely encoded, does signature generation fail instead of producing partial evidence?
- Visible statement: `This is not a type-name hash. It is a representation certificate within an explicitly supported domain.`
- Do not claim global completeness for all C++ types.

### 13. The declaration identifies entities; the compiler supplies the byte map

- Replace “the declaration remains the source of truth.”
- Show `PacketHeader` declaration beside the compiler-produced byte map, size, and alignment.
- Visible statement: `We do not derive layout from declaration order. We ask the compiler what it produced.`

### 14. Reflection exposes facts; recursion turns them into structure

- Show only the relevant reflection operations: `nonstatic_data_members_of`, `type_of`, `offset_of`, `bases_of`, and `bit_size_of`.
- Show the action chain: enumerate → recover type → read position → classify → recurse.
- State that reflection exposes compiler facts; TypeLayout defines normalization policy.

### 15. One consteval dispatcher handles every supported category

- Show the category tree: leaf, enum, array, record, union, opaque, unsupported.
- Give each category one explicit action: encode supported exposed structure, emit a named trust contract for opaque, or reject unsupported machinery.
- Do not present opaque internals as reflected evidence.
- End with `unsupported → compile-time rejection`.
- Visible statement: `There is no generic “probably compatible” fallback.`

### 16. Leaf tokens describe representation—not source spelling

- Map source types and aliases to canonical tokens such as `u32`, `f32`, `fld80`, and `ptr`, each with size and alignment.
- Explain that typedef names and member names are intentionally absent.
- Mention the IEC 559 precondition for `float` and `double`.
- Keep `wchar_t`, plain `char`, and `bool` assumptions for the appendix.

### 17. Absolute offsets remove irrelevant source paths

- Compare nested member, base subobject, and flat declarations.
- Show `absolute leaf offset = parent absolute offset + child offset`.
- Converge the three declarations to one root-relative leaf map.
- Visible statement: `Canonicalization removes source paths, but it never removes byte-position facts.`
- State that protocols needing names or nesting require an explicit schema.

### 18. Hidden layout machinery is rejected—not guessed

- Use virtual inheritance as one concrete fail-closed example.
- Show the causal sequence: hidden layout machinery is required by the proof → the supported signature cannot encode it completely → signature generation rejects the type at compile time.
- Recollect the policy in one line: `Encode exposed structure. Name explicit trust. Reject hidden machinery.`
- Move the complete array, enum, union, bit-field, empty-object, opaque, and unsupported-case matrix to the appendix.

### 19. A consteval walk assembles the certificate

- Reveal the complete `PacketHeader` signature once.
- Color and decode the target envelope as pointer width plus endianness, then the record header, absolute offset, leaf token, and leaf layout.
- Explicitly reconcile the Sched wording: `architecture` here means the representation-relevant target envelope; the current signature prefix is not a CPU/ISA identifier, and exact target identity is later bound by provenance.
- Show `compiler facts → recursive normalization → FixedString → per-build certificate`.
- Recollect coverage, canonicality, discrimination, and fail-closed behavior.
- State once that the same recursive inspection supplies the structural facts later consumed by Admission; do not introduce the Admission formula yet.

### 20. Agreement is a registered-type × build-edge predicate

- Define:

  ```text
  Agreement(K,A,B)
  iff Key_A(K) = Key_B(K)
      and Signature(K,A) = Signature(K,B)
  ```

- Explain that the key identifies the application contract and the signature compares representation.
- State that one comparison establishes Agreement on one declared edge within the declared signature domain/version.

### 21. Exact equality is both a gate and a diagnostic

- Show one short match and one short divergence.
- Use examples such as `@8 ↔ @12`, `f64 ↔ fld80`, and `a:4 ↔ a:8`.
- Show one real compile-time check using the current primitive, not a fabricated convenience API:

  ```cpp
  static_assert(layout_match(
      linux_plat::PacketHeader_layout,
      macos_plat::PacketHeader_layout));
  ```

- Visible conclusion: exact equality avoids compatibility heuristics and provides a readable failure location.
- Hashes may index signatures but must not be the only acceptance evidence.

### 22. Agreement proves encoded representation—not identity, meaning, or independence

- State the three limits compactly: matching encoded representation does not establish source identity, application meaning, or source-context independence.
- Do not introduce three separate examples. The pointer example on slide 23 supplies the concrete evidence for the next inference; the semantic limit returns once on slide 27.
- Transition: `Agreement says the encoded certificates match. It does not yet say the value can stand alone.`

### 23. Matching layouts can preserve the wrong thing

- Merge the current two pointer slides into one staged reveal.
- First show identical pointer bits and matching signatures.
- Then reveal the referent only exists in the producer address space.
- End with `Agreement MATCH / Admission FAIL`.
- Visible statement: `The pointer bits survived. The referent did not.`

### 24. Admission applies one transfer profile to one build

- Define `Admission_P(K,B)`.
- Restate the strict profile compactly: ordinary copy, zero fixup, source-address independence.
- Visible statement: `Pointer rejection is a consequence of this profile—not a universal rule for every boundary.`

### 25. Structural Admission has three independent conditions

- Define:

  ```text
  Admission_P(T,B)
  = LocalCopyLegal
    and NoDetectedStructuralContextDependency
    and RepresentationEvidenceComplete
  ```

- Explain each term in one short line.
- `RepresentationEvidenceComplete` means every reachable component is encoded or covered by an explicitly named trust contract; if a required component is unsupported, signature generation fails and Admission cannot pass.

### 26. The recursive check closes structural blind spots

- Show the ordinary-copy composition:

  ```cpp
  static_assert(
      std::is_trivially_copyable_v<PacketHeader> &&
      is_byte_copy_safe_v<PacketHeader>);
  ```

- Use one nested aggregate to show that the check is recursive.
- Explicitly state that `is_byte_copy_safe_v<T>` alone is wider than the main ordinary-copy permit.
- Move the complete category walk and API variants to the appendix or speaker notes.

### 27. Structural inspection cannot infer semantic dependence

- Show one minimal integer-disguised-handle record; do not expand into a handle taxonomy.
- Show `Structural Admission may pass / Application contract may reject`.
- Visible statement: `TypeLayout detects structural dependencies; it cannot recognize a handle disguised as an integer.`

### 28. Admission and Agreement reject independent failures

- Show the complete 2×2 decision matrix.
- Only Admission pass on both endpoint nodes plus Agreement match on their edge yields `EDGE PASS`.
- Visible one-edge formula: `EdgePass_P(K,A,B) = Admission_P(K,A) ∧ Admission_P(K,B) ∧ Agreement(K,A,B)`.
- Visible conclusion: `Evidence is input; one edge passes only by conjunction.`
- State that this formula assumes valid, correctly attributed evidence and is not the final per-key Permit over the complete declared build graph.
- Transition: `One declared edge can satisfy both gates. That does not yet close the declared build set.`

### 29. A closed claim needs a finite contract

- Define `C = (R,V,E,P)`:
  - registered contract keys;
  - exact participating builds;
  - required transfer edges;
  - ordinary-copy profile.
- Prefer four labeled boxes over an extended set-theory explanation.
- Visible statement: `CI proves a quantified finite set—not every compiler, target, or future build.`

### 30. Every actual build emits its own evidence

- Show each declared build node independently producing its signature and local Admission-related evidence.
- Use Linux GCC, Linux Clang P2996, and macOS Clang P2996 as the example nodes.
- Visible statement: `No build may infer another build’s representation.`

### 31. The emitted header contains evidence—not provenance

- Show one compact artifact containing the current decision fields: contract key, layout signature, and byte-copy-safe result. Ordinary-copy registration separately enforces local trivial copyability.
- Do not enumerate the complete `TypeEntry` or platform-metadata fields on the main slide.
- Clearly separate observed evidence from producer identity and freshness.
- Visible statement: `The artifact describes what was observed; CI must establish who produced it.`

### 32. CI binds evidence to an exact producer

- Show one simple binding: declared build identity + producer attestation → accepted evidence input.
- State that provenance validates the input before Admission and Agreement; it is not a third compatibility gate.
- Move the full source, headers, compiler, target, library, ABI flags, TypeLayout version, digest, workflow, and storage-location details to appendix slide 58.
- Visible statement: `The artifact says what was observed; CI establishes who produced it and when.`

### 33. CI quantifies the same two gates over the declared graph

- Show Linux GCC, Linux Clang, and macOS Clang as the declared nodes, with the required transfer edges visibly marked.
- Show Admission status inside each node.
- Generalize slide 28's edge decision for one registered key over the entire declared build graph: `ClosedPermit_C(K) = Admission_P(K,B) for every B ∈ V ∧ Agreement(K,A,B) for every (A,B) ∈ E`.
- State that CI repeats this closed decision for every `K ∈ R`; it does not collapse mixed per-type results into one ambiguous Permit.
- Move equality transitivity and spanning-tree comparison reduction to the appendix or speaker notes; they are optimizations, not part of the causal spine.
- Do not display the nonexistent `TYPELAYOUT_ASSERT_TRANSFER_SAFE` macro.

### 34. A closed run rejects every missing fact

- Show two operational outcomes for each registered key:
  - any required evidence is missing, stale, unattributable, or fails Admission/Agreement: no Permit for that key;
  - every required fact for that key is valid and passes: `ClosedPermit_C(K)`.
- State that the run is complete only after every declared key has a closed decision; an optional all-types-must-pass workflow policy belongs in notes or the appendix.
- Keep the finer failure taxonomy in diagnostics, notes, or the appendix.
- Use actual primitive composition or current `CompatReporter`, not a fabricated API.
- Preview the reporter's three audience-relevant diagnostic shapes—`byte-copy safe + layout match`, `Layout match (not byte-copy safe)`, and `Layout mismatch`—without explaining their implementation. Stage 5 attaches one concrete type to each shape; appendix slide 57 owns the full report.
- Visible statement: `A skipped macOS job does not pass a three-build contract.`

### 35. A real contract starts with bytes every supported build must read

- Show the concrete persistent boundary: recorder build → capture file/persistent bytes → later analyzer build.
- Show the build labels Linux x86-64/GCC 16, Linux x86-64/Clang P2996, and Apple ARM64/Clang P2996; speaker notes state that provenance binds the exact build identities behind those labels.
- State that any declared recorder may feed any declared analyzer, so all three pairwise Agreement edges are required.
- Introduce the production set `R_capture = { PacketHeader, MeasurementSample, CaptureTrailer, CaptureBlock }` without expanding all declarations.
- Keep the transfer profile visible as one compact label: `P: ordinary copy · zero fixup · source-address-independent`.
- Visible question: `Can this entire native type set use one raw-byte path?`

### 36. Four native types pass the complete three-build contract

- Compose the 96-byte `CaptureBlock` visually from a 16-byte `PacketHeader`, four 16-byte `MeasurementSample` records, and a 16-byte `CaptureTrailer`.
- Show one compact result for every `K ∈ R_capture`: Admission PASS on all three nodes, Agreement MATCH on every required edge, therefore the complete run establishes four independent `ClosedPermit_C(K)` results for `C = C_capture`.
- Make the direct authorization precise: `ClosedPermit_C(CaptureBlock)` authorizes native object representation for the whole-block raw-I/O path. Show whole `CaptureBlock` → raw write or `memcpy` → bytes → raw read or `memcpy` → whole `CaptureBlock`, while leaving lifetime, storage, synchronization, and error handling to slides 40–41. The other three Permits remain independent per-key results.
- Show the compact two-gate CI result for `CaptureBlock`: `Admission PASS + Agreement MATCH → PERMIT`. Speaker notes may map that back to trivial-copy registration plus the reporter's `byte-copy safe + layout match` wording.
- State explicitly that the operation performs no field-by-field encoding, endian conversion, or pointer fixup.
- Do not collapse the four per-key Permits into a new undefined global Permit; call them the permitted set.
- Visible statement: `Inside C_capture, CaptureBlock may use native bytes as its representation layer.`

### 37. One cached pointer removes a type from the raw-byte path

- Start from the fixed-width `MeasurementSample`, then add a cached metadata pointer to form `UnsafeWithPointer` as a proposed candidate key.
- Under `C_candidate(UnsafeWithPointer)`, show Agreement MATCH on every required edge and Admission FAIL on every node because the copied address depends on the producer process.
- Keep it outside `R_capture`; label the result `Agreement MATCH / Admission FAIL / REJECT`.
- Use the reporter's concrete diagnostic wording: `Layout match (not byte-copy safe)`.
- Visible statement: `Matching pointer bits do not make the referent transferable.`

### 38. One `long double` removes a type from the cross-ABI path

- Start from the fixed-width `MeasurementSample { uint64_t id; int64_t value_microunits; }`, then replace the value representation with the opening `Measurement { uint64_t id; long double value; }` as a proposed candidate key.
- Compare the two decisive representations side by side:
  - Linux x86-64 Clang: `id @0:u64`, `value @16:fld80[s:16,a:16]`, record size 32/alignment 16;
  - Apple ARM64 Clang: `id @0:u64`, `value @8:fld64[s:8,a:8]`, record size 16/alignment 8.
- Keep the shared `[64-le]` prefix visible to demonstrate that the global envelope alone is insufficient.
- Under `C_candidate(Measurement)`, show Admission PASS on every node, Linux GCC ↔ Linux Clang Agreement MATCH, and both required Linux ↔ Apple edges DIFFER.
- End with `Admission PASS / Agreement DIFFER / REJECT`.
- Use one compact `[DIFFER] Measurement layout signatures` diagnostic around the two decisive signature fragments.
- Visible statement: `Address-independent bytes can still have different representations.`

### 39. Four permits and two rejections exercise both gates

- Show the final three-row matrix:
  - every `K ∈ R_capture`: Admission PASS everywhere, Agreement MATCH everywhere, four per-key Permits;
  - candidate `UnsafeWithPointer`: Admission FAIL everywhere, Agreement MATCH everywhere, reject;
  - candidate `Measurement`: Admission PASS everywhere, Agreement DIFFER on the Linux–Apple edges, reject.
- Keep the workflow success rule in speaker notes rather than on the slide.
- Visible statement: `Agreement cannot rescue source dependence. Admission cannot rescue representation drift.`
- Resolve the opening question: when proposed under the same `V`, `E`, and `P`, `Measurement` fails its candidate contract and therefore cannot enter `R_capture`.

### 40. A representation permit is deliberately narrow

- Split the slide into `TypeLayout proves` and `Application still owns`.
- Proven: ordinary-copy Admission on every declared build, representation Agreement on required edges, and closed evidence for the declared contract.
- Application-owned: schema meaning, valid values and invariants, storage/lifetime, alignment/synchronization, trust/versioning.
- Visible statement: `Representation permit ≠ end-to-end safety.`

### 41. Runtime obligations depend on the boundary

- Show a compact table:
  - shared memory: lifetime, alignment, publication, memory ordering, torn reads;
  - Plugin/Host: unload safety, borrowed lifetimes, allocator, call surface;
  - stored bytes: versioning, durability, crash consistency, padding leakage;
  - network/device: validation, framing, coherence, open-ended peers.
- Visible statement: `Compile time decides whether the representation is admitted; runtime still owns the operation.`

### 42. Serialize when the required contract is broader

- Contrast controlled native bytes with serialization or explicit conversion.
- Native bytes fit closed, controlled, performance-sensitive, continuously verified contracts.
- Serialize/convert for independent evolution, untrusted input, canonical bytes, open targets, endian conversion, semantic transformation, or process-local handles.
- Visible statement: `A failed gate identifies where an explicit representation layer belongs.`

### 43. The real question is not “can I memcpy?”—it is “under which contract?”

- Return to the opening `Measurement` and distinguish two questions:
  - local operation: may this object be copied as bytes here?
  - boundary contract: may these bytes stand independently, and do all declared builds give them the same representation?
- State that `trivially_copyable` and `sizeof` can contribute local facts but cannot answer the relational question.
- Reframe the opening result: the unqualified question was incomplete; under the demonstrated candidate contract, `Measurement` is rejected because Agreement differs.
- Show the compact callback: `Measurement under C_candidate(Measurement) → Agreement DIFFER → REJECT`.
- Visible statement: `Across a boundary, a native C++ type becomes a binary contract.`

### 44. Reflection derives representation evidence; CI closes the decision

- Show one left-to-right chain:

  ```text
  declare C = (R,V,E,P) and contract key K
  → every B in V evaluates Admission_P(K,B)
    and emits Signature_B(K)
  → CI validates evidence inputs
  → Admission on every declared build node
    + Agreement of signatures on every required build edge
  → CI closes K over C
  → ClosedPermit_C(K) or REJECT
  ```

- Keep Admission and Agreement visually distinct while showing that neither alone reaches the final decision.
- Treat evidence presence, freshness, and attribution as input validation summarized in speaker notes, not as a third compatibility gate.
- Visible statement: `The compiler supplies the facts; the declared contract gives those facts scope.`

### 45. Permit native bytes only inside a closed contract

- Present four design-review takeaways:

  ```text
  1. Declare C = (R,V,E,P).
  2. Check Admission and Agreement separately.
  3. Keep every Permit per-type and contract-scoped.
  4. Need a broader contract? Convert explicitly.
  ```

- Scope note: `Representation compatibility—not semantic compatibility or schema evolution.`
- Final statement: `Permit native bytes only inside a closed contract; convert explicitly when the required contract is broader.`
- Keep the GitHub URL and a Q&A cue to appendix slide 46 in the footer.
- Do not end on a generic “Thank you” slide.

## 6. Appendix Design

The appendix contains 16 slides after the 45-slide main deck:

| Output | Question / topic | Primary source pattern |
|---:|---|---:|
| 46 | Q&A map | 37 |
| 47 | Why not `has_unique_object_representations`? | 38 |
| 48 | Padding locations versus padding contents | 39 |
| 49 | Implicit lifetime, storage, alignment, overlap, and synchronization | 40 |
| 50 | Endianness and why byte swapping is conversion | 41 |
| 51 | Why the global `[64-le]` envelope is conservative | 42 |
| 52 | `char`, `bool`, `wchar_t`, and floating-point assumptions | 44 |
| 53 | Opaque type trust boundary | 43 |
| 54 | Full supported / assumed / rejected matrix | 44 |
| 55 | Full signature grammar and recursive engine pseudocode | 48 |
| 56 | Complete difficult-case encodings | 45 |
| 57 | Diagnostic report anatomy | 46 |
| 58 | Artifact format versus CI provenance | 51 |
| 59 | Portable-capture demo: types, artifacts, and exact verdicts | 53 |
| 60 | Ordinary copy versus relocation | 54 |
| 61 | C++29 relocation status and project-policy limits | 55 |

Appendix slide 59 owns the details deliberately removed from Stage 5: the complete positive and negative type declarations, the two separate exporter registries, the per-producer artifact bundle, the no-padding fixture assertions, the retained generated signatures, the three-node/three-edge verdict table, and the exact success condition.

Material promoted to the main deck must not remain duplicated in the appendix. Appendix versions should contain additional detail, not a second copy of the same conclusion.

## 7. Primary Source-Slide Mapping

The final template frame map must resolve exact element IDs after complete source-deck inspection. The intended primary source slide for each output is:

| Output | Source | Output | Source | Output | Source |
|---:|---:|---:|---:|---:|---:|
| 1 | 1 | 22 | 52 | 43 | 6 |
| 2 | 4 | 23 | 17 | 44 | 26 |
| 3 | 2 | 24 | 15 | 45 | 36 |
| 4 | 2 | 25 | 18 | 46 | 37 |
| 5 | 3 | 26 | 50 | 47 | 38 |
| 6 | 2 | 27 | 19 | 48 | 39 |
| 7 | 2 | 28 | 21 | 49 | 40 |
| 8 | 3 | 29 | 23 | 50 | 41 |
| 9 | 6 | 30 | 24 | 51 | 42 |
| 10 | 5 | 31 | 51 | 52 | 44 |
| 11 | 14 | 32 | 25 | 53 | 43 |
| 12 | 8 | 33 | 26 | 54 | 44 |
| 13 | 9 | 34 | 27 | 55 | 48 |
| 14 | 10 | 35 | 28 | 56 | 45 |
| 15 | 48 | 36 | 29 | 57 | 46 |
| 16 | 12 | 37 | 30 | 58 | 51 |
| 17 | 52 | 38 | 31 | 59 | 53 |
| 18 | 45 | 39 | 32 | 60 | 54 |
| 19 | 13 | 40 | 34 | 61 | 55 |
| 20 | 13 | 41 | 40 |  |  |
| 21 | 13 | 42 | 35 |  |  |

Each output slide must duplicate one source slide and edit inherited elements in place. Secondary content references do not authorize rebuilding a slide from scratch.

## 8. Visual System

The redesigned deck preserves the source deck’s master, layout, typography, footer, page numbering, palette, and technical editorial style.

### 8.1 Fixed visual semantics

| Meaning | Treatment |
|---|---|
| compiler/build evidence | blue or source-deck neutral evidence color |
| Agreement | cyan/blue build-edge connector plus `MATCH` / `DIFFER` text |
| Admission | amber node-local state plus `PASS` / `FAIL` text |
| rejection | red plus explicit reason |
| edge-level decision | green outline plus `EDGE PASS` text; never `PERMIT` |
| closed-contract permit | green plus `PERMIT` text |
| application-owned obligations | neutral gray |

Color never carries meaning without text.

### 8.2 Approved visual forms

- byte map;
- recursive type walk;
- build graph;
- decision matrix;
- producer/consumer boundary;
- short code fragment;
- short signature fragment.

Avoid card grids, decorative illustrations, repeated full signatures, more than two flow-arrow layers, and slides that simultaneously contain code, table, diagram, and long prose.

### 8.3 Code and signature treatment

- Show only code needed for the current inference.
- Keep code to approximately 4–8 visible lines when possible.
- Never repeat a full signature without adding a new decoding or comparison action.
- Use consistent syntax highlighting and monospaced typography from the source deck.
- Keep offset, category token, size, and alignment colors consistent across all signature slides.

## 9. Terminology Contract

Use these terms consistently:

| Term | Exact meaning |
|---|---|
| Evidence | signature and Admission-related facts produced by one build |
| Admission | a node predicate for one type, build, and transfer profile |
| Agreement | exact signature equality for one registered type on one declared edge |
| EdgePass | the scoped conjunction of both endpoint Admission predicates and Agreement on one declared edge, assuming valid attributed evidence |
| Permit | the per-registered-type result after that key passes every required node and edge predicate in the declared finite contract |

Avoid unqualified uses of `safe`, `portable`, `compatible`, and `proof`. Prefer:

- structurally admitted;
- representation-compatible;
- closed-set proof;
- representation permit;
- no detected structural source-context dependency.

## 10. Technical Accuracy Requirements

The final deck must satisfy all of the following:

- Reflection exposes compiler facts; TypeLayout defines the canonicalization policy.
- The signature is a representation certificate only inside its supported domain.
- The Sched abstract's `architecture and endianness` wording denotes representation-relevant target facts; the current signature `arch-prefix` encodes pointer width and endianness only, while exact compiler/target identity belongs to provenance.
- The signature's global target envelope is deliberately conservative and may cause false negatives.
- `float` and `double` claims respect the implementation’s IEC 559 checks.
- `long double` uses a representation-specific token.
- Records and bases use absolute root-relative offsets; arrays and unions retain their category structure.
- Opaque registration is named trust, not reflected internal evidence.
- Virtual inheritance is rejected because the required hidden machinery is not represented.
- Agreement is per registered type and declared build edge.
- Admission is per build and transfer profile.
- Ordinary copy requires both local trivial copyability and the recursive structural predicate.
- Structural inspection does not prove semantic address independence.
- `EdgePass` is not the final Permit; a registered key receives a Permit only after it is closed over the complete declared build graph.
- Agreement must be established on every required transfer edge; exact-equality transitivity may reduce direct comparisons only within the same declared contract key and signature domain.
- An absent or skipped required node prevents a claim; it does not count as a pass.
- Artifact evidence and provenance are distinct; provenance validates evidence inputs rather than adding a third compatibility gate.
- The main permit excludes relocation.
- The permit does not prove padding contents, deterministic byte equality, runtime lifetime, storage, synchronization, schema meaning, or input validity.

## 11. Repository and Demo Alignment

The deck must describe the repository at or after baseline `201f06f`. Before final presentation export:

1. Implement the portable-capture positive set and both expected-rejection fixtures in one reproducible three-build demonstration pipeline so the repository matches the completed-demo narrative designed in Stage 5.
2. Pin the P2996 toolchain/container by immutable digest and retain source revision, compiler version, flags, target, and workflow-run provenance for the demonstrated artifacts.
3. Use actual primitive composition or `CompatReporter` unless a combined compile-time transfer-safe API is implemented and tested.
4. Do not show `TYPELAYOUT_ASSERT_TRANSFER_SAFE` while it is absent from the repository.
5. Do not imply that `TYPELAYOUT_ASSERT_COMPAT` joins entries by contract key; the current compile-time helper compares array positions. Use the key-based runtime reporter for that claim or improve the helper before showing it.
6. Keep ordinary-copy and relocation evidence separate. The current artifact has no explicit copy-mode field, so relocation cannot silently share the main permit.
7. Treat the optional macOS job as outside the default proof unless a complete cross-target run actually executes it. A skipped job shrinks the claim.

### 11.1 Deferred Portable-Capture Demo Implementation Contract

**Status:** implementation design recorded on 2026-08-27 and intentionally deferred. This section records the code, test, and CI work needed later; it does not add conditional wording to the Stage 5 content design.

The planned positive types are:

```cpp
// Reuse the existing fixed-width PacketHeader.

struct MeasurementSample {
    std::uint64_t id;
    std::int64_t  value_microunits;
};

struct CaptureTrailer {
    std::uint64_t payload_hash;
    std::uint32_t sample_count;
    std::uint32_t flags;
};

struct CaptureBlock {
    PacketHeader header;
    MeasurementSample samples[4];
    CaptureTrailer trailer;
};

struct UnsafeWithPointer {
    MeasurementSample sample;
    const std::byte* cached_metadata;
};

struct Measurement {
    std::uint64_t id;
    long double value;
};
```

The intended build-local facts are `CHAR_BIT == 8`, `sizeof(PacketHeader) == 16`, `sizeof(MeasurementSample) == 16`, `sizeof(CaptureTrailer) == 16`, and `sizeof(CaptureBlock) == 96`, with all four positive types trivially copyable and byte-copy safe under the ordinary-copy profile. The future demo must assert the expected offset of every member and `sizeof(T) ==` the sum of its member object sizes at every recursive level, ruling out internal and tail padding in the positive fixture. Exact-width integer leaves supply the required no-padding-bit representations. `std::has_unique_object_representations_v<T>` may be an additional fixture cross-check, but it is not the library's compatibility gate and must not be presented as one. These are requirements for the future demo to execute, not observations already produced by the repository.

The planned implementation files and responsibilities are:

| Planned path | Responsibility |
|---|---|
| `example/portable_capture_types.hpp` | Reuse `PacketHeader`; declare the three additional positive types plus the two negative fixtures |
| `example/portable_capture_export.cpp` | Register only `R_capture` and export build-local signatures and Admission-related facts |
| `example/portable_capture_negative_export.cpp` | Register the two candidate fixtures in a separate test-evidence registry and export their signatures on every declared build |
| `example/portable_capture_io.cpp` | Construct one deterministic `CaptureBlock`, write/read its complete object representation without per-field encoding, and verify the logical round trip |
| `example/portable_capture_check.cpp` | Require all three platform artifacts, compare every required contract key and edge with current reporter primitives, and issue the four per-key Permits |
| `example/portable_capture_negative_check.cpp` | Require complete negative-fixture evidence, then verify the exact Admission/Agreement result shape for each candidate without adding either to the production allowlist |
| `CMakeLists.txt` | Add isolated exporter, I/O, aggregate-check, and expected-rejection targets/tests |
| `.github/workflows/compat-pipeline.yml` | Produce, retain, and aggregate the complete three-build demo artifact bundles |

Each producer bundle is planned to contain:

- its generated positive and negative-registry `.sig.hpp` evidence in distinct artifact paths;
- a deterministic raw `capture.bin` written from the complete `CaptureBlock` object representation;
- provenance sufficient to bind source revision, compiler/version, target, relevant flags, signature version, and workflow run to the artifact.

The aggregate verification is planned to require all three declared producers, establish Admission for each positive key on all nodes, establish Agreement on every required pairwise edge, compare the deterministic raw artifacts, and run the expected-rejection checks. Its success criterion is:

```text
positive set: 4/4 per-key Permits
UnsafeWithPointer: evidence complete; Admission FAIL on every node; Agreement MATCH on every edge
Measurement: evidence complete; Admission PASS on every node; Linux↔Linux MATCH; both Linux↔Apple edges DIFFER
raw capture artifacts: identical across all three producers
```

The raw-file comparison is demonstration evidence, not a replacement for the two-gate proof. It is meaningful here because the positive fixture deliberately uses fixed-width leaves, fully initializes every value, and requires padding-free layouts. The future implementation must keep these fixture-specific conditions distinct from the library's general Permit, which does not prove deterministic padding contents.

No public TypeLayout API change is required. The positive implementation should compose the existing signature exporter, Admission primitives, generated `TypeEntry` data, and `CompatReporter` one key at a time. Because the reporter's aggregate boolean does not prove why a negative case failed, the negative checker must first verify evidence completeness and then inspect the relevant `TypeEntry::byte_copy_safe` and signature/layout relations directly. It must not accept an absent artifact or an extra unintended failure as an expected rejection. If every pairwise edge is claimed diagnostically, check those pairs explicitly rather than relying only on comparison with a single reference node.

The positive and negative exporters generate the same platform basename, include guard, and namespace shape, so their outputs must remain in distinct directories and their checkers must use separate include paths/translation units. The workflow must pin the reflection toolchain/container by immutable digest and use an always-running closure/status job. If the Apple ARM64 producer is skipped or unavailable, that job reports `INCOMPLETE`, never `PERMIT`. Raw bytes must be read into an existing, correctly aligned trivially-copyable object; the demo must not reinterpret file storage directly as a live `CaptureBlock`.

## 12. Speaker Notes and Sources

Every externally sourced non-trivial claim must have a `[Sources]` block in speaker notes. At minimum, notes should distinguish:

- P2996 reflection wording and operations;
- the C++ implicit-lifetime and ordinary byte-copy rules;
- C++26/C++29 relocation status;
- platform-specific `long double` observations and the exact artifacts that produced them;
- any compiler or standard-library behavior described as factual.

The source deck’s existing notes should be preserved when still correct, updated when visible claims change, and removed when the associated slide is omitted.

## 13. Implementation Workflow

The PPTX implementation must use the existing deck as the design source and follow template-clone/edit mode:

1. Inspect and render every source slide.
2. Audit masters, layouts, inherited placeholders, fonts, footers, and slide numbering.
3. Create a validated `template-frame-map.json` using the mapping in this spec as the starting point.
4. Duplicate source slides into a starter deck.
5. Edit inherited elements in place with `@oai/artifact-tool`.
6. Preserve the source master/layout hierarchy and original theme parts.
7. Add or update speaker-note source blocks.
8. Render every final slide and inspect each one at full size.
9. Run overlap, overflow, placeholder, and template-fidelity checks.
10. Export a new PPTX copy; do not overwrite the user’s source attachment.

## 14. Acceptance Criteria

The redesigned deck is complete when:

- the main deck contains the approved 45-slide cumulative argument;
- the appendix contains slides 46–61 as a 16-question support structure, for 61 slides total;
- the opening `Measurement` question is answered on slides 38–39, reframed as a contract question on slide 43, and resolved by the final rule on slide 45;
- signature generation is the longest single technical chapter and establishes its trust properties;
- the three boundary scenarios each state which assumption is retained and which is lost;
- stages 1–7 each preserve one causal spine and defer non-essential inventories, variants, and optimizations;
- Stage 5 applies the model to one coherent persistent-storage contract, shows four per-key Permits with `CaptureBlock` authorizing native representation for the whole-object raw-byte path, and uses the pointer and `long double` alternatives to expose the two independent rejection modes;
- Stage 6 briefly bounds that Permit, preserves application-owned runtime obligations, and directs broader semantic or evolution requirements to explicit conversion;
- Stage 7 uses three distinct slides to restate the problem, compress the complete method, and deliver an actionable takeaway rather than leaving the audience on a list of limitations;
- slides 21 and 26 fulfill the promised compile-time checks with current `layout_match` and Admission primitives, while slides 34 and 36–38 expose the corresponding CI diagnostic shapes;
- Agreement, Admission, and Permit are visually and verbally distinct;
- slide 28 establishes only `EdgePass`, while slides 29–34 establish or reject a per-key Permit over the complete declared build graph;
- ordinary copy and relocation never share one unqualified permit;
- no slide displays a repository API that does not exist at the implementation baseline;
- CI claims are explicitly finite and provenance-bound, with provenance presented as evidence validation rather than a third compatibility gate;
- the portable-capture implementation contract is recorded separately and does not interrupt the Stage 5 audience-facing chain;
- each slide has one primary claim and an audience-facing title;
- no unintended overlap, clipping, placeholder prompt, broken connector, inconsistent footer, or unreadable code remains;
- all externally sourced claims and assets are traceable in speaker notes;
- the final PPTX preserves the source deck’s visual identity and remains fully editable.

## 15. Design Self-Review

The approved design has been reviewed for placeholders, contradictions, ambiguity, scope, technical accuracy, pacing, and opening/closing closure.

- There are no unresolved placeholders or undefined decision points.
- Boundary means build-identity loss or address-space-identity loss; the strict profile is separately stated.
- Trivial copyability is consistently local and belongs to ordinary Admission.
- Signature generation, Agreement, Admission, edge-level decision, CI closure, and application obligations have non-overlapping responsibilities.
- Stage 3 does not issue the final Permit; Stage 4 is the only stage that closes each registered key over the finite contract.
- Provenance establishes whether evidence may enter the two-gate decision and is never presented as a third gate.
- Stage 5 preserves per-key decisions: the permitted set is four independently permitted keys, not a new aggregate compatibility predicate.
- Stage 6 introduces no new gate or mechanism; it only limits the conclusion established by Stage 5.
- Stage 7 introduces no new proof obligation; slides 43–45 form the explicit chain problem recap → method recap → actionable takeaway.
- The production `R_capture` set contains only positive boundary types; pointer and `long double` alternatives are separate expected-rejection fixtures under the same build graph and profile.
- The recorded portable-capture implementation work remains separate from the completed-demo narrative used to design Stage 5.
- Opaque support is consistently described as trust rather than complete evidence.
- The deck uses actual repository primitives unless future implementation work adds a tested convenience API.
- The 45-slide main deck targets approximately 52–55 minutes, within the section-level 50–57 minute pacing range, with the signature chapter receiving the greatest allocation.
- Slide 45 resolves the opening question and ends on an operating rule rather than an implementation detail.
