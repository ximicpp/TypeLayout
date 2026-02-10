# Conference Session Proposal

---

## Title

**"Your Struct is Lying to You: Compile-Time Layout Verification with C++26 Reflection"**

## Session Type

Regular Session (60 minutes)

## Session Level

Intermediate

## Tags

`reflection`, `P2996`, `compile-time`, `ABI`, `type safety`, `zero-copy`, `cross-platform`

---

## Abstract

Every C++ developer has written `static_assert(sizeof(MyStruct) == 64)` — and watched it break when someone adds a field, when a different platform inserts padding, or when two modules silently disagree on the same struct's layout. Manual `sizeof`/`offsetof` asserts are O(n) per field, cannot compare two types, and have no cross-platform story.

This talk introduces **TypeLayout**, a header-only C++26 library that replaces those fragile, per-field asserts with a single `static_assert` that automatically verifies the *complete* memory layout of any type — every field offset, every byte of padding, every alignment constraint, every level of inheritance — at compile time, with zero runtime cost.

TypeLayout uses P2996 static reflection to generate deterministic, human-readable **type layout signatures**: compact strings that encode everything the compiler knows about a type's layout. A two-layer signature system separates byte-level identity (Layout layer: flattened, nameless — "are these bytes the same?") from structural identity (Definition layer: preserving field names, inheritance hierarchy, and enum qualified names — "do these types mean the same thing?"). A Projection Theorem formally guarantees that a Definition match strictly implies a Layout match, giving developers a clear decision rule for IPC, plugin systems, file formats, and ODR violation detection.

We will demonstrate real-world applications with live cross-platform comparisons (Linux x86_64, macOS ARM64, Windows x64), walk through the denotational semantics proofs that back the library's zero-false-positive guarantee, and show how the cross-platform toolchain lets you verify struct compatibility across platforms — without needing P2996 on every machine.

## Outline

**Part 1: The Problem (10 min)**

*"Your struct is lying to you."*

- Live demo: the same `struct PacketHeader` compiled on Linux x86_64, macOS ARM64, and Windows x64 — three different layouts from the same source code
- Why `static_assert(sizeof(T) == N)` is necessary but insufficient:
  - Doesn't check field offsets
  - Doesn't detect new fields
  - Maintenance cost is O(n) per field
  - Can't compare two types
- Real-world horror stories: IPC corruption, plugin crashes, file format incompatibility

**Part 2: The Solution — Type Layout Signatures (15 min)**

*"Let the compiler tell the truth."*

- How P2996 reflection gives us `nonstatic_data_members_of`, `offset_of`, `bases_of` — the raw materials
- TypeLayout's contribution: aggregate these into a **single deterministic string** at compile time
- Signature anatomy:
  ```
  [64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8]}
  ```
- Live coding: from `struct Message` to its signature in 3 lines of code
- The entire public API — 4 functions:
  ```cpp
  get_layout_signature<T>();
  get_definition_signature<T>();
  layout_signatures_match<T, U>();
  definition_signatures_match<T, U>();
  ```

**Part 3: Two Layers — Layout vs Definition (10 min)**

*"Same bytes, different meaning."*

- Why one layer isn't enough:
  ```cpp
  struct v1 { uint32_t timeout_ms; };
  struct v2 { uint32_t timeout_seconds; }; // renamed!
  // Layout: MATCH (same bytes) — but the semantics drifted!
  // Definition: MISMATCH — caught!
  ```
- Layout layer: flattens inheritance, strips names → pure byte identity
- Definition layer: preserves names, inheritance tree, enum qualified names → structural identity
- Projection Theorem: `definition_match ⟹ layout_match` (formally proven, zero exceptions)
- Decision rule: "When in doubt, use Definition."

**Part 4: Real-World Applications (10 min)**

*"Where signatures save you."*

- **IPC / Shared memory**: embed hash, verify on connect
  ```cpp
  static_assert(layout_signatures_match<Writer::Data, Reader::Data>());
  ```
- **Plugin systems**: export signature via `dlsym`, verify at load time
- **Cross-platform file formats**: Layout match + Safety classification = zero-copy decision
- **ODR violation detection**: the only compile-time tool that catches "same name, different definition" across compilation units
- Live demo: the cross-platform compatibility report across 3 platforms

**Part 5: Formal Correctness (5 min)**

*"Not just tested — proven."*

- Soundness: signature match ⟹ memcpy-compatible (zero false positives under stated assumptions)
- Encoding Faithfulness: signatures are injective (different layouts → different signatures)
- Projection Theorem + Strictness: Definition match strictly implies Layout match, but not vice versa — two layers are genuinely distinct
- Why denotational semantics proofs matter for a safety-critical verification tool

**Part 6: Cross-Platform Toolchain (5 min)**

*"P2996 on one machine. Verification everywhere."*

- Two-phase architecture:
  - Phase 1: export signatures (needs P2996)
  - Phase 2: compare signatures (any C++17 compiler)
- `.sig.hpp` files: portable signature snapshots
- `TYPELAYOUT_CHECK_COMPAT(linux, macos, windows)` — one-line multi-platform check
- CI/CD integration: compile-time ABI guard

**Part 7: Q&A (5 min)**

---

## Key Takeaways

1. `sizeof`/`offsetof` are necessary but manually incomplete — TypeLayout automates them completely
2. P2996 reflection enables a new category of compile-time safety tools
3. Two-layer signatures solve both byte compatibility (IPC) and semantic compatibility (serialization)
4. Cross-platform struct verification is now possible without serialization frameworks
5. Formal proofs give the library a level of correctness assurance rare in C++ libraries

## Target Audience

- **Primary**: C++ developers working with IPC, shared memory, network protocols, plugin systems, or binary file formats
- **Secondary**: Library authors interested in P2996 reflection applications
- **Tertiary**: Language designers interested in how reflection enables new safety guarantees

## Prerequisites

- Familiarity with `sizeof`, `alignof`, `offsetof`
- Basic understanding of memory layout (padding, alignment)
- No P2996 knowledge required (will be explained)

## Why This Talk Matters

1. **P2996 is coming**: C++26 static reflection is the most anticipated feature. This talk shows a real, working application — not a toy demo.

2. **Practical safety**: Unlike academic reflection examples, TypeLayout solves a real problem (layout corruption) that every systems programmer has encountered.

3. **Live demos**: The talk features live cross-platform comparisons with real compiler output, not slides-only.

4. **Formal rigor**: The library comes with denotational semantics proofs — unusual for a C++ library, and a model for how reflection-based tools should be validated.

5. **Reproducible and self-contained**: The library is header-only with zero dependencies, and all denotational semantics proofs are included in the repository — every claim in this talk can be independently verified.

**What attendees will take home:**
- A mental model for reasoning about C++ type layout across platforms
- A ready-to-use library for compile-time layout verification in their own projects
- An understanding of how P2996 reflection can be applied beyond toy examples to build real safety tools

---

## Alternative: 30-Minute Condensed Version

If a shorter session is preferred, the talk can be condensed to 30 minutes with the following structure:

| Part | Topic | Time |
|------|-------|------|
| 1 | The Problem — live demo + why sizeof is insufficient | 5 min |
| 2 | The Solution — signatures + API | 10 min |
| 3 | Two Layers + Projection Theorem | 7 min |
| 4 | Cross-Platform Toolchain + one application demo | 5 min |
| 5 | Q&A | 3 min |

The formal correctness and detailed application sections would be condensed into brief references, with pointers to the supplementary materials for interested attendees.

---

## Supplementary Materials

*(All materials available upon acceptance; anonymous repository link provided to reviewers on request.)*

- **Formal Proofs**: 900+ lines of denotational semantics proofs (Soundness, Injectivity, Projection Theorem)
- **Application Analysis**: 1100+ lines covering 6 real-world scenarios (IPC, plugins, file formats, ODR detection)
- **Cross-Platform Demo**: 3-platform comparison with pre-generated signatures
