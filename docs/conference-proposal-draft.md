# Conference Proposal Draft

## Title

**Can We Trust These Native Types? Build-Time Layout and Transport Verification with C++26 Reflection**

## Format

Standard session (60 minutes including Q&A)

## Abstract

Native C++ types routinely cross boundaries: between processes, between machines, and between memory and storage. Teams reuse structs for IPC, protocol headers, persistent data, plugin boundaries, and mapped files, yet the language gives us little build-time proof that those bytes will still be valid and compatible on the other side. A type can be trivially copyable and still be unsafe to transport. A type can also look ordinary in source code and still compile to a different layout on another platform, compiler, or data model.

This talk shows how C++26 reflection can build compile-time layout signatures for native types by enumerating fields, bases, offsets, and bit-fields. The signature records sizes, alignments, offsets, and pointer-like hazards in one representation. That single representation answers two practical questions at build time: do two targets produce the same layout, and is the type suitable for byte-level transport at all?

We will turn that mechanism into an engineering workflow rather than a language tour. The session will show how transport checks are derived from the signature, how signatures are exported on target platforms, how generated headers are aggregated in a verification build, and how CI can fail when layouts diverge or transport preconditions are violated. Attendees will see three concrete cases: a fixed-width safe record, a type whose layout matches but still contains unsafe pointers, and a type that diverges across platforms despite looking reasonable in source.

We will also cover what this method proves, and what it does not. It does not promise semantic compatibility; it proves byte-layout compatibility and transport assumptions. We will discuss important boundaries, including virtual inheritance, opaque types, and implementation-defined fields such as `long`, `wchar_t`, and `long double`. The result is a practical build-time verification workflow for teams that want stronger guarantees while still working with native C++ types.

## Key Takeaways

- Learn when native C++ types are genuinely suitable for byte-level transport, and why `trivially_copyable` and `sizeof` checks are not enough.
- See how C++26 reflection can construct a single compile-time layout signature that supports both ABI comparison and transport-safety analysis.
- Leave with a CI-friendly verification workflow: export signatures on target platforms, aggregate generated headers, and fail builds on mismatch.

## Outline

### 1. Problem: native structs cross boundaries more often than we admit

- Why teams reuse native structs at process, machine, plugin, and storage boundaries
- The two recurring failure modes: transport-unsound bytes and cross-platform layout drift
- Why existing checks catch only fragments of the real problem

### 2. Mechanism: compile-time layout signatures from C++26 reflection

- Using reflection to enumerate fields, bases, offsets, and bit-fields
- Flattening native layouts into a compile-time signature with size, alignment, and offset information
- Why one representation is enough to support multiple build-time checks

### 3. Safety derivation: one signature, two answers

- Layout equality as a direct signature comparison
- Transport safety as a property derived from pointer-like and platform-sensitive structure in the signature
- Three concrete examples: fixed-width safe type, pointer-containing type with matching layout, and platform-divergent type

### 4. Cross-platform verification pipeline: from target builds to CI gate

- Export signatures on each target platform
- Aggregate generated headers in a verification build
- Use `static_assert` and reporting to fail CI when layouts diverge or preconditions are violated

### 5. Practical limits and recommendations

- What this method proves, and what it intentionally does not prove
- Handling virtual inheritance, opaque types, and implementation-defined fields
- Guidelines for designing native types that survive cross-platform transport

## Reviewer Notes

- This is not a general reflection overview. It is a concrete C++26 reflection application aimed at a real systems problem: proving build-time ABI and transport properties of native C++ types.
- The talk includes real code, generated artifacts, and cross-platform examples rather than slides that stay at the language-feature level.
- The material is intended for a broad C++ audience: any team that reuses native structs at process, machine, or storage boundaries can apply the workflow, even if they do not work on networking or shared-memory infrastructure full time.
- The demos are backed by a working prototype implementation, but the talk is framed around the method and engineering workflow, not around launching a product or library.
