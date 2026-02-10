# §1 Introduction

The C++ memory model grants programmers direct control over data layout—a
power that enables zero-copy inter-process communication, memory-mapped file
I/O, and hardware register access. This power, however, comes with an implicit
contract: every module that touches a shared data structure must agree on its
exact byte-level layout. When that contract is violated—silently, at link time
or at runtime—the consequences range from corrupted data to security
vulnerabilities.

## 1.1 The Problem: Manual Layout Verification

Today, the standard practice for verifying type layout compatibility in C++ is
a battery of hand-written `static_assert` checks:

```cpp
struct PacketHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t length;
    uint64_t timestamp;
};

// Manual verification: one assert per property
static_assert(sizeof(PacketHeader) == 24);
static_assert(alignof(PacketHeader) == 8);
static_assert(offsetof(PacketHeader, magic)     == 0);
static_assert(offsetof(PacketHeader, version)   == 4);
static_assert(offsetof(PacketHeader, flags)     == 8);
static_assert(offsetof(PacketHeader, length)    == 12);
static_assert(offsetof(PacketHeader, timestamp) == 16);
```

This approach has four fundamental limitations:

1. **Incompleteness.** Checking `sizeof` alone does not guarantee field offsets
   are correct. A struct may have the expected total size yet contain different
   internal padding due to reordered or resized fields.

2. **Maintenance cost is O(n).** Every field addition, removal, or type change
   requires updating every corresponding assert. In practice, these asserts
   fall out of date—silently, because the struct "still compiles."

3. **Cannot compare two types.** The checks above verify a single type against
   hard-coded constants. They cannot express the question: "Is *this* struct
   layout-compatible with *that* struct?"—the fundamental question in IPC,
   plugin systems, and cross-module communication.

4. **No cross-platform story.** The same source code may produce different
   layouts on different platforms (e.g., `long` is 4 bytes on Windows LLP64
   but 8 bytes on Linux LP64). Manual asserts are platform-local; they cannot
   detect cross-platform divergence.

These limitations lead to real-world failures. In shared-memory IPC, a
reader and writer compiled with different struct definitions silently corrupt
data. In plugin systems, a host application and a dynamically loaded plugin
disagree on a callback struct's layout, causing memory stomps. In binary file
formats, a file written on one platform becomes unreadable on another because
of padding differences.

## 1.2 Our Approach: Compile-Time Type Layout Signatures

We present **TypeLayout**, a header-only C++ library that uses C++26 static
reflection (P2996) to generate *complete, deterministic, human-readable*
type layout signatures at compile time—with zero runtime overhead.

With TypeLayout, the 7-line manual verification above reduces to:

```cpp
static_assert(layout_signatures_match<SenderPacket, ReceiverPacket>());
```

This single line provides a compiler-verified proof that the two types have
identical byte layouts: same field types, same sizes, same alignments, same
offsets, same padding—automatically derived from the type definitions
themselves via compile-time reflection.

TypeLayout introduces a *two-layer signature system* that distinguishes
between two notions of type compatibility:

- **Layout signatures** capture *byte identity*: the flattened sequence of
  field offsets, sizes, and alignments, with inheritance hierarchies fully
  expanded and field names stripped. Two types with matching Layout signatures
  are `memcpy`-compatible.

- **Definition signatures** capture *structural identity*: the full type
  structure including field names, inheritance trees, enum qualified names,
  and polymorphism markers. Two types with matching Definition signatures are
  structurally equivalent—same bytes *and* same meaning.

The two layers are related by a *projection*: every Definition match implies
a Layout match, but not vice versa. This allows users to choose the
appropriate level of strictness for their use case.

## 1.3 Motivating Example

Consider two teams independently developing IPC endpoints:

```cpp
// Team A: sender module
namespace sender {
    struct Message {
        uint32_t id;
        uint64_t timestamp;
        double   payload;
    };
}

// Team B: receiver module
namespace receiver {
    struct Message {
        uint32_t id;
        uint64_t timestamp;
        double   payload;
    };
}
```

With TypeLayout, both byte-level and structural compatibility can be verified
at compile time:

```cpp
// Byte-level: can we memcpy safely?
static_assert(layout_signatures_match<
    sender::Message, receiver::Message>());

// Structural: are they truly the same structure?
static_assert(definition_signatures_match<
    sender::Message, receiver::Message>());
```

If Team B renames `payload` to `value`, the Layout check still passes (same
bytes), but the Definition check catches the semantic drift—preventing a
class of bugs that no amount of `sizeof`/`offsetof` checking can detect.

## 1.4 Contributions

This paper makes the following contributions:

1. **A two-layer type layout signature system** that distinguishes byte
   identity (Layout) from structural identity (Definition), built on C++26
   static reflection (P2996). The system is fully automatic, requiring no
   annotations or code generation. (§3)

2. **A formal semantics** grounded in denotational semantics and refinement
   theory, with proofs of three key properties: (§4)
   - *Soundness*: signature match implies `memcpy`-compatibility (zero false
     positives)
   - *Encoding Faithfulness (Injectivity)*: distinct layouts produce distinct
     signatures
   - *Strict Refinement*: Definition signatures are a strict refinement of
     Layout signatures (`definition_match ⟹ layout_match`)

3. **A cross-platform verification toolchain** using a two-phase architecture:
   Phase 1 exports signatures on platforms with P2996 support; Phase 2
   compares signatures on any C++17 compiler. This decouples the reflection
   requirement from the verification requirement. (§5)

4. **An empirical evaluation** demonstrating coverage of all C++ type
   categories (primitives, records, unions, inheritance, bit-fields,
   polymorphic types), compile-time overhead characterization, and
   systematic comparison with existing approaches (`sizeof`/`offsetof`
   asserts, ABI Compliance Checker, Boost.PFR). (§6)

## 1.5 Paper Outline

The remainder of this paper is organized as follows. §2 provides background
on the C++ memory layout model and P2996 static reflection. §3 presents the
two-layer signature system design and generation algorithm. §4 develops the
formal semantics and proves the core theorems. §5 describes the cross-platform
toolchain. §6 presents the evaluation. §7 discusses related work. §8
discusses limitations and future work, and §9 concludes.
