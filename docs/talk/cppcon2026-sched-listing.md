# CppCon 2026 Sched Listing Snapshot

This document is a normalized, repository-local snapshot of the public CppCon
2026 Sched listing. It preserves the external promise that the slide deck must
fulfill without depending on the availability or current rendering of Sched.

- Source: <https://cppcon2026.sched.com/event/2RT5c/can-i-memcpy-this-type-across-a-boundary-verifying-object-representation-at-compile-time-with-c++26-reflection>
- Short link: <https://sched.co/2RT5c>
- Retrieved: 2026-08-26
- Event ID: `2RT5c`

## Published Session Metadata

| Field | Published value |
|---|---|
| Conference | CppCon 2026 |
| Title | Can I memcpy This Type Across a Boundary? Verifying Object Representation at Compile Time With C++26 Reflection |
| Date | Tuesday, September 15, 2026 |
| Time | 09:00-10:00 MDT |
| Venue | `_3` |
| Session type | General / Breakout |
| Presenter | Fanchen Su, Tech Lead, NetEase Games |
| Subjects | Future C++; Generic/Metaprogramming; Safety; Software Quality |

## Published Abstract

Native C++ types often become the format for bytes that cross a boundary:
shared-memory IPC, plugin interfaces, persistent storage, or software built
for more than one ABI. At that point the type is no longer just an
implementation detail; it is part of a binary contract. `trivially_copyable`,
`sizeof`, and code review help, but they do not answer the two questions that
matter: may this type be transported as bytes at all, and do all supported ABIs give it the same object representation?

C++26 reflection can derive that evidence from the type itself. The technique
builds a compile-time layout signature from ordinary C++ types, without IDL,
generated stubs, or runtime inspection. The signature records the
representation facts needed for byte transfer: architecture and endianness,
leaf type tokens, sizes, alignments, absolute offsets, bit-fields, and
pointer-like markers. It deliberately leaves out field names and source-level
meaning.

With those signatures in hand, the build can enforce a gate for memcpy-style
transfer. You name the boundary types and supported ABIs. Each target exports
signatures, and a verification build permits direct byte transfer only when
the type is byte-copy safe and the signature matches across the set. We'll
walk through the workflow with `static_assert` checks and CI diagnostics: a
fixed-width type that passes, a pointer-containing type rejected by admission,
and a platform-divergent type rejected by signature comparison. The claim is
intentionally narrow: representation compatibility, not semantic compatibility or schema evolution.

## External Commitments for the Deck

The public listing commits the talk to the following points:

1. The motivating boundaries include shared-memory IPC, plugin interfaces,
   persistent storage, and multiple ABIs.
2. The decision has two independent gates: byte-transport admission and
   cross-ABI object-representation agreement.
3. The evidence is derived from ordinary C++ types with C++26 reflection and
   encoded as compile-time layout signatures.
4. The workflow names a finite set of boundary types and supported ABIs, has
   each target export evidence, and verifies the set in a build/CI gate.
5. The walkthrough includes one passing fixed-width type, one admission
   failure caused by pointer-like state, and one signature mismatch caused by
   platform divergence.
6. The claim stops at representation compatibility. It does not promise
   semantic compatibility or schema evolution.
