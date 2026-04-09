# Conference Proposal Draft

## Title

**When C++ Types Cross Boundaries: A Build-Time Workflow for Layout and Byte Transport**

## Format

Standard session (60 minutes including Q&A)

## Abstract

C++ types stop being local implementation details the moment they cross a boundary: across processes, binaries, machines, and storage. The same struct can end up in an IPC message, a plugin interface, a protocol header, or a mapped file. At that point, C++ gives us little build-time proof that the bytes still have the same layout, or that moving them as raw bytes is even sound. A type can be trivially copyable and still be unsafe to transport. It can also look harmless in source and still compile to a different layout on another platform.

P2996-style C++26 reflection is now landing in real toolchains, making a new kind of build-time verification practical. If the compiler can enumerate fields, base classes, offsets, and bit-fields, it can describe a type's layout in a form we can compare during the build. From that description, plus a small set of safety rules, we can answer two questions: do two targets produce the same layout, and is the type suitable for byte-level transport at all?

This session follows one end-to-end workflow rather than touring reflection features: generate signatures on target platforms, compare them in a verification build, and fail CI when layouts diverge or safety rules are violated. Along the way we will look at three concrete outcomes: a safe fixed-width type, a type whose layout matches but still contains unsafe pointers, and a type that diverges across platforms despite looking reasonable in source. The same workflow applies well beyond networking or shared memory; any codebase that reuses ordinary structs across build, binary, or storage boundaries can benefit from it.

We will also make the method's limits explicit. It does not prove semantic compatibility. It tells us whether layouts match and whether byte transport satisfies a clear set of assumptions. We will cover practical limits around virtual inheritance, opaque types, and implementation-defined fields such as `long`, `wchar_t`, and `long double`.

## Key Takeaways

- Learn when C++ types are genuinely suitable for byte-level transport, and why `trivially_copyable` and `sizeof` checks are not enough.
- See how C++26 reflection can construct a compile-time layout signature that supports both layout comparison and transport-safety analysis.
- Leave with a CI-friendly verification workflow: export signatures on target platforms, aggregate generated headers, and fail builds on mismatch.

## Outline

### 1. Where C++ types stop being local details

- Why the same C++ structs end up at process, machine, plugin, and storage boundaries
- The two recurring failure modes: transport-unsound bytes and cross-platform layout drift
- Why existing checks catch only fragments of the real problem

### 2. What the compiler can tell us at build time

- Using reflection to enumerate fields, bases, offsets, and bit-fields
- Flattening C++ object layouts into a compile-time signature with size, alignment, and offset information
- Why one layout description can drive both checks

### 3. One description, two checks

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
