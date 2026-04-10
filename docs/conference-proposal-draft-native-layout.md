# Conference Proposal Draft

## Title

**Can I memcpy This Type Across a Boundary? Verifying Native Layout with C++26 Reflection**

## Format

Standard session (60 minutes including Q&A)

## Abstract

C++ teams sometimes choose to move native object bytes across controlled boundaries such as shared-memory IPC, plugin and shared-library interfaces, and cross-target systems code, yet the language gives us little build-time proof that two targets agree on byte-level representation, or that a type meets explicit preconditions for raw-byte transport. A type can be trivially copyable and still violate those preconditions, and a type that looks harmless in source can still compile to a different representation on another platform.

This talk presents a compile-time layout-signature technique built on C++26 reflection (P2996). Using reflection to enumerate fields, base classes, sizes, alignments, offsets, and bit-fields, the compiler derives a normalized signature that we compare during the build to check whether selected targets produce the same native byte-level representation. We then pair that layout check with explicit raw-byte transport rules, so the talk distinguishes two separate outcomes: representation agreement across targets, and transport precondition checks for the type itself. Pointer-like and other non-transportable cases are rejected unless covered by an explicit contract. We walk through one end-to-end workflow -- generate signatures on target platforms, compare them in a verification build, and wire the checks into CI -- and show three concrete outcomes: a fixed-width type that passes, a type whose layout matches but still contains unsafe pointers, and a type that diverges across platforms despite looking reasonable in source.

We also make the method's limits explicit: it checks representation agreement and stated byte-transport preconditions; it does not prove semantic compatibility. We cover practical limits around virtual inheritance, opaque types, and implementation-defined types such as `long`, `wchar_t`, and `long double`.

## Key Takeaways

- Learn what makes a C++ type acceptable for byte-level transport, and why `trivially_copyable` and `sizeof` checks are not enough.
- See how C++26 reflection can construct a compile-time layout signature for verifying cross-target byte-level representation, and how the same machinery can enforce explicit transport preconditions -- without requiring a separate schema language, generated serialization stubs, or runtime verification overhead.
- Leave with a CI-friendly verification workflow: export signatures per platform, aggregate them in a verification build, and fail the build on layout mismatches or violated transport preconditions.

## Outline

### 1. Where C++ types become interfaces

- Why teams sometimes reuse native C++ structs at shared-memory, plugin/shared-library, and cross-target boundaries
- The two recurring failure modes: transport-unsound bytes and cross-platform representation drift
- Why existing checks -- `trivially_copyable`, `sizeof`, and IDL or serialization approaches -- each address only part of the problem

### 2. How a layout signature is built

- Using reflection to enumerate fields, bases, offsets, and bit-fields
- Recursively flattening nested structs and base classes into a normalized compile-time representation with absolute offsets
- What the signature encodes: architecture prefix, leaf-type tokens, sizes, alignments, offsets, and pointer-like markers (e.g., `[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:f64[s:8,a:8]}`)
- Edge cases such as bit-fields, arrays, and empty bases

### 3. What native layout checking tells us

- Layout agreement: comparing signatures across platforms
- Transport precondition checks: combining the layout analysis with explicit rules for pointer-like and polymorphic cases, plus explicit contracts for opaque types
- Cross-platform comparison: using signatures to expose implementation-defined differences
- Three concrete examples: a fixed-width type that passes, a pointer-containing type with matching layout, and a platform-divergent type

### 4. The workflow in practice

- Each target platform exports `.sig.hpp` headers for a selected set of boundary types
- A verification build on any single platform `#include`s all exported headers and runs `static_assert` on signature equality
- CI integration: fail the build when representations diverge or transport-safety preconditions are violated; show reporter output that identifies which types differ and where the layout signature begins to diverge
- Incremental adoption: start with a handful of boundary types, expand coverage as the team gains confidence

### 5. What the method cannot promise

- What this method proves, and what it intentionally leaves out
- Handling virtual inheritance, explicitly-registered opaque types, and implementation-defined types

## Reviewer Notes

- This is not a general reflection overview. It is a concrete C++26 reflection application aimed at a real systems problem: checking build-time byte-level representation and transport properties of C++ types used across boundaries.
- The talk includes real code, generated artifacts, and cross-target verification examples rather than staying at the language-feature level.
- The material is intended for C++ teams that intentionally exchange native object bytes across controlled shared-memory, plugin/shared-library, or cross-target boundaries; it is not a general-purpose wire-format or persistence talk.
- The core workflow is backed by reflection-capable implementations available today; cross-platform artifacts are pre-generated where a second target is not part of the live path. The talk is framed around the method and engineering workflow, not around claiming uniform toolchain coverage.
