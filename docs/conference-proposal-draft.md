# Conference Proposal Draft

## Title

**Can I Safely memcpy This Type? Compile-Time Layout Verification with C++26 Reflection**

## Format

Standard session (60 minutes including Q&A)

## Abstract

C++ types routinely cross boundaries -- processes, machines, plugins, storage -- yet the language gives us little build-time proof that two targets agree on layout, or that moving those bytes is even sound. A type can be trivially copyable and still unsafe to transport; it can look harmless in source and compile to a different layout on another platform.

This talk presents compile-time layout signatures built on C++26 reflection (P2996). If the compiler can enumerate fields, base classes, offsets, and bit-fields, it can emit a signature we compare during the build. That signature, together with a small safety rule set, answers two questions at build time: do two targets agree on the same layout, and is the type suitable for byte-level transport? We follow one end-to-end workflow -- generate signatures on target platforms, compare them in a verification build, and wire the checks into CI -- with three concrete outcomes: a safe fixed-width type, a type whose signature matches but still contains unsafe pointers, and a type that diverges across platforms despite looking reasonable in source.

We will also make the method's limits explicit: it proves layout agreement and transport-safety assumptions, not semantic compatibility. We cover practical limits around virtual inheritance, opaque types, and implementation-defined fields such as `long`, `wchar_t`, and `long double`.

## Key Takeaways

- Learn when C++ types are genuinely suitable for byte-level transport, and why `trivially_copyable` and `sizeof` checks are not enough.
- See how C++26 reflection can construct a compile-time layout signature, and how that one representation supports both layout comparison and transport-safety analysis -- no IDL, no code generation, no runtime overhead.
- Leave with a CI-friendly verification workflow: export signatures per platform, aggregate them, and fail builds on mismatch.

## Outline

### 1. Where C++ types become interfaces

- Why the same C++ structs end up at process, binary, machine, and storage boundaries
- The two recurring failure modes: transport-unsound bytes and cross-platform representation drift
- Why existing checks -- `trivially_copyable`, `sizeof`, even IDL-based schemes -- each catch only a fragment of the real problem

### 2. How a layout signature is built

- Using reflection to enumerate fields, bases, offsets, and bit-fields
- Recursively flattening nested structs and base classes into one compile-time string with absolute offsets
- What the signature encodes: leaf types, sizes, alignments, offsets, and pointer-like tokens

### 3. What the signature tells us

- Layout agreement: comparing signatures across platforms
- Transport safety: scanning the signature for pointer tokens
- Three concrete examples: fixed-width safe type, pointer-containing type with matching layout, and platform-divergent type

### 4. The workflow in practice

- Export signatures on each target platform
- Aggregate generated headers in a verification build
- Use generated checks, `static_assert`, and reporting to surface mismatches in CI when representations diverge or preconditions are violated

### 5. What the method cannot promise

- What this method proves, and what it intentionally does not prove
- Handling virtual inheritance, opaque types with explicit contracts, and implementation-defined fields
- Guidelines for designing C++ types that survive cross-platform transport

## Reviewer Notes

- This is not a general reflection overview. It is a concrete C++26 reflection application aimed at a real systems problem: checking build-time byte-level representation and transport properties of C++ types used across boundaries.
- The talk includes real code, generated artifacts, and multi-target verification examples rather than slides that stay at the language-feature level; some example artifacts are simulated where a target toolchain is not yet part of the live demo path.
- The material is intended for C++ teams that reuse structs at process, binary, machine, or storage boundaries; networking and shared-memory examples are only one slice of the use cases.
- The core workflow is backed by reflection-capable implementations that are available today in limited toolchains, but the talk is framed around the method and engineering workflow, not around claiming uniform toolchain coverage or launching a product or library.
- Live demos have pre-recorded fallbacks; cross-platform artifacts are pre-generated where a second target is not available on stage.
