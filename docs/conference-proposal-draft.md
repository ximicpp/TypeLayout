# Conference Proposal Draft

## Title

**When C++ Types Cross Boundaries: Checking Layout and Byte Transport at Build Time**

## Format

Standard session (60 minutes including Q&A)

## Abstract

A C++ type stops being just a local implementation detail when it crosses a boundary: processes, binaries, machines, or storage. The same struct can become an IPC message, a protocol header, a mapped file record, or another binary contract. At that point, C++ gives us little build-time proof that two targets will lay out those bytes the same way, or that moving them as raw bytes is even sound. A type can be trivially copyable and still be unsafe to transport. It can also look harmless in source and still compile differently on another platform.

P2996-style C++26 reflection is beginning to land in real toolchains, making a new kind of build-time verification practical. If the compiler can enumerate fields, base classes, offsets, and bit-fields, it can emit a layout signature we can compare during the build. That signature, together with a few safety rules and explicit contracts for opaque types, lets us ask two questions: do two targets agree on the same captured layout, and is the type suitable for byte-level transport at all?

This session follows one end-to-end workflow rather than touring reflection features: generate signatures on target platforms, compare them in a verification build, and surface results in CI with `static_assert`, reporting, or a small wrapper around the generated checks. We will look at three concrete outcomes: a safe fixed-width type, a type whose signature matches but still contains unsafe pointers, and a type that diverges across platforms despite looking reasonable in source. The intended takeaway is a reusable method for codebases that reuse C++ structs across process, binary, or storage boundaries.

We will also make the method's limits explicit. It does not prove semantic compatibility. It tells us whether two targets agree on the captured layout and whether byte transport meets a clear set of assumptions. We will cover practical limits around virtual inheritance, opaque types that require explicit contracts, and implementation-defined fields such as `long`, `wchar_t`, and `long double`.

## Key Takeaways

- Learn when C++ types are genuinely suitable for byte-level transport, and why `trivially_copyable` and `sizeof` checks are not enough.
- See how C++26 reflection can construct a compile-time layout signature, and how that signature plus a small safety rule set supports both layout comparison and transport-safety analysis.
- Leave with a CI-friendly verification workflow: export signatures on target platforms, aggregate generated headers, and surface mismatches with `static_assert`, reporting, or a thin wrapper around the generated checks.

## Outline

### 1. Where C++ types become interfaces

- Why the same C++ structs end up at process, binary, machine, and storage boundaries
- The two recurring failure modes: transport-unsound bytes and cross-platform representation drift
- Why existing checks catch only fragments of the real problem

### 2. What the compiler already knows

- Using reflection to enumerate fields, bases, offsets, and bit-fields
- Flattening C++ object layouts into a compile-time signature with size, alignment, and offset information
- Why one layout signature, combined with a small safety rule set and explicit opaque-type contracts, can drive both checks

### 3. From one description to two checks

- Layout agreement as a direct signature comparison
- How transport safety is derived from that description and a small safety rule set
- Three concrete examples: fixed-width safe type, pointer-containing type with matching representation, and platform-divergent type

### 4. The workflow in practice

- Export signatures on each target platform
- Aggregate generated headers in a verification build
- Use `static_assert`, reporting, or a thin wrapper around the generated checks to surface mismatches in CI when representations diverge or preconditions are violated

### 5. What the method cannot promise

- What this method proves, and what it intentionally does not prove
- Handling virtual inheritance, opaque types with explicit contracts, and implementation-defined fields
- Guidelines for designing C++ types that survive cross-platform transport

## Reviewer Notes

- This is not a general reflection overview. It is a concrete C++26 reflection application aimed at a real systems problem: checking build-time byte-level representation and transport properties of C++ types used across boundaries.
- The talk includes real code, generated artifacts, and multi-target verification examples rather than slides that stay at the language-feature level; some example artifacts are simulated where a target toolchain is not yet part of the live demo path.
- The material is intended for C++ teams that reuse structs at process, binary, machine, or storage boundaries; networking and shared-memory examples are only one slice of the use cases.
- The core workflow is backed by reflection-capable implementations that are available today in limited toolchains, but the talk is framed around the method and engineering workflow, not around claiming uniform toolchain coverage or launching a product or library.
