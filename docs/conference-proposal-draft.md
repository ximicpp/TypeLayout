# Conference Proposal Draft

## Title

**When C++ Types Cross Boundaries: Build-Time Checks for Layout and Byte Transport**

## Format

Standard session (60 minutes including Q&A)

## Abstract

C++ types stop being local implementation details the moment they cross a boundary: across processes, binaries, machines, and storage. The same structs often get reused in interfaces, files, protocol headers, and mapped data, yet the language gives us little build-time proof that those bytes still describe the same layout, or are even safe to move as raw bytes. A type can be trivially copyable and still be unsafe to transport. A type can also look ordinary in source and still compile to a different layout on another platform.

This talk shows how P2996-style C++26 reflection, now landing in real toolchains, makes a new kind of build-time verification practical. If the compiler can enumerate fields, base classes, offsets, and bit-fields, it can describe a C++ type's layout in a form checked during the build. From that description, plus a small set of safety rules, we can answer two practical questions: do two targets produce the same layout, and is the type suitable for byte-level transport at all?

The session focuses on one end-to-end workflow, not a reflection tour: generate signatures on target platforms, compare them in a verification build, and let CI fail when layouts diverge or safety rules are violated. We will use real code and three concrete examples: a safe fixed-width type, a type whose layout matches but still contains unsafe pointers, and a type that diverges across platforms despite looking reasonable in source. You do not need to work on networking or shared memory to run into this problem; any codebase that reuses ordinary structs across build, binary, or storage boundaries can benefit from the method.

We will also make the method's boundaries explicit. This method does not prove semantic compatibility. It checks byte-layout compatibility and the assumptions required for byte transport. We will cover practical limits around virtual inheritance, opaque types, and implementation-defined fields such as `long`, `wchar_t`, and `long double`.

## Key Takeaways

- Learn when C++ types are genuinely suitable for byte-level transport, and why `trivially_copyable` and `sizeof` checks are not enough.
- See how C++26 reflection can construct a compile-time layout signature that supports both layout comparison and transport-safety analysis.
- Leave with a CI-friendly verification workflow: export signatures on target platforms, aggregate generated headers, and fail builds on mismatch.

## Outline

### 1. Where C++ types stop being local details

- Why the same C++ structs end up at process, machine, plugin, and storage boundaries
- The two recurring failure modes: transport-unsound bytes and cross-platform layout drift
- Why existing checks catch only fragments of the real problem

### 2. What reflection gives us at build time

- Using reflection to enumerate fields, bases, offsets, and bit-fields
- Flattening C++ object layouts into a compile-time signature with size, alignment, and offset information
- Why one layout description can drive both checks

### 3. One representation, two answers

- Layout equality as a direct signature comparison
- How transport safety is derived from that description and a small safety rule set
- Three concrete examples: fixed-width safe type, pointer-containing type with matching layout, and platform-divergent type

### 4. One end-to-end CI workflow

- Export signatures on each target platform
- Aggregate generated headers in a verification build
- Use `static_assert` and reporting to fail CI when layouts diverge or preconditions are violated

### 5. What this method can and cannot prove

- What this method proves, and what it intentionally does not prove
- Handling virtual inheritance, opaque types, and implementation-defined fields
- Guidelines for designing C++ types that survive cross-platform transport

## Reviewer Notes

- This is not a general reflection overview. It is a concrete C++26 reflection application aimed at a real systems problem: checking build-time layout and transport properties of C++ types used across boundaries.
- The talk includes real code, generated artifacts, and cross-platform examples rather than slides that stay at the language-feature level.
- The material is intended for a broad C++ audience: any team that reuses C++ structs at process, machine, or storage boundaries can apply the workflow, even if they do not work on networking or shared-memory infrastructure full time.
- The demos are backed by working reflection-capable implementations available today, but the talk is framed around the method and engineering workflow, not around launching a product or library.
