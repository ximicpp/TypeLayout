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

The main argument first uses one strict ordinary-copy profile:

- ordinary object copy;
- zero fixup;
- source-address-independent bytes;
- finite declared type and build sets;
- trusted producer objects;
- representation verification rather than semantic schema verification.

Stage 5 then makes one explicit profile change to `whole_region_relocation`. Under that separate profile, a complete contiguous region may be copied to a new base while its single region-relative offset space is preserved. Region headers, elements, and pointees may not be moved independently. The two compatibility gates do not change; only the declared Admission policy and its application-owned invariant change.

The relocatable-world example models one producer build and one pre-verified consumer build. The same closed contract supports two application placements: server-to-server checkpoint transfer or recovery, and server-to-declared-native-client snapshot delivery. The client claim is about exact builds and ABI evidence, not architecture names alone. It does not turn the region into an open network format.

The published Sched abstract uses `architecture and endianness` as shorthand for representation-relevant target facts. At repository baseline `201f06f`, the signature's global `arch-prefix` explicitly encodes pointer width and endianness; leaf tokens, sizes, and alignments carry further representation facts, while exact compiler and target identity belong to CI provenance. The deck must explain this mapping once and must not claim that the signature prefix encodes a complete CPU architecture or ISA identity.

The main permit excludes:

- relocation of non-trivially-copyable objects;
- process-local pointers, references, member pointers, and function pointers;
- application semantics, units, enum meaning, handles, and invariants;
- open-ended platform portability;
- untrusted-input validation;
- runtime lifetime, storage, alignment, synchronization, framing, and crash consistency;
- deterministic canonical bytes when padding or multiple value representations are possible.

Relocation remains a separate contract, not an extension of an ordinary-copy Permit. The deck must not imply that `is_byte_copy_safe_v<T>` alone grants ordinary `memcpy` permission, or that a whole-region Permit authorizes independent relocation of its parts.

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

   > Agreement is not permission: Admission applies one declared transfer profile to one type on one build. The ordinary-copy profile rejects source-context dependence; the later whole-region profile admits only explicitly supported region-relative representations under the complete-region invariant.

6. **Closed CI**

   > CI closes each registered type's claim over a finite build contract: every declared build emits evidence; CI accepts it only when it is fresh and attributable; and that type receives a Permit only when Admission passes on every node and Agreement is established on every required transfer edge. Invalid or missing input leaves the claim incomplete; a gate failure rejects it.

7. **Operating boundary**

   > A representation permit is deliberately narrow: it approves native object representation inside one closed contract and one profile; the application still owns meaning, lifetime, storage, synchronization, validation, and trust—otherwise choose another explicit profile or representation layer.

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
→ authorize one ordinary-copy type set and reject two nearby alternatives
→ change the profile explicitly and show a relocatable world region with dynamic data and an object graph
→ separate the build/CI Permit from runtime validation of the actual bytes
→ state the operating boundary
→ restate the problem, method, and decision rule as the final takeaway
```

The main deck contains 47 slides. The appendix contains 16 slides, for 63 slides total.

The talk uses two coordinated threads:

- **decision spine:** boundary → evidence → Agreement → Admission → closed CI → contract-scoped Permit;
- **value thread:** a stored native-byte region is previewed when both build and address-space identity are lost; its server-to-server and server-to-declared-native-client placements are introduced when the complete example begins, then used to bound and summarize the result.

The value thread carries the question and the payoff, not the implementation. `PacketHeader` remains the small signature example, `UnsafeWithPointer` isolates Admission, and `Measurement` isolates real ABI Agreement drift. Region containers, graph structure, validation, and business output remain concentrated in Slides 38–42 and the appendix.

| Section | Main slides | Expected time | Communication job |
|---|---:|---:|---|
| Boundary and scope | 1–9 | 7–8 min | Define the question, two axes, three scenarios, and strict profile |
| Local checks | 10–11 | 2 min | Prove that local traits and total size cannot establish a relation |
| Signature engine | 12–19 | 12–14 min | Make the core technology trustworthy and inspectable |
| Agreement | 20–22 | 4–5 min | Define exact edge equality and its limits |
| Admission | 23–28 | 7–8 min | Define the profile-aware node predicate and structural limits |
| Closed CI | 29–34 | 7–8 min | Produce provenance-bound per-key decisions over the finite build graph |
| Apply | 35–42 | 9–10 min | Establish the ordinary-copy baseline, then separate pre-deployment permission from runtime use of a connected world region |
| Operating boundary | 43–44 | 2–3 min | State what each Permit proves, what remains application-owned, when to re-close, and when to use an explicit representation |
| Conclusion and takeaway | 45–47 | 3 min | Restate the problem, compress the complete method, and leave one actionable decision rule |

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

This keeps the technical review constraints subordinate to the audience-facing argument. The main-deck flow is `why evidence is needed → how one build produces evidence → how the two gates decide one edge → how CI closes the finite contract → how the model works for ordinary copy → how a separate whole-region profile supports declared server and native-client consumers → how build/CI permission differs from runtime validation → what each Permit does and does not authorize → the complete decision rule the audience should retain`; it must not become a boundary catalog, signature grammar tour, Admission API inventory, provenance-field inventory, graph-optimization tutorial, or demo-implementation walkthrough.

Stages 1–7 preserve exactly two compatibility gates: Admission on build nodes and Agreement on build edges. CI provenance validates that evidence belongs to the declared producer and run; it is an input-validity condition, not a third compatibility gate.

### 4.2 Stage 1 — Why Evidence Is Needed (Slides 1–11)

**Job:** explain why direct byte transfer needs evidence that local checks do not provide.

Tell the story in this order:

```text
We move native C++ bytes across a boundary.
→ The receiver may use a different build, a different address space, or both.
→ We first limit the claim to one strict transfer profile and a finite build set.
→ That creates two separate questions.
→ Local traits and total size cannot answer both questions.
→ Each build must describe the representation it actually produced.
```

Start with one type and ask whether the audience would approve its bytes across every supported build. Do not answer yet.

Use the build-identity × address-space-identity matrix to show what a boundary can change. Give one short example for each lost assumption, then combine them in the stored-bytes case.

On Slide 7, complete the matrix with the both-assumptions-lost case. Show producer build A writing a stored native-byte region and declared consumer build B reading it under another build identity, address space, and load base. Preview server recovery and native-client world snapshots only as the later demo. Do not introduce the two placements, region containers, relative-pointer implementation, or runtime obligations yet.

State the exact starting profile used through the ordinary-copy baseline:

- ordinary object copy;
- no fixup;
- bytes must not depend on the producer's address space;
- the participating build set is finite and declared in advance.

Define a transfer profile as the rules for one kind of byte transfer. Then name the two questions:

1. **Admission:** may this type use this profile?
2. **Agreement:** do the declared builds give it the same object representation?

Then show why familiar checks are not enough:

- `trivially_copyable` covers local byte-copy legality, not the complete cross-boundary claim;
- equal `sizeof` does not prove equal member types, offsets, alignment, bit-fields, or other representation facts;
- code review does not produce repeatable evidence for every declared build.

Slide 8 states that the first profile is an ordinary-copy baseline, defines Admission and Agreement in plain language, and previews the later explicit `whole_region_relocation` profile. Slide 9 may preview the rest of the argument and name the world-region producer/consumer result as the final payoff, but it must not introduce formulas, APIs, or CI details yet.

Keep long compiler/flag/header/ABI inventories, IPC mechanics, versioning, trust, framing, lifetime, synchronization, and crash consistency out of this stage. A short boundary footer is enough.

End with:

> `Each build must describe the representation it actually produced.`

Then ask: `How can one build produce trustworthy representation evidence?`

### 4.3 Stage 2 — How One Build Produces Evidence (Slides 12–19)

**Job:** show how one build produces a representation certificate with clear limits, not just a generated string.

State the input, output, and failure rule:

```text
Input:  ordinary C++ type T compiled by build B
Output: Signature_B(T) within the explicitly supported signature domain
Failure: compile-time rejection
```

Tell the story in this order:

```text
First say what the certificate must guarantee.
→ Use reflection to obtain the layout facts produced by this compiler.
→ Normalize the supported representation recursively.
→ Encode visible structure, name explicit trust, and reject unsupported hidden structure.
→ Build one inspectable consteval certificate.
→ Check the result against the original requirements.
```

Before showing any implementation, ask four simple questions:

- **Coverage:** did we record every representation fact needed by this check?
- **Canonicality:** do the same supported facts always produce the same normalized form?
- **Discrimination:** does every encoded difference change the certificate?
- **Fail closed:** if a required fact cannot be encoded completely, does generation fail instead of returning partial evidence?

Keep the responsibilities separate:

```text
compiler + reflection → layout facts
TypeLayout            → normalization policy
FixedString           → per-build certificate
later CI stages       → export, freshness, and provenance
```

Show one declaration beside the byte map produced by the compiler. Explain only this reflection path:

```text
enumerate → get type → get position → classify → recurse
```

The certificate records canonical leaf tokens, sizes, alignments, and root-relative absolute offsets. It also covers the supported array, nested-record, bit-field, and pointer-like cases.

Be precise about architecture information. The signature prefix records pointer width and endianness. Exact compiler and target identity belongs to build provenance, not to the type signature itself.

Use one small policy table:

- supported visible structure: encode it;
- explicitly trusted opaque structure: record the named trust contract;
- unsupported structure with no trust contract: reject it at compile time.

Virtual inheritance is the preferred fail-closed example. End with one complete, decoded `PacketHeader` certificate.

Do not show the full primitive-token table, complete category rules, opaque macro variants, relocation contracts, signature grammar, or recursive pseudocode here.

An opaque type is not described as fully reflected. It is covered by an explicit trust contract. The certificate covers the encoded facts in the declared signature domain; it does not claim to describe every compiler fact or every C++ type. Field names and source-level meaning are not part of the certificate.

End with:

> `Each build now has its own inspectable representation certificate.`

The certificate supplies the evidence used by Agreement. The same recursive inspection also supplies the local structural facts used by Admission, but Admission itself starts in Stage 3.

Then ask: `What does exact equality between two certificates establish—and what does it not establish?`

### 4.4 Stage 3 — How the Two Gates Decide One Edge (Slides 20–28)

**Job:** separate the node check from the edge check, then derive the rule for one declared transfer edge. Do not call this a final Permit.

Use this picture:

```text
Build A node ───── Agreement edge ───── Build B node
  Admission                              Admission
```

Tell the story in this order:

```text
The registered contract key and both certificates match.
→ Agreement holds on this declared edge.
→ A pointer example shows why Agreement is not permission.
→ Admission checks each endpoint under the selected transfer profile.
→ An integer-handle example shows what structural Admission cannot know.
→ Both endpoint Admissions and the edge Agreement must pass.
→ Only this one declared edge now passes.
```

Define Agreement first because it follows directly from the certificate produced in Stage 2. State that this is the explanation order, not a priority between the gates: a Permit still needs both. Do not call Agreement the first gate. For the same registered contract key and signature-domain version, exact certificate equality establishes Agreement on one declared edge. Show one readable match and one readable difference.

State its three limits plainly. Agreement does not prove:

- that both sides came from the same source declaration;
- that both sides give the bytes the same application meaning;
- that the value is independent of the producer's context.

Use a pointer as the counterexample: the certificates can match while the copied address still depends on the producer process. This gives `Agreement MATCH / Admission FAIL`.

Then define Admission for one type, one build, and one transfer profile. Under the talk's strict profile it requires:

1. ordinary byte-copy legality;
2. `NoDetectedStructuralContextDependency`;
3. `RepresentationEvidenceComplete`.

For the ordinary-copy path, show:

```cpp
std::is_trivially_copyable_v<T> && is_byte_copy_safe_v<T>
```

On Slide 24, recall the connected world region without teaching its implementation: its region-relative offsets require a different profile and the complete-region invariant. This callback exists only to prove that Admission is profile-dependent.

Say “no detected structural dependency,” not “proven context-independent.” Complete evidence means that every reachable component is encoded or covered by a named trust contract. It does not mean that opaque internals were reflected. If a required component is unsupported, Stage 2 rejects generation, so Admission and `EdgePass` cannot be established.

Use an integer-disguised handle to show the other limit. Structural Admission may pass, but the application may still reject the value because TypeLayout does not know its meaning.

For one contract key and one build edge, define:

```text
EdgePass_P(K,A,B)
= Admission_P(K,A)
  and Admission_P(K,B)
  and Agreement(K,A,B)
```

Keep the node/edge decision matrix on screen. Move exhaustive pointer-token lists, the full recursive Admission algorithm, relocation API variants, parser details, reporter details, and artifact implementation to the appendix.

`EdgePass` assumes that both evidence artifacts are valid and correctly attributed. It covers one edge only. It is not the final Permit for the complete build graph.

End with:

> `One declared edge can satisfy both gates. That does not yet close the declared build set.`

Then ask: `How does CI check every declared type, build node, and required edge?`

### 4.5 Stage 4 — How CI Closes the Finite Contract (Slides 29–34)

**Job:** extend the one-edge rule to every registered type, declared build, and required transfer edge. Also verify that every evidence artifact came from the build and run it claims to describe.

State the input and the possible results:

```text
Input:  C = (R,V,E,P) plus evidence emitted by every declared build
Output: one closed PERMIT / REJECT decision for every evaluable K in R
Incomplete: missing, stale, or unattributable required evidence leaves the claim INCOMPLETE and issues no Permit
```

Explain the four parts without a set-theory detour:

- `R`: the registered boundary-type keys;
- `V`: the exact participating builds;
- `E`: the transfer edges that must work;
- `P`: the transfer profile.

The set is finite and explicit. Nothing here covers an unnamed compiler, target, configuration, or future build.

Tell the story in this order:

```text
One passing edge does not cover the full build set.
→ Declare C = (R,V,E,P).
→ Every real build emits its own evidence.
→ CI verifies who produced each artifact and when.
→ For each K, check Admission on every build node.
→ For each K, check Agreement on every required edge.
→ Missing, stale, or unattributable evidence gives INCOMPLETE and no Permit.
→ Valid evidence plus a failed gate gives REJECT and no Permit.
→ If every required check passes, K receives ClosedPermit_C(K).
→ The run is complete only when every K in R has PERMIT or REJECT.
```

Show one build graph. Every node must visibly emit its own signature and Admission-related evidence. State the division of work clearly:

> `The artifact says what the build observed. CI establishes who produced it and when.`

Provenance validates the input before the two gates run. It is not a third compatibility gate.

When all required evidence is present, fresh, and attributable, define:

```text
ClosedPermit_C(K)
= Admission_P(K,B) for every B in V
  and Agreement(K,A,B) for every (A,B) in E
```

Use one missing-build example. A skipped required job never counts as a pass.

Keep the three states separate:

- `INCOMPLETE`: required evidence is missing, stale, or cannot be attributed;
- `REJECT`: evidence is valid, but Admission or Agreement fails;
- `PERMIT`: valid evidence covers the complete graph and both gates pass everywhere required.

Both `INCOMPLETE` and `REJECT` issue no Permit, but they mean different things and need different diagnostics.

The Permit belongs to one key `K`. A project may separately require every key in `R` to receive a Permit before the workflow passes. That is an aggregate project policy, not a new compatibility predicate.

Move complete metadata fields, the full provenance envelope, graph-comparison optimizations, the detailed diagnostic taxonomy, reporter APIs, and convenience macros to notes or the appendix.

End with:

> `A complete run gives every declared type PERMIT or REJECT; incomplete evidence remains INCOMPLETE and never produces a Permit.`

Then state the application sequence: first use a small ordinary-copy contract to isolate the two failure shapes, then return to the stored world region previewed on Slide 7. Ask: `Can the same two-gate model authorize a useful producer-and-consumer contract?`

### 4.6 Stage 5 — Apply the Model to Useful Raw-Byte Contracts (Slides 35–42)

**Job:** first complete the public ordinary-copy example, then show why the method matters for a connected world region crossing to another declared server or native-client build. The profile change must be explicit. Admission and Agreement must be shown as compile/build/CI decisions completed before deployment, while actual region validation remains runtime work.

State the two-part result:

```text
ordinary_copy
  fixed-width set → PERMIT
  native pointer → Admission REJECT
  long double → Agreement REJECT

whole_region_relocation
  dynamic strings + collections + object graph
  → four per-key Permits
  → server consumer: relocate, validate, mutate, save, reload
  → native-client consumer: relocate, validate, query, use
```

Treat Slides 35–37 as one three-result group rather than three new stories. Use parallel titles and verdict shapes: `Admission PASS + Agreement MATCH`, `Admission FAIL + Agreement MATCH`, and `Admission PASS + Agreement DIFFER`. The ordinary-copy baseline uses `C_capture` and the same three declared builds from the earlier design. Compress its positive set into Slide 35, retain a short separate Slide 36 for the already-explained pointer failure, and give Slide 37 enough time to resolve the opening `Measurement` question with real ABI evidence:

- `UnsafeWithPointer`: Agreement matches, but ordinary-copy Admission fails because the stored address depends on the producer process.
- `Measurement { uint64_t id; long double value; }`: Admission passes, but the real Linux x86-64 and Apple ARM64 representations differ, so Agreement fails.

These three slides satisfy the public fixed-width pass, pointer-containing Admission rejection, and platform-divergent Agreement rejection. The packed-`Entity` case introduced later is synthetic supplemental evidence and must not replace `Measurement`.

Slide 38 expands the stored-region preview from Slide 7 into the full producer-and-consumer question. Fixed records prove the decision rule, but they do not show the practical value of retaining native, connected data. Introduce two placements of the same region contract here: server to declared server for checkpoint transfer or recovery, and server to declared native client for snapshot delivery. A useful world region contains dynamic names, collections, indexes, shared targets, null links, and cycles. The server path resumes, modifies, and saves again; the client path may stop after validation and query.

Slide 39 changes the contract explicitly and names it `C_world`:

```text
P_world = whole_region_relocation

copy one complete contiguous region
preserve one region-relative offset space
do not move headers, elements, or pointees independently
```

Show `C_world = (R_world,V,E,P_world)` before listing the four keys.

State that `V` contains exact pre-verified builds, which may play server or native-client roles, and `E` contains declared producer-to-consumer edges. Do not imply that a CPU architecture label establishes Agreement.

Declare four Agreement keys:

```text
R_world = {
  WorldSnapshot,
  Entity,
  EntityRelativePtr,
  EntityIndexEntry
}
```

Under this profile, the supported `relative_ptr` and frozen region containers are admitted only because the complete-region invariant gives their stored offsets a stable interpretation. Native pointers remain address-space-dependent and fail Admission.

Slide 40 separates build/CI permission from runtime execution:

```text
each declared build
  → compile-time Admission
  → exported signatures
verification build / CI
  → complete evidence
  → Agreement on each declared edge
  → four contract-scoped Permits
```

State explicitly that runtime does not recompute Admission or Agreement. The visible positive lines from the demo summarize build/CI evidence materialized into the demonstration; they are not loader-time reflection checks.

Slide 41 shows the complete runtime flow without expanding container implementations:

```text
checkpoint file or network snapshot
validate the envelope
copy to a different base
validate stored ranges and graph before dereference
server consumer: query, mutate, save, and reload
native-client consumer: query and use
```

Make the practical result visible:

- the destination base changes;
- stored region-relative offsets remain unchanged;
- null, shared, cyclic, and container-stored links validate;
- party HP is 420;
- tick changes from 42 to 43;
- boss HP changes from 300 to 250;
- the second load preserves the mutation.

Prefer `stored region-relative offsets remain unchanged` over `raw offsets unchanged`; the latter can be confused with TypeLayout member offsets.

Slide 42 maps each negative to the phase that owns it:

```text
native pointer         build-time Admission FAIL       no Permit
packed Entity          build/CI Agreement DIFFER       no Permit
corrupt region offset  runtime graph REJECT before dereference
```

The first two rows fail before deployment of that native-byte path. The talk-sized demo may say `load skipped` to show that its orchestrated workflow stops, but the logical decision owner is build/CI. The corrupt-offset row is not a third TypeLayout gate. It demonstrates an application-owned runtime obligation after the two representation gates. The packed fixture demonstrates signature sensitivity inside this relocation example; it is not the real platform-divergent example promised by the public abstract.

Do not present a local `producer_ok` fixture comparison as the full supported-build matrix. A slide may state the exact local demo result, while any claim over the declared multi-build set must cite the corresponding retained build artifacts.

Move complete declarations, region-container implementation, builder logic, full signature strings, envelope bytes, provenance fields, all pairwise matrix rows, and detailed validation code to the appendix or implementation notes.

End with:

> `Build and CI can authorize a useful region contract for declared server or native-client consumers, but runtime validation and the complete-region invariant still belong to the application.`

Then ask: `What exactly does each Permit authorize, and what still belongs to the application?`

### 4.7 Stage 6 — Bound the Permit (Slides 43–44)

**Job:** stop the audience from applying either Stage 5 Permit more broadly than its evidence and profile allow. Also explain when native bytes stop being the right representation.

Tell the story in this order:

```text
Stage 5 gives representation Permits inside two separate contracts.
→ Each Permit proves representation compatibility under one declared profile.
→ The application still owns lifetime, storage, synchronization, validation, and failures.
→ A finite change to V or E needs fresh evidence and a new decision.
→ A changed transfer model needs a new profile or an explicit representation layer.
```

Keep this stage short, but keep all three boundaries clear.

Slide 43 combines the proof boundary and the application-owned work. One table separates what a profile-specific Permit proves from the profile invariant, valid values, storage, lifetime, alignment, synchronization, transport, validation, trust, and versioning that remain with the application. State once that build and CI check the representation contract while runtime validates the actual bytes and operation. Do not repeat the corrupt-offset example from Slide 42. The detailed boundary matrix belongs on appendix slide 51.

Slide 44 separates two kinds of change:

- If a finite build or edge set changes, generate fresh evidence and close the revised contract again.
- If the transfer model changes but remains closed, declare and verify the appropriate profile.
- If peers are open-ended, representations differ, canonical bytes are required, or meaning and schema must evolve independently, use an explicit representation and conversion layer.

Untrusted input still needs validation. Serialization alone does not make it safe.

Do not reopen signature generation, CI mechanics, or demo internals. This stage adds no new gate. It only limits the Permits already established.

End with:

> `Re-close finite contract changes; change the profile explicitly; use an explicit representation when the required contract cannot stay closed.`

Then ask: `What problem did we solve, how did we solve it, and what rule should the audience remember?`

### 4.8 Stage 7 — Summarize the Problem, Method, and Takeaway (Slides 45–47)

**Job:** return to the positive result after the limitations. Answer the opening question and leave one complete decision rule that the audience can reuse.

Tell the story in this order:

```text
Return to “Can I memcpy this type across a boundary?”
→ The question is incomplete until the boundary contract is named.
→ Declare C and K.
→ Check the two separate questions: may the bytes travel, and do the representations match?
→ Every declared build evaluates Admission and emits a reflection-derived signature.
→ CI accepts only complete, fresh, attributable evidence.
→ Admission on every node and Agreement on every required edge decide K over C.
→ Permit native bytes only inside C. Re-check finite changes. Use an explicit representation when C cannot stay closed.
```

Use three slides because the conclusion has three jobs:

- **Slide 45 — restate the problem:** local `memcpy` legality is not the same as cross-boundary permission. Return to both running examples: `Measurement` is rejected by Agreement under the ordinary-copy candidate contract, while the world region receives build/CI Permits under its separate whole-region contract and still requires runtime validation of actual input.
- **Slide 46 — restate the method:** show one uninterrupted path from `C` and `K`, through per-build Admission and signature evidence, through Agreement and CI, to `ClosedPermit_C(K)` or rejection.
- **Slide 47 — give the rule:** show the design-review checklist and the final operating rule.

The checklist is:

1. Declare `C = (R,V,E,P)`.
2. Check Admission and Agreement separately.
3. Keep every Permit per-type, per-profile, and limited to that contract.
4. Re-check finite changes; change profiles explicitly; use an explicit representation when the required contract cannot stay closed.

Keep the scope statement visible:

> `Representation compatibility—not semantic compatibility or schema evolution.`

Keep the demo callback visible on Slide 47:

> `Different profile. Same two gates. Separate Permit.`

This is a summary, not a new proof stage. Do not repeat signature grammar, demo layouts, ABI numbers, reporter output, or artifact mechanics. Every conclusion here must point back to something already established in Stages 1–6.

End the talk with:

> `Permit native bytes only inside a closed contract. Re-close finite changes; use an explicit representation when the contract cannot stay closed.`

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
- Show identical pointer bits referring to an object in Process A while no target is established in Process B.
- Do not introduce the Admission term yet.

### 6. A shared address space does not make two builds layout-compatible

- Show Plugin build P and Host build H in one process.
- Mark address-space identity as retained and build identity as lost.
- List only ABI-relevant drift sources: compiler, flags, headers, packing, standard-library ABI.
- Visible conclusion: `Pointers may still work, but matching layout is not automatic.`
- State orally that the talk later selects a stricter address-independent profile.

### 7. Stored native bytes may lose both assumptions

- Show producer build A → stored native-byte region → declared consumer build B.
- Mark build identity, address-space identity, and load base as changed.
- Preview the later demo only with `server recovery · native-client world snapshot`.
- Do not introduce the two placements, supported-client definition, connected graph, profile invariant, or runtime obligations here.

### 8. Start with one strict transfer profile

- Funnel Process, Plugin, and Stored into one strict profile.
- Define a transfer profile as the rules for one kind of byte transfer.
- Name the baseline `ordinary copy` and show four conditions: `memcpy`-style object transfer, zero fixup or field conversion, source-address-independent bytes, and a finite declared build set.
- Define the two questions in plain language: `Admission — May this type use this profile?` and `Agreement — Do the declared builds give it the same object representation?`
- Preview `whole_region_relocation` as the later connected-world profile and state orally that Stage 5 declares it separately rather than reusing this ordinary-copy Permit.

### 9. Seven steps turn the question into a decision

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

- Add one short payoff line: the small examples explain each step, and the world-region producer/consumer demo shows the combined result.

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
- Add one compact callback: the connected world region's relative offsets require `whole_region_relocation` plus the complete-region invariant.

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
- Move the full source, headers, compiler, target, library, ABI flags, TypeLayout version, digest, workflow, and storage-location details to appendix slide 60.
- Visible statement: `The artifact says what was observed; CI establishes who produced it and when.`

### 33. CI quantifies the same two gates over the declared graph

- Show Linux GCC, Linux Clang, and macOS Clang as the declared nodes, with the required transfer edges visibly marked.
- Show Admission status inside each node.
- Generalize slide 28's edge decision for one registered key over the entire declared build graph: `ClosedPermit_C(K) = Admission_P(K,B) for every B ∈ V ∧ Agreement(K,A,B) for every (A,B) ∈ E`.
- State that CI repeats this closed decision for every `K ∈ R`; it does not collapse mixed per-type results into one ambiguous Permit.
- Move equality transitivity and spanning-tree comparison reduction to the appendix or speaker notes; they are optimizations, not part of the causal spine.
- Do not display the nonexistent `TYPELAYOUT_ASSERT_TRANSFER_SAFE` macro.

### 34. Missing evidence makes the run incomplete—not passing

- Show three operational states for each registered key:
  - missing, stale, or unattributable required evidence: `INCOMPLETE / NO PERMIT`;
  - valid evidence plus Admission or Agreement failure: `REJECT / NO PERMIT`;
  - valid evidence plus both gates passing over the complete graph: `PERMIT`.
- State that the run is complete only after every declared key has a `PERMIT / REJECT` decision; `INCOMPLETE` means the requested closed claim was not evaluated.
- Keep the detailed failure taxonomy in diagnostics, notes, or appendix slide 59.
- Use actual primitive composition or current `CompatReporter`, not a fabricated API.
- Preview the reporter's three audience-relevant diagnostic shapes—`byte-copy safe + layout match`, `Layout match (not byte-copy safe)`, and `Layout mismatch`—without explaining their implementation. Stage 5 attaches one concrete type to each shape; appendix slide 59 owns the full report.
- Visible statement: `A skipped macOS job makes the three-build claim INCOMPLETE; it does not pass or reject the type.`
- In the transition, say that the ordinary-copy baseline comes first to isolate the gates, then the talk expands the stored-region preview from Slide 7.

### 35. Ordinary copy: a fixed-width contract passes

- Compress the recorder → persistent bytes → analyzer boundary and `C_capture = (R_capture,V,E,P_ordinary)` into one slide.
- Compose the 96-byte `CaptureBlock` from one 16-byte header, four 16-byte samples, and one 16-byte trailer.
- Show four independent per-key results: Admission PASS on all declared builds, Agreement MATCH on every required edge, and four Permits.
- State that `ClosedPermit_C(CaptureBlock)` authorizes whole-block raw I/O with no field encoding, endian conversion, or fixup.
- Visible statement: `Inside C_capture, CaptureBlock may use its native bytes as the stored representation.`

### 36. Ordinary copy: a native pointer fails Admission

- Add `const Metadata* cached_metadata` to the fixed-width sample.
- Under `C_candidate(UnsafeWithPointer)`, show Agreement MATCH and Admission FAIL because the copied address depends on the producer process.
- Keep it outside `R_capture` and use the diagnostic wording `Layout match, but not byte-copy safe`.
- Visible statement: `Matching pointer bits do not transfer the target object.`

### 37. Ordinary copy: `long double` fails Agreement across real ABIs

- Return to the opening `Measurement { uint64_t id; long double value; }`.
- Compare the decisive Linux x86-64 and Apple ARM64 signature fragments, including the shared `[64-le]` prefix.
- Show Admission PASS on every node, Linux ↔ Linux MATCH, and Linux ↔ Apple DIFFER.
- End with `Admission PASS / Agreement DIFFER / REJECT`.
- Visible statement: `Address-independent bytes can still have different representations.`

### 38. One connected world region supports two closed boundaries

- Open by expanding the stored-region preview from Slide 7 into the producer-and-consumer question.
- State what the fixed capture did and did not prove: it validates the two-gate method but does not show the payoff for connected application state.
- Show server → declared server for checkpoint transfer or recovery, and server → declared native client for snapshot delivery.
- Show the game-world requirements: dynamic names, entity collection, ID index, null/shared/cyclic links, save/load, mutation, and reload.
- Visible goal: `Move one complete region to the consumer's base without per-object encoding or pointer fixups.`
- End by stating that this is a different transfer contract and therefore needs a different profile.

### 39. Whole-region relocation is a different declared profile

- Contrast `ordinary_copy` with `whole_region_relocation`; do not present one as an upgrade to the other's Permit.
- State the invariant: copy one complete contiguous region, preserve one region-relative offset space, and never move a header, element array, or pointee independently.
- Name the full contract `C_world = (R_world,V,E,whole_region_relocation)`.
- Show `R_world = { WorldSnapshot, Entity, EntityRelativePtr, EntityIndexEntry }`.
- State that `V` names exact declared server and native-client builds and `E` names declared producer-to-consumer edges.
- State that supported relative representations are admitted under this profile; native pointers are still rejected.
- Visible statement: `The profile changes. The two gates do not.`

### 40. Build and CI establish the Permit before deployment

- Show per-build compile-time Admission and signature export, followed by verification-build/CI evidence validation and Agreement on every declared edge.
- Show four contract-scoped Permits only after complete evidence and both gates pass.
- State explicitly that runtime does not recompute Admission or Agreement.
- If the visible Agreement result comes from a retained local producer fixture, label it as that exact comparison. Reserve full supported-matrix language for retained native build evidence.
- Visible statement: `Build and CI approve the native-byte path before deployment.`

### 41. Runtime validates the actual region before typed access

- Show the runtime flow: checkpoint file or network snapshot → validate the envelope → copy to a different base → validate stored ranges and graph before dereference.
- Show the two consumer uses: a server queries, mutates, saves, and reloads; a native client may stop after validation, query, and use.
- Make these outcomes visible: destination base changed; stored region-relative offsets unchanged; null/shared/cyclic/container links PASS; party HP 420; tick 42→43; boss HP 300→250; mutation persisted after reload.
- State that network framing, reliability, and authentication remain transport obligations.
- Visible statement: `Connected native data moved as one region, with no per-field decoding or pointer fixups.`

### 42. Each failure stops at the layer that owns it

- Show three rows:
  - native pointer → build-time Admission FAIL → no Permit;
  - packed `Entity` → build/CI Agreement DIFFER → no Permit;
  - corrupt region offset → runtime graph REJECT before dereference.
- Label packed `Entity` as synthetic ABI drift; it supplements but does not replace the real `Measurement` result on Slide 37.
- State explicitly that runtime graph validation is not a third TypeLayout gate.
- Visible statement: `TypeLayout checks representation. The application still validates the stored graph.`

### 43. A Permit proves representation, not runtime safety

- Split the slide into `TypeLayout proves` and `Application still owns`.
- Proven: profile-specific Admission on every declared build, representation Agreement on required edges, and complete evidence for the declared contract.
- Application-owned: the profile invariant, schema meaning, valid values, storage/lifetime/alignment, synchronization/transport, validation/trust/versioning.
- State that the exact application work depends on the boundary, but do not repeat the corrupt-offset example from Slide 42.
- Move the complete shared-memory / plugin / stored-bytes / network-device matrix to appendix slide 51.
- Visible statements: `Representation Permit ≠ end-to-end safety.` and `Build and CI check the representation contract; runtime validates the actual operation and bytes.`

### 44. Re-close finite changes; change the profile explicitly

- Native bytes fit closed, controlled, performance-sensitive, continuously verified contracts.
- If a finite declared build or edge set changes, regenerate evidence and close the revised contract again.
- If the transfer model changes, declare another profile and its invariants explicitly; do not reuse an old Permit.
- Use an explicit representation/conversion layer for open-ended peers, representation divergence, canonical bytes, endian conversion, independent evolution, semantic transformation, or process-local handles.
- Untrusted input additionally requires validation; serialization alone does not make it safe.
- Visible statement: `Re-check the contract after a finite change. Change the representation when the contract cannot stay closed.`

### 45. The real question is not “can I memcpy?”—it is “under which contract?”

- Return to the opening `Measurement` and distinguish the local operation from the boundary contract.
- Reframe the opening result: under the demonstrated ordinary-copy candidate contract, `Measurement` is rejected because Agreement differs.
- Show `Measurement under C_candidate(Measurement) → Agreement DIFFER → REJECT`.
- Contrast it with the positive world-region result: under `C_world`, build and CI establish four separate Permits, a declared server or native-client consumer loads the whole region, and runtime graph validation remains application-owned.
- State the synthesis: different profiles, the same two gates, and separate contract-scoped results.
- Visible statement: `Across a boundary, a native C++ type becomes a binary contract.`

### 46. Reflection derives representation evidence; CI closes the decision

- Show one uninterrupted chain from `C`, `K`, and the selected profile through per-build Admission and signatures, input validation, node and edge checks, and the per-key result.
- Keep Admission and Agreement visually distinct while showing that neither alone reaches the final decision.
- Treat evidence presence, freshness, and attribution as input validation, not as a third compatibility gate.
- Visible statement: `The compiler supplies the facts; the declared contract gives those facts scope.`

### 47. Permit native bytes only inside a closed contract

- Present four design-review takeaways:

  ```text
  1. Declare C = (R,V,E,P).
  2. Check Admission and Agreement separately.
  3. Keep every Permit per-type, per-profile, and contract-scoped.
  4. Re-close finite changes; change profiles explicitly;
     use an explicit representation when C cannot stay closed.
  ```

- Scope note: `Representation compatibility—not semantic compatibility or schema evolution.`
- Demo callback: `Different profile. Same two gates. Separate Permit.`
- Final statement: `Permit native bytes only inside a closed contract.`
- Keep the GitHub URL and a Q&A cue to appendix slide 48 in the footer.
- Do not end on a generic “Thank you” slide.

## 6. Appendix Design

The appendix contains 16 slides after the 47-slide main deck:

| Output | Question / topic | Primary source pattern |
|---:|---|---:|
| 48 | Q&A map | 37 |
| 49 | Why not `has_unique_object_representations`? | 38 |
| 50 | Padding locations versus padding contents | 39 |
| 51 | Implicit lifetime, storage, alignment, overlap, and synchronization | 40 |
| 52 | Endianness and why byte swapping is conversion | 41 |
| 53 | Why the global `[64-le]` envelope is conservative | 42 |
| 54 | `char`, `bool`, `wchar_t`, and floating-point assumptions | 44 |
| 55 | Opaque type trust boundary | 43 |
| 56 | Full supported / assumed / rejected matrix | 44 |
| 57 | Full signature grammar and recursive engine pseudocode | 48 |
| 58 | Complete difficult-case encodings | 45 |
| 59 | Diagnostic report anatomy | 46 |
| 60 | Artifact format versus CI provenance | 51 |
| 61 | Portable-capture demo: types, artifacts, and exact verdicts | 53 |
| 62 | Ordinary copy versus whole-region relocation | 54 |
| 63 | C++29 relocation status and project-policy limits | 55 |

Appendix slide 61 owns the portable-capture details deliberately removed from Stage 5: the complete positive and negative type declarations, the two separate exporter registries, the per-producer artifact bundle, the no-padding fixture assertions, the retained generated signatures, the three-node/three-edge verdict table, and the exact success condition. Appendix slide 62 owns the complete profile comparison, region-container implementation, full relocatable-world type graph, native matrix, validation algorithm, and the distinction between build/CI permission and runtime validation.

Material promoted to the main deck must not remain duplicated in the appendix. Appendix versions should contain additional detail, not a second copy of the same conclusion.

## 7. Primary Source-Slide Mapping

The final template frame map must resolve exact element IDs after complete source-deck inspection. The intended primary source slide for each output is:

| Output | Source | Output | Source | Output | Source |
|---:|---:|---:|---:|---:|---:|
| 1 | 1 | 22 | 52 | 43 | 34 |
| 2 | 4 | 23 | 17 | 44 | 35 |
| 3 | 2 | 24 | 15 | 45 | 6 |
| 4 | 2 | 25 | 18 | 46 | 26 |
| 5 | 3 | 26 | 50 | 47 | 36 |
| 6 | 2 | 27 | 19 | 48 | 37 |
| 7 | 2 | 28 | 21 | 49 | 38 |
| 8 | 3 | 29 | 23 | 50 | 39 |
| 9 | 6 | 30 | 24 | 51 | 40 |
| 10 | 5 | 31 | 51 | 52 | 41 |
| 11 | 14 | 32 | 25 | 53 | 42 |
| 12 | 8 | 33 | 26 | 54 | 44 |
| 13 | 9 | 34 | 27 | 55 | 43 |
| 14 | 10 | 35 | 28 | 56 | 44 |
| 15 | 48 | 36 | 29 | 57 | 48 |
| 16 | 12 | 37 | 30 | 58 | 45 |
| 17 | 52 | 38 | 2 | 59 | 46 |
| 18 | 45 | 39 | 21 | 60 | 51 |
| 19 | 13 | 40 | 26 | 61 | 53 |
| 20 | 13 | 41 | 40 | 62 | 54 |
| 21 | 13 | 42 | 44 | 63 | 55 |

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

- the main deck contains the approved 47-slide cumulative argument;
- the appendix contains slides 48–63 as a 16-question support structure, for 63 slides total;
- the opening `Measurement` question is answered on slide 37, reframed as a contract question on slide 45, and resolved by the final rule on slide 47;
- signature generation is the longest single technical chapter and establishes its trust properties;
- the three boundary scenarios each state which assumption is retained and which is lost;
- the stored-region case is previewed on Slide 7, recalled briefly on Slides 8, 9, 24, and the Slide 34 transition, expanded into server-to-server and server-to-declared-native-client placements on Slide 38, demonstrated fully on Slides 38–42, and resolved on Slides 45 and 47;
- the world-region thread never displaces the small teaching examples: `PacketHeader` explains signatures, `UnsafeWithPointer` isolates Admission, and `Measurement` isolates real ABI Agreement drift;
- stages 1–7 each preserve one causal spine and defer non-essential inventories, variants, and optimizations;
- Stage 5 first completes the ordinary-copy contract and its two required rejection modes, then explicitly changes to `whole_region_relocation`, separates build/CI permission from runtime validation, and shows the practical payoff for declared server and native-client consumers;
- Stage 5 keeps packed-`Entity` drift supplemental, retains `Measurement` as the real ABI-divergent example, and labels corrupt-offset rejection as runtime validation rather than a third TypeLayout gate;
- Stage 6 briefly bounds both profile-specific Permits, compresses application-owned runtime obligations, re-closes finite build/edge changes, and directs open-ended or representation-broader requirements to an explicit representation layer;
- Stage 7 uses three distinct slides to restate the problem, compress the complete method, and deliver an actionable takeaway rather than leaving the audience on a list of limitations;
- Stage 7 closes both running examples and states `Different profile. Same two gates. Separate Permit.` without broadening either Permit;
- slides 21 and 26 fulfill the promised compile-time checks with current `layout_match` and Admission primitives, while slides 34–37 and 40–42 expose the corresponding CI and runtime diagnostic shapes;
- Agreement, Admission, and Permit are visually and verbally distinct;
- slide 28 establishes only `EdgePass`, while slides 29–34 establish or reject a per-key Permit over the complete declared build graph;
- ordinary copy and relocation never share one unqualified permit;
- no slide displays a repository API that does not exist at the implementation baseline;
- CI claims are explicitly finite and provenance-bound, with provenance presented as evidence validation rather than a third compatibility gate;
- portable-capture and relocatable-world implementation details remain in their implementation notes and appendix; Stage 5 shows only the evidence required for its audience-facing claims;
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
- Stage 4 distinguishes `INCOMPLETE` input evidence from an evaluated `REJECT`; neither state issues a Permit.
- Provenance establishes whether evidence may enter the two-gate decision and is never presented as a third gate.
- Stage 5 preserves per-key decisions for both contracts: each four-key permitted set is project policy over four independent Permits, not a new aggregate compatibility predicate.
- Stage 6 introduces no new gate or mechanism; it limits the conclusion established by Stage 5 and does not treat finite contract expansion as an automatic serialization requirement.
- Stage 7 introduces no new proof obligation; slides 45–47 form the explicit chain problem recap → method recap → actionable takeaway.
- The production `R_capture` set contains only positive boundary types; pointer and `long double` alternatives are separate expected-rejection fixtures under the same build graph and profile.
- The two demos never share one unqualified Permit: the fixed capture uses `ordinary_copy`, while the world region uses `whole_region_relocation` and its complete-region invariant.
- The world region carries the practical question and payoff across the talk; its two application placements, containers, graph, validation, and business details remain concentrated in Stage 5.
- Admission is always described as a per-build compile-time decision, Agreement as a verification-build/CI comparison, and actual region validation as runtime work.
- Opaque support is consistently described as trust rather than complete evidence.
- The deck uses actual repository primitives unless future implementation work adds a tested convenience API.
- The 47-slide main deck targets approximately 55–56 minutes, with the signature chapter remaining the longest technical explanation and the practical demo receiving enough time to establish value.
- Slide 47 resolves the opening question and ends on an operating rule rather than an implementation detail.
