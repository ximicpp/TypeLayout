# Conference Proposal Draft

## Title

**Can I Safely memcpy This Type? Verifying Layout at Compile Time with C++26 Reflection**

## Format

Standard session (60 minutes including Q&A)

## Abstract

C++ types routinely cross boundaries -- between processes, binaries, machines, and storage formats -- yet the language gives us little build-time proof that two targets agree on byte-level layout, or that those bytes are safe to transport at all. A type can be trivially copyable and still be unsafe to transport, and a type that looks harmless in source can still compile to a different representation on another platform.

This talk presents a compile-time layout-signature technique built on C++26 reflection (P2996). If the compiler can enumerate fields, base classes, offsets, and bit-fields, it can derive a canonical signature that we compare during the build to verify cross-target byte-level representation. That same machinery, together with a small set of explicit safety rules, also tells us whether a type satisfies our stated preconditions for byte-level transport. We walk through one end-to-end workflow -- generate signatures on target platforms, compare them in a verification build, and wire the checks into CI -- and show three concrete outcomes: a fixed-width type that passes, a type whose layout matches but still contains unsafe pointers, and a type that diverges across platforms despite looking reasonable in source.

We also make the method's limits explicit: it proves layout agreement and checks stated transport-safety assumptions; it does not prove semantic compatibility. We cover practical limits around virtual inheritance, opaque types, and implementation-defined fields such as `long`, `wchar_t`, and `long double`.

## Key Takeaways

- Learn what makes a C++ type acceptable for byte-level transport, and why `trivially_copyable` and `sizeof` checks are not enough.
- See how C++26 reflection can construct a compile-time layout signature for verifying cross-target byte-level representation, and how that same machinery also supports transport-safety analysis -- without an external IDL, generated serialization stubs, or runtime verification overhead.
- Leave with a CI-friendly verification workflow: export signatures per platform, aggregate them in a verification build, and fail the build on layout mismatches or violated transport preconditions.

## Outline

### 1. Where C++ types become interfaces

- Why the same C++ structs end up at process, binary, machine, and storage boundaries
- The two recurring failure modes: transport-unsound bytes and cross-platform representation drift
- Why existing checks -- `trivially_copyable`, `sizeof`, and even IDL-based approaches -- each address only part of the problem

### 2. How a layout signature is built

- Using reflection to enumerate fields, bases, offsets, and bit-fields
- Recursively flattening nested structs and base classes into a canonical compile-time representation with absolute offsets
- What the signature encodes: architecture prefix plus the layout-relevant facts this method compares across platforms

### 3. What the signature tells us

- Layout agreement: comparing signatures across platforms
- Transport-safety checks: combining the signature with explicit rules for pointer-like and polymorphic cases, plus explicit contracts for opaque types
- Cross-platform comparison: using signatures to expose implementation-defined differences
- Three concrete examples: a fixed-width type that passes, a pointer-containing type with matching layout, and a platform-divergent type

### 4. The workflow in practice

- Export signatures on each target platform
- Aggregate generated headers in a verification build
- Use generated checks, `static_assert`, and CI reporting to surface layout mismatches or violated transport preconditions

### 5. What the method cannot promise

- What this method proves, and what it intentionally leaves out
- Handling virtual inheritance, explicitly-registered opaque types, and implementation-defined fields

## Reviewer Notes

- This is not a general reflection overview. It is a concrete C++26 reflection application aimed at a real systems problem: checking build-time byte-level representation and transport properties of C++ types used across boundaries.
- The talk includes real code, generated artifacts, and cross-target verification examples rather than staying at the language-feature level; where a target toolchain is not part of the live reflection path, the comparison artifact is pre-generated or simulated explicitly.
- The material is intended for C++ teams that reuse structs at process, binary, machine, or storage boundaries; networking and shared-memory examples are only one slice of the use cases.
- The core workflow is backed by reflection-capable implementations available today, while the cross-target story combines live-capable paths with explicitly pre-generated artifacts where toolchain coverage is still incomplete. The talk is framed around the method and engineering workflow, not around claiming uniform toolchain coverage or launching a product or library.
