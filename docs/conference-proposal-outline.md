# Conference Proposal Outline

## Title

**Can I Safely memcpy This Type? Compile-Time Layout Verification with C++26 Reflection**

## Format

Standard session (60 minutes including Q&A)

## Abstract (draft)

memcpy-ing a C++ type across address spaces -- shared memory, network protocols,
persistent storage -- is deceptively dangerous. Pointers dangle, vtable pointers
corrupt, and the same struct compiles to different byte layouts on different
platforms. Existing tools either miss real problems (trivially_copyable accepts
types with pointers) or require an external IDL (Protocol Buffers, FlatBuffers).

This talk presents a compile-time mechanism called Layout Signatures, using C++26
static reflection (P2996): encode each type's byte-level layout as a compile-time
string.
From this one representation, two questions are answered at build time: (1) do the
byte layouts match across platforms? -- just compare signature strings; and (2) is
the type safe for byte-level transport? -- scan the signature for pointer tokens.
The approach works directly on native C++ types -- no IDL, no code generation, no
runtime overhead.

We will walk through how Layout Signatures are constructed using P2996 reflection
primitives, then show how the same signature enables both cross-platform comparison
and safety checking, with live demonstrations on real cross-platform data (x86_64
Linux, ARM64 macOS).

## Outline

### 1. The Problem (8 min)

**1.1 Motivating scenarios**
- Shared memory IPC (Boost.Interprocess, mmap)
- Network protocol headers (zero-copy receive)
- Persistent file formats (memory-mapped DB)
- Common pattern: `memcpy(&header, buffer, sizeof(Header))`

**1.2 Pitfalls A -- byte-copy safety**

Even on the same machine, memcpy across address spaces can silently
produce broken data:

| Pitfall | Why it breaks |
|---------|---------------|
| Raw pointers | Dangle in another address space |
| References | Implemented as pointers, same problem |
| Virtual function tables | Hidden vptr is an absolute address |
| Virtual inheritance | Hidden vbptr, compiler-specific layout |
| Heap-owning types | `std::string`, `std::vector` contain internal pointers |

Key insight: `std::is_trivially_copyable` does NOT catch these --
a struct with a raw pointer is trivially_copyable but NOT safe to transport.

**1.3 Pitfalls B -- cross-platform, layout mismatch**

Same source code, different byte layout:

| Pitfall | Example |
|---------|---------|
| Type size | `long`: LP64 = 8B, LLP64 = 4B |
| Type representation | `long double`: x87 80-bit, ARM64 64-bit, IBM double-double, IEEE binary128 |
| Character width | `wchar_t`: Linux 4B, Windows 2B |
| Endianness | little-endian vs big-endian |
| Padding rules | Compiler-specific alignment and padding insertion |
| Bit-field layout | Storage unit boundaries and ordering are implementation-defined |

**1.4 Why existing tools fall short**

| Tool | Limitation |
|------|-----------|
| `trivially_copyable` | Accepts types with pointers (false positive) |
| `static_assert(sizeof(T) == N)` | Does not inspect internal field layout |
| Protocol Buffers / FlatBuffers | Requires external IDL, cannot use native C++ types directly |
| Manual review | Does not scale, misses platform-dependent differences |

For cross-platform layout comparison, there is currently no automated
C++ native tool at all -- this is exactly the gap this talk fills.

**1.5 The question this talk answers**

> At compile time, can we automatically determine:
> is this type safe to memcpy to another platform?

Two sub-questions: (A) is the byte layout identical on both platforms?
(B) can the bytes be safely transported? We answer both from a single
mechanism -- the Layout Signature.

### 2. Layout Signature -- the Core Mechanism (20 min, includes Demo A)

One mechanism, two answers.

**2.1 Idea**: encode a type's byte-level layout as a compile-time string.
Nested structs and base classes are recursively expanded so the signature
captures every leaf field with its absolute offset.

**2.2 C++26 reflection primitives (P2996)**
- `^^T` -- reflect a type
- `nonstatic_data_members_of` -- enumerate fields
- `offset_of` -- byte offset of each field
- `type_of` -- field type reflection
- `is_bit_field`, `bit_size_of` -- bit-field support

**2.3 Signature format**

```
[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:f64[s:8,a:8]}
  ^          ^           ^               ^
  arch       total       field 1         field 2
  prefix     size/align  at offset 0     at offset 8
```

Key elements encoded in the signature:
- Architecture prefix: bit-width + endianness
- Leaf field types: canonical names (u32, f64, ptr, etc.)
- Field offsets, sizes, and alignments
- Pointer-like tokens (ptr, fnptr, ref, rref, memptr, vptr)

**2.4 Demo A -- signature generation** (5 min)

Live: define a struct, call `get_layout_signature<T>()`, inspect the
output string.  Show three cases:
- Safe struct (fixed-width fields only) -- clean signature
- Struct with pointer -- `ptr[` token appears in signature
- Polymorphic class -- `vptr` token appears in signature

**2.5 Two properties fall out of this design**
- Signature equality = layout match (for cross-platform comparison)
- Pointer tokens in signature = unsafe for transport (for safety checking)

These are not separate concepts -- they are direct consequences of what
the signature encodes. Even hidden pointers (like the vptr in polymorphic
types) are encoded as tokens, so safety checking is entirely derivable
from the signature. Section 3 shows how each property is used.

### 3. Two Applications of the Signature (10 min)

**3.1 Application 1: Cross-platform layout comparison**

Same type, two platforms -- are the byte layouts identical?
Each platform exports its signatures to a header file at build time;
a CI build includes both and compares them.

```cpp
static_assert(local_sig == remote_sig,
    "Layout mismatch: type has different byte layout on remote platform");
```

- Two platforms produce the same signature -> byte layouts are identical
- Different signatures -> pinpoint exactly which types differ
- Design trade-off: answers "are the bytes identical?" not "are the
  semantics identical?"

**3.2 Application 2: Byte-copy safety checking**

Can this type's bytes be safely transported?

```cpp
static_assert(is_byte_copy_safe_v<T>,
    "Type is not safe for byte-level transport");
```

What makes a type NOT byte-copy safe:
- Pointers / references: dangle in another address space
- Polymorphic types: hidden vptr is an absolute address

All of these are pointer-like -- and all are encoded as tokens in the
Layout Signature. Safety checking is just scanning the signature for
pointer tokens (ptr, fnptr, ref, rref, memptr, vptr). No separate
analysis pass needed -- the signature already contains the answer.

**3.3 The answer**

```
byte_copy_safe AND signatures match -> safe to memcpy across platforms
```

Both checks are compile-time:
- byte_copy_safe: `static_assert(is_byte_copy_safe_v<T>)`
- Signature match: `static_assert(local_sig == remote_sig)` in a CI build
  that `#include`s exported `.sig.hpp` from each target platform

### 4. Demo B -- Cross-Platform Comparison (10 min)

**4.1 Workflow: how to use it in practice**
- Phase 1: compile on each target platform, export signatures to `.sig.hpp`
- Phase 2: CI build `#include`s all `.sig.hpp`, `static_assert` on equality
- All checks are compile-time -- no runtime overhead in the deployed binary

**4.2 Live comparison**: same types on 2 platforms
- x86_64 Linux (GCC 16, LP64)
- ARM64 macOS (Clang P2996, LP64)

**4.3 What breaks and why**
- `long double` field: x87 80-bit (16B) vs ARM64 64-bit (8B)
- Struct with only fixed-width types: identical everywhere
- Even same data model (LP64) can differ in representation

**4.4 Practical recommendations**
- Avoid platform-dependent types (`long`, `long double`, `wchar_t`);
  prefer fixed-width alternatives (`int32_t`, `double`)
- For types whose internal layout should not enter the signature (e.g.,
  `offset_ptr`-based containers), use opaque registration macros
- Run signature export as part of CI

### 5. Summary + Q&A (12 min)

**5.1 Recap**

One core mechanism -- Layout Signature -- answers both questions:

| Application | How |
|-------------|-----|
| Cross-platform layout comparison | Signature equality (`sigA == sigB`) |
| Byte-copy safety checking | Pointer token scan in the signature |

Both satisfied -> safe to memcpy across platforms.

**5.2 Key takeaways**
- C++26 reflection enables compile-time layout analysis that was previously impossible
- `trivially_copyable` is not enough -- pointer detection and cross-platform comparison are essential
- One mechanism (Layout Signature), two applications, zero runtime overhead
- Works on native C++ types -- no IDL, no code generation

**5.3 Q&A**

---

## Narrative arc

```
Problem (why should I care?)
  -> memcpy is everywhere, but silently dangerous
  -> existing tools miss real problems

One mechanism (how does it work?)
  -> Layout Signature: encode layout as compile-time string via P2996
  -> Every leaf field captured with absolute offset; pointer tokens included
  -> P2996 reflection provides the primitives

Two applications (what do I get from it?)
  -> Cross-platform comparison: signature equality = layout match
  -> Safety checking: pointer token scan in the signature
  -> The answer: both satisfied = safe to memcpy

Proof (does it actually work?)
  -> Demo A: live signature generation (safe, pointer, polymorphic)
  -> Demo B: live cross-platform comparison on 2 platforms
  -> Practical workflow and recommendations

Takeaway (what do I do next?)
  -> Use fixed-width types, avoid platform-dependent types, run signature CI
```
