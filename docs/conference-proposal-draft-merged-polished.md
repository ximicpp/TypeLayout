# Conference Proposal Draft

## Title

**Can I memcpy This Type Across a Boundary? Verifying Object Representation at Compile Time with C++26 Reflection**

## Format

Standard session (60 minutes including Q&A)

## Abstract

C++ teams sometimes move native object bytes across controlled boundaries -- shared-memory IPC, plugin interfaces, persistent storage, cross-target systems code. Yet the language offers little compile-time proof that two targets produce the same object representation, or that the type satisfies explicit preconditions for raw-byte transport. A type can be trivially copyable and still fail those preconditions, and a type that looks harmless in source can compile to a different representation on another platform.

This talk presents a compile-time layout-signature technique built on C++26 reflection (P2996) that operates directly on native C++ types -- no IDL, no generated stubs, no runtime overhead. Reflection enumerates fields, base classes, sizes, alignments, offsets, and bit-fields, from which it derives a deterministic signature that encodes architecture, endianness, leaf-type tokens with sizes and alignments, absolute offsets, and bit-field layout. The same machinery also enforces transport rules, rejecting pointer-like, polymorphic, and other non-transportable cases unless an explicit contract says otherwise. We walk through an end-to-end workflow -- export signatures, compare them with `static_assert`, and use an optional CI reporter to surface mismatches -- covering a fixed-width type that passes, a pointer-containing type with matching representation, and a platform-divergent type.

If signatures match across targets and the type passes transport precondition checks, raw-byte transport is safe. The method does not establish semantic compatibility -- matching bytes do not guarantee matching meaning.

## Key Takeaways

- Learn why `trivially_copyable` and `sizeof` are necessary but not sufficient when native object bytes cross a boundary.
- See how C++26 reflection (P2996) builds a deterministic compile-time signature for comparing object representation across targets. The same machinery enforces explicit transport preconditions.
- Leave with a CI-friendly workflow: export signatures per target, aggregate them in a verification build, and fail early on representation mismatches or transport-policy violations.

## Outline

### 1. Where native types become interfaces

- Why teams reuse native C++ structs at controlled byte-oriented boundaries
- The two recurring failure modes: representation drift across targets and types that are inherently unsafe to transport
- Why `trivially_copyable`, `sizeof`, and ad hoc checks still leave blind spots

### 2. How the signature is built

- Using reflection to enumerate fields, bases, offsets, sizes, alignments, and bit-fields
- Flattening nested structs and base subobjects into a deterministic compile-time form with absolute offsets
- What the signature encodes: architecture and endianness, leaf-type tokens, sizes, alignments, absolute offsets, bit-fields, and pointer-like markers
- Edge cases: arrays, empty bases, virtual inheritance, and opaque types

### 3. What the checks tell us

- Representation comparison: verifying agreement or surfacing mismatches across targets
- Transport precondition checks: which types are safe to transport and which are rejected
- Three concrete examples: a fixed-width type that passes, a pointer-containing type with matching representation, and a platform-divergent type

### 4. The workflow in practice

- Each target platform exports `.sig.hpp` headers for a selected set of boundary types
- A verification build aggregates exported signatures and checks representation agreement via `static_assert`
- CI integration: optional reporter diagnostics that pinpoint mismatched types and differing fields

### 5. What the method does not cover

- Semantic compatibility: matching representation does not guarantee matching meaning or behavioral equivalence
- Schema evolution, versioning, and general-purpose persistence formats are out of scope

## Reviewer Notes

- This is not a general reflection overview. It is a concrete application of C++26 reflection (P2996) to a real systems problem: checking object representation and explicit transport preconditions for native C++ types used across boundaries.
- The talk includes real code, exported signature artifacts, and cross-target verification examples rather than staying at the language-feature level.
- The intended audience is C++ teams that exchange native object bytes across controlled boundaries such as shared-memory IPC, plugin interfaces, persistent storage, and cross-target builds.
- The live workflow uses GCC 16 (which merged P2996 into trunk in January 2026) via Docker on Linux, with a second target's exported signatures verified in the same build. macOS with an experimental Clang fork serves as an alternative demo path.
