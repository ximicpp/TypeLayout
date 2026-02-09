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

Every C++ developer has written `static_assert(sizeof(MyStruct) == 64)`. It works — until someone adds a field and forgets to update the assert. Or until a different platform silently adds 4 bytes of padding. Or until two independently compiled modules disagree on the same struct's layout, and your shared memory silently corrupts data.

**Before** — manual, incomplete, and O(n) per field:

```cpp
// You write this for EVERY struct, EVERY field, EVERY version...
static_assert(sizeof(PacketHeader) == 24);
static_assert(offsetof(PacketHeader, magic)     == 0);
static_assert(offsetof(PacketHeader, version)   == 4);
static_assert(offsetof(PacketHeader, flags)     == 8);
static_assert(offsetof(PacketHeader, length)    == 12);
static_assert(offsetof(PacketHeader, timestamp) == 16);
// ...and hope nobody adds a field without updating these.
```

**After** — automatic, complete, and always correct:

```cpp
// One line. All fields. All padding. All offsets. Compiler-verified.
static_assert(layout_signatures_match<SenderPacket, ReceiverPacket>());
```

What if the compiler could automatically verify your type's *complete* memory layout — every field offset, every byte of padding, every alignment constraint — at compile time, with zero runtime cost?

This talk introduces **Boost.TypeLayout**, a header-only library that uses C++26 static reflection (P2996) to generate deterministic, human-readable **type layout signatures**. That single line above gives you a complete, compiler-verified proof that two types are byte-compatible — covering all fields, all padding, all inheritance, all bit-fields. No hand-written asserts. No external tools. No runtime overhead.

We'll explore the two-layer signature system (Layout for byte identity, Definition for structural identity), demonstrate real-world applications (IPC, plugins, cross-platform file formats), walk through the formal correctness proofs, and show how the cross-platform toolchain lets you verify struct compatibility across Linux, macOS, and Windows — without needing P2996 on every machine.

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
- V3 Projection Theorem: `definition_match ⟹ layout_match` (mathematically proven)
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

- Soundness: signature match ⟹ memcmp-compatible (zero false positives)
- Encoding Faithfulness: signatures are injective (different layouts → different signatures)
- Strict Refinement: Definition is strictly more precise than Layout
- Why formal proofs matter for a safety-critical verification tool

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

5. **Open source + Boost track**: Already structured for Boost submission, with CI, formal proofs, and cross-platform toolchain.

---

## Speaker Bio

*(需要根据实际情况填写)*

[Speaker Name] is a C++ developer with experience in [relevant areas: systems programming, embedded, high-performance computing]. They are the author of Boost.TypeLayout, a compile-time type layout verification library based on C++26 static reflection. Their work focuses on applying language-level reflection to solve practical safety problems in systems programming.

---

## Supplementary Materials

- **GitHub**: https://github.com/ximicpp/TypeLayout
- **Formal Proofs**: `PROOFS.md` — 900+ lines of denotational semantics proofs
- **Application Analysis**: `APPLICATIONS.md` — 1100+ lines covering 6 real-world scenarios
- **Cross-Platform Demo**: `example/compat_check.cpp` — 3-platform comparison with pre-generated signatures
