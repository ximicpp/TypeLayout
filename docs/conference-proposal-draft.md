# Conference Proposal Draft

## Title

**Can We Verify C++ Types Across Boundaries? Build-Time Layout and Transport Checks with C++26 Reflection**

## Format

Standard session (60 minutes including Q&A)

## Abstract

C++ types routinely cross boundaries: between processes, between machines, between plugins, and between memory and storage. Teams reuse them for IPC, protocol headers, persistent data, and mapped files, yet the language gives us little build-time proof that those bytes still describe the same layout, or are even safe to move across the boundary as raw bytes. A type can be trivially copyable and still be unsafe to transport. A type can also look ordinary in source code and still compile to a different layout on another platform, compiler, or data model.

This talk shows how current P2996-style C++26 reflection implementations make a new kind of build-time verification practical. If the compiler can enumerate fields, base classes, offsets, and bit-fields, it can help us describe a C++ type's object layout in a form that can be checked during the build. That layout representation, together with a small set of admission rules, answers two practical questions: do two targets produce the same layout, and is the type suitable for byte-level transport at all?

The session is deliberately framed as an engineering workflow, not as a reflection overview and not as a project introduction. We will show how the checks are derived, how signatures are exported on target platforms, how generated headers are aggregated in a verification build, and how CI can fail when layouts diverge or transport preconditions are violated. Attendees will see three concrete outcomes: a safe fixed-width type, a type whose layout matches but still contains unsafe pointers, and a type that diverges across platforms despite looking reasonable in source.

We will also make the method's boundaries explicit. It does not promise semantic compatibility; it checks byte-layout compatibility and byte-transport preconditions under explicit assumptions. We will cover the practical limits around virtual inheritance, opaque types, and implementation-defined fields such as `long`, `wchar_t`, and `long double`. The goal is to leave attendees with a reusable build-time verification method for C++ types used across boundaries, not just an explanation of a language feature.

## Key Takeaways

- Learn when C++ types are genuinely suitable for byte-level transport, and why `trivially_copyable` and `sizeof` checks are not enough.
- See how C++26 reflection can construct a compile-time layout signature that supports both layout comparison and transport-safety analysis.
- Leave with a CI-friendly verification workflow: export signatures on target platforms, aggregate generated headers, and fail builds on mismatch.

## Outline

### 1. Where C++ types break across boundaries

- Why teams reuse C++ structs at process, machine, plugin, and storage boundaries
- The two recurring failure modes: transport-unsound bytes and cross-platform layout drift
- Why existing checks catch only fragments of the real problem

### 2. What reflection gives us at build time

- Using reflection to enumerate fields, bases, offsets, and bit-fields
- Flattening C++ object layouts into a compile-time signature with size, alignment, and offset information
- Why one layout representation, combined with a small rule set, supports multiple build-time checks

### 3. One representation, two answers

- Layout equality as a direct signature comparison
- Transport safety as a property derived from the layout representation together with a small admission rule set
- Three concrete examples: fixed-width safe type, pointer-containing type with matching layout, and platform-divergent type

### 4. Turning the checks into a CI gate

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
- The demos are backed by a working prototype implementation, but the talk is framed around the method and engineering workflow, not around launching a product or library.
