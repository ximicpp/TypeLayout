# CppCon 2026 TypeLayout Deck Redesign

**Status:** Approved design, ready for implementation planning

**Date:** 2026-08-23

**Repository baseline:** `201f06f8a9dd20323ffd8af836c545ef2380e82d`

**Source deck:** `cppcon2026_typelayout_36slide_main_argument_map_editorial_no_audio_optimized.pptx`

**Talk title:** *Can I memcpy this type across a boundary?*

**Audience:** Experienced C++ developers attending CppCon 2026

**Format assumption:** 60-minute session, approximately 50–52 minutes of prepared content plus Q&A

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

   > Agreement is not permission: under the talk’s zero-fixup, source-address-independent profile, Structural Admission requires ordinary byte-copy legality, no detected source-context dependency, and closed evidence.

6. **Closed CI**

   > CI closes the claim over a finite type × build contract: every declared build emits fresh, attributable evidence; every type passes Admission on every node; and Agreement covers the declared comparison graph.

7. **Operating boundary**

   > A representation permit is deliberately narrow: it approves native object representation inside one closed contract; the application still owns meaning, lifetime, storage, synchronization, and trust—otherwise serialize or convert.

## 4. Narrative Architecture

The approved narrative is proof-first and signature-centered:

```text
opening question
→ define the boundary and transfer profile
→ show why local checks cannot compare builds
→ generate canonical per-build evidence
→ establish Agreement on build edges
→ establish Admission on build nodes
→ close the finite contract in CI
→ apply both gates to three types
→ state the operating boundary
```

The main deck contains 43 slides. The appendix contains 16 slides, for 59 slides total.

| Section | Main slides | Expected time | Communication job |
|---|---:|---:|---|
| Boundary and scope | 1–9 | 7–8 min | Define the question, two axes, three scenarios, and strict profile |
| Local checks | 10–11 | 2 min | Prove that local traits and total size cannot establish a relation |
| Signature engine | 12–19 | 12–14 min | Make the core technology trustworthy and inspectable |
| Agreement | 20–22 | 4–5 min | Define exact edge equality and its limits |
| Admission | 23–28 | 7–8 min | Define the profile-aware node predicate and structural limits |
| Closed CI | 29–34 | 7–8 min | Close the finite quantified claim with provenance |
| Apply | 35–39 | 5–6 min | Resolve the opening example and pressure-test both predicates |
| Operating boundary | 40–43 | 4 min | State what the permit proves and when to serialize |

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

- Define four properties:
  - coverage of every representation fact used by the proof;
  - canonicality for the same supported facts;
  - discrimination for every encoded difference;
  - fail-closed behavior for unavailable evidence.
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
- Give each category one explicit action.
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

### 18. Difficult cases are encoded explicitly—or rejected

- Show a compact two-column table:
  - arrays: recursive element signature and extent;
  - enums: underlying representation;
  - unions: total layout and alternatives;
  - bit-fields: byte.bit position, width, and storage kind;
  - empty/EBO/`no_unique_address`: actual embedded footprint;
  - opaque: named trust contract;
  - virtual inheritance: rejection.
- Visible rule: `Encode exposed structure. Name explicit trust. Reject hidden machinery.`

### 19. A consteval walk assembles the certificate

- Reveal the complete `PacketHeader` signature once.
- Color and decode the target envelope, record header, absolute offset, leaf token, and leaf layout.
- Show `compiler facts → recursive normalization → FixedString → constexpr evidence`.
- Recollect coverage, canonicality, discrimination, and fail-closed behavior.

### 20. Agreement is a registered-type × build-edge predicate

- Define:

  ```text
  Agreement(K,A,B)
  iff Key_A = Key_B
      and Signature(K,A) = Signature(K,B)
  ```

- Explain that the key identifies the application contract and the signature compares representation.
- State that one comparison proves one declared edge.

### 21. Exact equality is both a gate and a diagnostic

- Show one short match and one short divergence.
- Use examples such as `@8 ↔ @12`, `f64 ↔ fld80`, and `a:4 ↔ a:8`.
- Visible conclusion: exact equality avoids compatibility heuristics and provides a readable failure location.
- Hashes may index signatures but must not be the only acceptance evidence.

### 22. Agreement proves representation—not identity, meaning, or independence

- Show three counterexamples:
  - different source structures with the same canonical byte map;
  - a field with the same integer representation but different units;
  - a pointer token with the same representation and source dependence.
- Transition: `Agreement says the bits have the same structure. It does not yet say the value can stand alone.`

### 23. Matching layouts can preserve the wrong thing

- Merge the current two pointer slides into one staged reveal.
- First show identical pointer bits and matching signatures.
- Then reveal the referent only exists in the producer address space.
- End with `Agreement PASS / Admission FAIL`.
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
    and NoDetectedContextDependency
    and EvidenceClosed
  ```

- Explain each term in one short line.
- `EvidenceClosed` means every reachable component is encoded, explicitly trusted, or rejected; it does not claim opaque internals were reflected.

### 26. The recursive check closes structural blind spots

- Show the ordinary-copy composition:

  ```cpp
  ordinary_admission<T> =
      std::is_trivially_copyable_v<T> &&
      is_byte_copy_safe_v<T>;
  ```

- Show recursive handling of arrays, bases, members, unions, polymorphism, and opaque registrations.
- Explicitly state that `is_byte_copy_safe_v<T>` alone is wider than the main ordinary-copy permit.

### 27. Structural inspection cannot infer semantic dependence

- Show an apparently harmless record containing `uintptr_t`, a file descriptor, and a process-local index.
- Show `Structural Admission may pass / Application contract may reject`.
- Visible statement: `TypeLayout detects structural dependencies; it cannot recognize a handle disguised as an integer.`

### 28. Admission and Agreement reject independent failures

- Show the complete 2×2 decision matrix.
- Only Admission pass plus Agreement match yields `PERMIT`.
- Visible formula: `RepresentationPermit = Admission on every node ∧ Agreement on every required edge`.
- Visible conclusion: `Evidence is input; permission is the conjunction.`

### 29. A closed claim needs a finite contract

- Define `C = (R,V,E,P)`:
  - registered contract keys;
  - exact participating builds;
  - permitted/required build edges;
  - ordinary-copy profile.
- Visible statement: `CI proves a quantified finite set—not every compiler, target, or future build.`

### 30. Every actual build emits its own evidence

- Show exact source, headers, and toolchain feeding a reflection exporter on build B.
- Show the exporter producing signatures and local Admission results.
- Use Linux GCC, Linux Clang P2996, and macOS Clang P2996 as the example nodes.
- Visible statement: `No build may infer another build’s representation.`

### 31. The emitted header contains evidence—not provenance

- Show the current `TypeEntry` shape: contract key, layout signature, byte-copy-safe result.
- Show platform label, architecture envelope, and ABI measurements.
- Clearly separate what the header contains from source revision, compiler identity, flags, and freshness.
- Visible statement: `The artifact describes what was observed; CI must establish who produced it.`

### 32. CI binds evidence to an exact producer

- Show a provenance envelope containing source revision, boundary headers, compiler/version, target triple, standard library, ABI flags, TypeLayout/signature version, toolchain digest, and workflow run.
- Provenance may be an external CI attestation rather than fields inside `.sig.hpp`.
- Visible statement: `Signatures describe the result; provenance identifies the producer.`

### 33. Exact equality lets a connected graph close the build set

- Show Linux GCC — Linux Clang — macOS Clang with two matching edges.
- Show Admission status inside each node.
- Explain equality transitivity and why a spanning tree can reduce comparison count.
- State that connectivity never replaces per-node evidence.
- Do not display the nonexistent `TYPELAYOUT_ASSERT_TRANSFER_SAFE` macro.

### 34. A closed run rejects every missing fact

- Show five outcomes:
  - missing required node: no claim;
  - stale or unattributable evidence: reject;
  - Admission failure: reject;
  - Agreement failure: reject;
  - all quantified facts pass: permit.
- Use actual primitive composition or current `CompatReporter`, not a fabricated API.
- Visible statement: `A skipped macOS job does not pass a three-build contract.`

### 35. Three types put the two gates under pressure

- Present `PacketHeader`, `UnsafeWithPointer`, and `Measurement`.
- Ask the audience to predict Admission, Agreement, and final decision.
- Keep descriptions to one line per type.

### 36. Agreement cannot rescue failed Admission

- Show two rows:
  - PacketHeader: Admission pass, Agreement match, permit;
  - UnsafeWithPointer: Admission fail, Agreement match, reject.
- Visible statement: `Same layout is necessary—but never sufficient.`

### 37. Linux represents `Measurement` in 32 bytes

- Show Linux x86-64 Clang:
  - `id` at 0 as `u64`;
  - `value` at 16 as `fld80` with 16-byte object representation;
  - record size 32, alignment 16.
- Show `Admission PASS / Agreement pending`.

### 38. macOS represents it in 16 bytes—Agreement fails

- Compare Linux and macOS ARM64 directly:
  - size 32/alignment 16 versus size 16/alignment 8;
  - `@16:fld80` versus `@8:fld64`.
- Keep the shared `[64-le]` prefix visible to demonstrate that the global envelope alone is insufficient.
- End with `Admission PASS / Agreement FAIL / REJECT`.

### 39. One permit and two rejections explain the complete model

- Show the three-row final matrix.
- Visible statement: `Admission cannot rescue representation drift. Agreement cannot rescue source dependence.`
- Resolve the opening question: do not ship `Measurement` across the declared edge.

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

### 43. Permit native bytes only inside a closed contract

- Final three lines:

  ```text
  Declare the boundary.
  Generate and close the evidence.
  Permit native bytes only inside that contract.
  ```

- Re-show the `Measurement` result compactly.
- Final statement: `Native bytes are a controlled optimization—not a universal wire format.`
- Keep the GitHub URL and Q&A cue in the footer.
- Do not end on a generic “Thank you” slide.

## 6. Appendix Design

The appendix contains 16 slides after the 43-slide main deck:

| Output | Question / topic | Primary source pattern |
|---:|---|---:|
| 44 | Q&A map | 37 |
| 45 | Why not `has_unique_object_representations`? | 38 |
| 46 | Padding locations versus padding contents | 39 |
| 47 | Implicit lifetime, storage, alignment, overlap, and synchronization | 40 |
| 48 | Endianness and why byte swapping is conversion | 41 |
| 49 | Why the global `[64-le]` envelope is conservative | 42 |
| 50 | `char`, `bool`, `wchar_t`, and floating-point assumptions | 44 |
| 51 | Opaque type trust boundary | 43 |
| 52 | Full supported / assumed / rejected matrix | 44 |
| 53 | Full signature grammar and recursive engine pseudocode | 48 |
| 54 | Complete difficult-case encodings | 45 |
| 55 | Diagnostic report anatomy | 46 |
| 56 | Artifact format versus CI provenance | 51 |
| 57 | Full `TelemetryFrame` signature | 53 |
| 58 | Ordinary copy versus relocation | 54 |
| 59 | C++29 relocation status and project-policy limits | 55 |

Material promoted to the main deck must not remain duplicated in the appendix. Appendix versions should contain additional detail, not a second copy of the same conclusion.

## 7. Primary Source-Slide Mapping

The final template frame map must resolve exact element IDs after complete source-deck inspection. The intended primary source slide for each output is:

| Output | Source | Output | Source | Output | Source |
|---:|---:|---:|---:|---:|---:|
| 1 | 1 | 16 | 12 | 31 | 51 |
| 2 | 4 | 17 | 52 | 32 | 25 |
| 3 | 2 | 18 | 45 | 33 | 26 |
| 4 | 2 | 19 | 13 | 34 | 27 |
| 5 | 3 | 20 | 13 | 35 | 28 |
| 6 | 2 | 21 | 13 | 36 | 29 |
| 7 | 2 | 22 | 52 | 37 | 30 |
| 8 | 3 | 23 | 17 | 38 | 31 |
| 9 | 6 | 24 | 15 | 39 | 32 |
| 10 | 5 | 25 | 18 | 40 | 34 |
| 11 | 14 | 26 | 50 | 41 | 40 |
| 12 | 8 | 27 | 19 | 42 | 35 |
| 13 | 9 | 28 | 21 | 43 | 36 |
| 14 | 10 | 29 | 23 |  |  |
| 15 | 48 | 30 | 24 |  |  |

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
| permit | green plus `PERMIT` text |
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
| Permit | the closed-contract result after all required node and edge predicates pass |

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
- The architecture prefix is a deliberately conservative global envelope and may cause false negatives.
- `float` and `double` claims respect the implementation’s IEC 559 checks.
- `long double` uses a representation-specific token.
- Records and bases use absolute root-relative offsets; arrays and unions retain their category structure.
- Opaque registration is named trust, not reflected internal evidence.
- Virtual inheritance is rejected because the required hidden machinery is not represented.
- Agreement is per registered type and declared build edge.
- Admission is per build and transfer profile.
- Ordinary copy requires both local trivial copyability and the recursive structural predicate.
- Structural inspection does not prove semantic address independence.
- A connected equality graph closes only the explicitly declared finite set.
- An absent or skipped required node prevents a claim; it does not count as a pass.
- Artifact evidence and provenance are distinct.
- The main permit excludes relocation.
- The permit does not prove padding contents, deterministic byte equality, runtime lifetime, storage, synchronization, schema meaning, or input validity.

## 11. Repository and Demo Alignment

The deck must describe the repository at or after baseline `201f06f`. Before final presentation export:

1. Put `PacketHeader`, `UnsafeWithPointer`, and `Measurement` into one reproducible demonstration pipeline, or label the cross-target results as recorded reference artifacts rather than a current always-on green pipeline.
2. Pin the P2996 toolchain/container by immutable digest and retain source revision, compiler version, flags, target, and workflow-run provenance for the demonstrated artifacts.
3. Use actual primitive composition or `CompatReporter` unless a combined compile-time transfer-safe API is implemented and tested.
4. Do not show `TYPELAYOUT_ASSERT_TRANSFER_SAFE` while it is absent from the repository.
5. Do not imply that `TYPELAYOUT_ASSERT_COMPAT` joins entries by contract key; the current compile-time helper compares array positions. Use the key-based runtime reporter for that claim or improve the helper before showing it.
6. Keep ordinary-copy and relocation evidence separate. The current artifact has no explicit copy-mode field, so relocation cannot silently share the main permit.
7. Treat the optional macOS job as outside the default proof unless a complete cross-target run actually executes it. A skipped job shrinks the claim.

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

- the main deck contains the approved 43-slide cumulative argument;
- the appendix contains the 16-question support structure;
- the opening `Measurement` question is answered on slides 37–39 and resolved again on slide 43;
- signature generation is the longest single technical chapter and establishes its trust properties;
- the three boundary scenarios each state which assumption is retained and which is lost;
- Agreement, Admission, and Permit are visually and verbally distinct;
- ordinary copy and relocation never share one unqualified permit;
- no slide displays a repository API that does not exist at the implementation baseline;
- CI claims are explicitly finite and provenance-bound;
- each slide has one primary claim and an audience-facing title;
- no unintended overlap, clipping, placeholder prompt, broken connector, inconsistent footer, or unreadable code remains;
- all externally sourced claims and assets are traceable in speaker notes;
- the final PPTX preserves the source deck’s visual identity and remains fully editable.

## 15. Design Self-Review

The approved design has been reviewed for placeholders, contradictions, ambiguity, scope, technical accuracy, pacing, and opening/closing closure.

- There are no unresolved placeholders or undefined decision points.
- Boundary means build-identity loss or address-space-identity loss; the strict profile is separately stated.
- Trivial copyability is consistently local and belongs to ordinary Admission.
- Signature generation, Agreement, Admission, CI closure, and application obligations have non-overlapping responsibilities.
- Opaque support is consistently described as trust rather than complete evidence.
- The deck uses actual repository primitives unless future implementation work adds a tested convenience API.
- The 43-slide main deck fits approximately 50–52 minutes with the signature chapter receiving the greatest allocation.
- The final slide resolves the opening question and ends on an operating rule rather than an implementation detail.
