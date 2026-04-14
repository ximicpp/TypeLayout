# Conference Proposal Draft

## Title

**Can I memcpy This Type Across a Boundary? Verifying Object Representation at Compile Time with C++26 Reflection**

## Format

Standard session (60 minutes including Q&A)

## Abstract

C++ teams sometimes move native object bytes across controlled boundaries -- shared-memory IPC, plugin interfaces, cross-target systems code. Yet the language offers little build-time proof that two targets produce the same object representation, or that the type satisfies explicit preconditions for raw-byte transport. A type can be trivially copyable and still fail those preconditions, and a type that looks harmless in source can compile to a different representation on another platform.

This talk presents a compile-time layout-signature technique built on C++26 reflection (P2996). Reflection enumerates fields, base classes, sizes, alignments, offsets, and bit-fields, from which it derives a deterministic signature encoding architecture, endianness, leaf-type tokens, absolute offsets, and bit-field layout. A second pass applies explicit transport rules, rejecting pointer-like, polymorphic, and other non-transportable cases unless an explicit contract says otherwise. Unlike IDL-driven or serialization-library approaches, the method operates directly on native C++ types with no generated stubs and no runtime overhead. We walk through an end-to-end workflow -- export signatures, compare them with `static_assert`, and use an optional CI reporter to surface mismatches -- covering a fixed-width type that passes, a pointer-containing type with matching representation, and a platform-divergent type.

The method checks representation agreement and stated transport preconditions, but does not establish semantic compatibility. The talk covers practical limits around virtual inheritance, opaque types, and implementation-defined types such as `long`, `wchar_t`, and `long double`.

## Key Takeaways

- Learn why `trivially_copyable` and `sizeof` are necessary but not sufficient when native object bytes cross a boundary.
- See how the current C++26 reflection proposal can build a deterministic compile-time signature for comparing object representation across targets, and how the same machinery can enforce explicit transport preconditions.
- Leave with a CI-friendly workflow: export signatures per target, aggregate them in a verification build, and fail early on representation mismatches or transport-policy violations.

## Outline

### 1. Where native types become interfaces

- Why teams reuse native C++ structs at controlled byte-oriented boundaries
- The two recurring failure modes: cross-target representation drift and locally unsafe raw-byte transport
- Why `trivially_copyable`, `sizeof`, and ad hoc checks still leave blind spots

### 2. How the signature is built

- Using reflection to enumerate fields, bases, offsets, sizes, alignments, and bit-fields
- Flattening nested structs and base subobjects into a deterministic compile-time form with absolute offsets
- What the signature encodes: architecture and endianness, leaf-type tokens, sizes, alignments, absolute offsets, bit-fields, and pointer-like markers
- Supported edge cases: arrays, empty bases, and bit-fields
- Practical limits: opaque types and virtual inheritance

### 3. What the checks tell us

- Representation agreement: comparing signatures across targets
- Transport precondition checks: applying explicit rules to pointer-like, polymorphic, and opaque cases
- Cross-platform differences: exposing implementation-defined and ABI-sensitive types
- Three concrete examples: a fixed-width type that passes, a pointer-containing type with matching representation, and a platform-divergent type

### 4. The workflow in practice

- Each target platform exports `.sig.hpp` headers for a selected set of boundary types
- A verification build on any single platform `#include`s the exported headers and runs `static_assert` for required representation checks
- CI integration: optional reporter diagnostics identify mismatched types, list the specific fields that differ, and highlight character-level signature divergence; builds fail on required representation checks or transport-policy violations
- Incremental adoption: start with a small set of boundary types and expand coverage as the team gains confidence

### 5. What the method cannot promise

- What the method checks, and what it intentionally leaves out
- Semantic compatibility, schema evolution, and general-purpose persistence formats are out of scope
- Practical limits around virtual inheritance, opaque types, and implementation-defined fundamental types

## Reviewer Notes

- This is not a general reflection overview. It is a concrete application of the current C++26 reflection proposal to a real systems problem: checking object representation and explicit transport preconditions for native C++ types used across boundaries.
- The talk includes real code, exported signature artifacts, and cross-target verification examples rather than staying at the language-feature level.
- The intended audience is C++ teams that deliberately exchange native object bytes across controlled boundaries; it is not a general-purpose wire-format or persistence talk.
- The live workflow uses GCC 16 (which merged P2996 into trunk in January 2026) via Docker on Linux, with a second target's exported signatures verified in the same build. macOS with an experimental Clang fork serves as an alternative demo path.
