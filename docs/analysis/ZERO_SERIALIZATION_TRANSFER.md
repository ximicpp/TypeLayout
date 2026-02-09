# Zero-Serialization Transfer: Necessary and Sufficient Conditions

> A systematic analysis of when C++ types can be transmitted as raw bytes
> across platforms without serialization/deserialization, grounded in
> Boost.TypeLayout's two-layer signature system and formal verification theory.

## Table of Contents

1. [Formal Definition and Necessary-Sufficient Conditions](#1-formal-definition)
2. [C++ Type Taxonomy for Zero-Serialization Eligibility](#2-type-taxonomy)
3. [Complete Proof Chain](#3-proof-chain)
4. [Network Transmission Deep Dive](#4-network-deep-dive)
5. [The Complete Safety Chain: From Layout Match to send/recv](#5-safety-chain)
6. [Type Remediation Guide: Unsafe → Safe](#6-remediation)
7. [Decision Tree: Zero-Copy vs Serialization Frameworks](#7-decision-tree)
8. [Practical Examples and Code Patterns](#8-examples)

---

## 1. Formal Definition and Necessary-Sufficient Conditions {#1-formal-definition}

### 1.1 The Zero-Serialization Transfer Predicate

**Definition** (Zero-Serialization Transfer, ZST).  
Given a type \( T \), a source platform \( P_s \), and a destination platform \( P_d \):

\[
\boxed{
\text{ZST}(T, P_s, P_d) \stackrel{\text{def}}{\iff}
\forall v \in \text{Values}(T) :
\text{interpret}_{P_d}(\text{bytes}_{P_s}(v)) = v
}
\]

In words: **every value of type T, when read as raw bytes on the source platform, can
be directly interpreted on the destination platform to recover the identical value.**

This is the mathematical definition of "no serialization needed."

### 1.2 Necessary and Sufficient Conditions

The five underlying physical conditions for ZST can be **reduced to two
machine-verified conditions plus one background axiom**, because the layout
signature already encodes the architecture prefix (endianness + pointer width).

#### Reduction Proof

| Original | Reduced To | Reason |
|----------|-----------|--------|
| N1 (Layout Match) | **C1** | Directly checked |
| N2 (Same Endianness) | **⊂ C1** | Architecture prefix `[64-le]` is part of the signature string; `sig_L(T,Ps)==sig_L(T,Pd)` ⟹ identical prefix ⟹ same endianness |
| N3 (IEEE 754) | **A1** | Platform axiom, not encoded in signatures; holds on all modern hardware |
| N4 (No Pointer Fields) | **⊂ C2** | `classify_safety` returns `Safe` only when no `ptr[`, `fnptr[`, `vptr` patterns found |
| N5 (No Bit-Fields) | **⊂ C2** | `classify_safety` returns `Safe` only when no `bits<`, `wchar[` patterns found |

#### The Simplified ZST Theorem

\[
\boxed{
\text{ZST}(T, P_s, P_d) \iff
\underbrace{\text{sig}_L(T, P_s) = \text{sig}_L(T, P_d)}_{\textbf{C1: Layout Signature Match}}
\;\land\;
\underbrace{\text{classify\_safety}(\text{sig}) = \text{Safe}}_{\textbf{C2: Safety Classification}}
}
\]

under the **background axiom A1**: both platforms use IEEE 754 floating-point
(true on all modern x86, ARM, RISC-V hardware).

| # | Condition | What It Covers | How Verified |
|---|-----------|---------------|--------------|
| **C1** | Layout Signature Match | `sizeof`, `alignof`, all field offsets, endianness, pointer width | `sig_layout(T, Ps) == sig_layout(T, Pd)` (string equality) |
| **C2** | Safety = Safe | No pointer fields, no bit-fields, no platform-dependent types | `classify_safety(sig) == SafetyLevel::Safe` (pattern scan) |
| **A1** | IEEE 754 (axiom) | Floating-point bit patterns have same interpretation | Documented assumption |

### 1.3 Why C1 and C2 Are Irreducible

**C1 and C2 are genuinely independent** — neither implies the other:

| Case | C1 | C2 | Example |
|------|----|----|---------|
| C1 ∧ C2 | ✅ | ✅ | `struct { uint32_t x; double y; }` — Safe + matching layouts → **ZST** ✓ |
| C1 ∧ ¬C2 | ✅ | ❌ | `struct { int32_t x; int* p; }` — Layouts match on same-arch, but pointer values not portable |
| ¬C1 ∧ C2 | ❌ | ✅ | `struct { uint32_t x; }` with `alignas(16)` on one platform but `alignas(4)` on another |
| ¬C1 ∧ ¬C2 | ❌ | ❌ | `struct { long x; unsigned flags:3; }` — Both layout mismatch and unsafe |

**Therefore C1 ∧ C2 is the minimal, irreducible form.**

### 1.4 Counterexamples for Necessity

Violating either condition leads to concrete failures:

| Violated | Counterexample |
|----------|---------------|
| ¬C1 (layout mismatch) | `long x;` — 8 bytes on Linux, 4 bytes on Windows. Receiver reads wrong field boundaries. |
| ¬C1 (endianness, caught via prefix) | `uint32_t x = 0x01020304;` — bytes differ between big-endian and little-endian. Signatures differ at `[64-le]` vs `[64-be]`. |
| ¬C2 (pointer field) | `int* p;` — pointer value `0x7fff5678` is meaningless in a different address space. |
| ¬C2 (bit-field) | `unsigned flags:3;` — bit 0 could be MSB or LSB depending on compiler. |
| ¬A1 (non-IEEE 754) | `float x;` on IBM hex float — bit pattern has different meaning. |

### 1.5 How TypeLayout Implements C1 ∧ C2

```
┌─────────────────────────────────────────────────────────────┐
│          TypeLayout Verification Pipeline (Simplified)       │
├──────────┬──────────────────────────────────────────────────┤
│          │                                                  │
│  C1      │  sig_layout(T, Ps) == sig_layout(T, Pd)         │
│  (机器)   │  Covers: layout + endianness + pointer width     │
│          │                                                  │
│  C2      │  classify_safety(sig) == SafetyLevel::Safe       │
│  (机器)   │  Covers: no pointers + no bit-fields + no wchar  │
│          │                                                  │
│  A1      │  IEEE 754 (background axiom)                     │
│  (公理)   │  Holds on all modern hardware                    │
│          │                                                  │
├──────────┼──────────────────────────────────────────────────┤
│ Verdict  │  C1 ∧ C2 → "Serialization-free"                 │
│          │  Otherwise → serialization required              │
└──────────┴──────────────────────────────────────────────────┘
```

This maps directly to `compat_check.hpp` lines 278–279:

```cpp
if (r.layout_match && r.safety == SafetyLevel::Safe)
    verdict = "Serialization-free";  // C1 ∧ C2
```

**Both conditions are machine-verified. No human judgment required.**

---

## 2. C++ Type Taxonomy for Zero-Serialization Eligibility {#2-type-taxonomy}

### 2.1 Three-Tier Classification

We classify the entire C++ type space into three categories based on their
**inherent** zero-serialization eligibility:

```
┌─────────────────────────────────────────────────────────────────────┐
│                    C++ Type Space                                    │
│                                                                     │
│  ┌──────────────────────────────────────────────┐                   │
│  │          🟢 SAFE                              │                   │
│  │  Fixed-width integers, IEEE 754 floats,       │                   │
│  │  byte arrays, fixed-underlying enums           │                   │
│  │  → ZST on ALL conforming platform pairs       │                   │
│  │                                                │                   │
│  │  ┌──────────────────────────────────────────┐ │                   │
│  │  │  uint8_t, int16_t, uint32_t, int64_t     │ │                   │
│  │  │  float, double                           │ │                   │
│  │  │  char[N], std::byte[N]                   │ │                   │
│  │  │  enum class E : uint8_t { ... }          │ │                   │
│  │  └──────────────────────────────────────────┘ │                   │
│  └──────────────────────────────────────────────┘                   │
│                                                                     │
│  ┌──────────────────────────────────────────────┐                   │
│  │          🟡 CONDITIONAL                       │                   │
│  │  Platform-dependent sizes, pointers, vtables  │                   │
│  │  → ZST on SOME platform pairs, not all        │                   │
│  │                                                │                   │
│  │  ┌──────────────────────────────────────────┐ │                   │
│  │  │  long, unsigned long                     │ │                   │
│  │  │  wchar_t                                 │ │                   │
│  │  │  long double                             │ │                   │
│  │  │  T*, void*, function pointers            │ │                   │
│  │  │  polymorphic types (vptr)                │ │                   │
│  │  │  size_t, ptrdiff_t, intptr_t             │ │                   │
│  │  └──────────────────────────────────────────┘ │                   │
│  └──────────────────────────────────────────────┘                   │
│                                                                     │
│  ┌──────────────────────────────────────────────┐                   │
│  │          🔴 UNSAFE                            │                   │
│  │  Implementation-defined layout, non-portable  │                   │
│  │  → ZST unreliable even if signatures match    │                   │
│  │                                                │                   │
│  │  ┌──────────────────────────────────────────┐ │                   │
│  │  │  Bit-fields (unsigned x : 3)             │ │                   │
│  │  │  Types with compiler-specific padding     │ │                   │
│  │  │  #pragma pack / __attribute__((packed))   │ │                   │
│  │  └──────────────────────────────────────────┘ │                   │
│  └──────────────────────────────────────────────┘                   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 Formal Definition of Each Category

**Safe** (零序列化安全):
\[
T \in \text{Safe} \iff \text{constituents}(T) \subseteq \{\text{fixed-int}, \text{ieee754-float}, \text{byte-array}, \text{fixed-enum}\}
\]

All constituent fields are types whose layout is **fully determined by the C++ standard
plus IEEE 754**, independent of compiler or OS.

**Conditional** (条件性零序列化):
\[
T \in \text{Conditional} \iff T \notin \text{Safe} \land \exists (P_s, P_d) : \text{ZST}(T, P_s, P_d)
\]

The type contains platform-dependent elements, but on specific platform pairs
(e.g., two Linux x86_64 systems with the same compiler), ZST may still hold.

**Unsafe** (需要序列化):
\[
T \in \text{Unsafe} \iff \text{constituents}(T) \cap \{\text{bit-field}\} \neq \emptyset
\]

Contains elements whose bit-level layout is **implementation-defined** by
C++ [class.bit] §12.2.4. Even identical layout signatures don't guarantee
identical bit-level semantics across different compilers.

### 2.3 Classification ↔ Safety Level Mapping

| Taxonomy | `classify_safety()` | Signature Pattern | ZST Guarantee |
|----------|--------------------|--------------------|---------------|
| Safe | `SafetyLevel::Safe` | No `bits<`, `ptr[`, `vptr`, `wchar[` | Universal |
| Conditional (pointer) | `SafetyLevel::Warning` | `ptr[`, `fnptr[`, `,vptr` | Layout-only (values not portable) |
| Conditional (platform-dep) | `SafetyLevel::Risk` | `wchar[` | Platform-pair-specific |
| Unsafe (bit-field) | `SafetyLevel::Risk` | `bits<` | Unreliable |

### 2.4 Comprehensive Type Reference Table

| C++ Type | Category | Signature Element | Reason |
|----------|----------|-------------------|--------|
| `uint8_t` | 🟢 Safe | `u8` | Fixed 1 byte, all platforms |
| `int16_t` | 🟢 Safe | `i16` | Fixed 2 bytes, all platforms |
| `uint32_t` | 🟢 Safe | `u32` | Fixed 4 bytes, all platforms |
| `int64_t` | 🟢 Safe | `i64` | Fixed 8 bytes, all platforms |
| `float` | 🟢 Safe | `f32` | IEEE 754 binary32, 4 bytes |
| `double` | 🟢 Safe | `f64` | IEEE 754 binary64, 8 bytes |
| `char[N]` | 🟢 Safe | `bytes[s:N,a:1]` | Byte array, trivial |
| `std::byte[N]` | 🟢 Safe | `bytes[s:N,a:1]` | Byte array, trivial |
| `bool` | 🟢 Safe | `bool` | 1 byte on all modern platforms |
| `enum class E : uint32_t` | 🟢 Safe | `enum[s:4,a:4]<u32>` | Fixed underlying type |
| `int` | 🟢 Safe* | `i32` | *32-bit on all modern platforms (de facto) |
| `long` | 🟡 Conditional | `i64` or `i32` | LP64: 8B, LLP64: 4B |
| `unsigned long` | 🟡 Conditional | `u64` or `u32` | LP64: 8B, LLP64: 4B |
| `wchar_t` | 🟡 Conditional | `wchar[s:4]` or `wchar[s:2]` | Linux/macOS: 4B, Windows: 2B |
| `long double` | 🟡 Conditional | `f80` or `f128` or `f64` | x86: 80-bit, ARM: 64-bit |
| `size_t` | 🟡 Conditional | `u64` or `u32` | Depends on pointer width |
| `T*` | 🟡 Conditional | `ptr[s:8,a:8]` | Values not portable across address spaces |
| `void*` | 🟡 Conditional | `ptr[s:8,a:8]` | Values not portable |
| virtual class | 🟡 Conditional | `...,vptr` | Vtable ABI differs (Itanium vs MSVC) |
| `unsigned x : 3` | 🔴 Unsafe | `bits<u32,3,...>` | Bit ordering implementation-defined |
| `#pragma pack(1)` | 🔴 Unsafe | varies | Compiler-specific padding |

> \* `int` is 32-bit on all platforms in Boost.TypeLayout's target space (ILP32, LP64, LLP64).
> However, it is not a *fixed-width* type by standard. For maximum portability, prefer `int32_t`.

---

## 3. Complete Proof Chain {#3-proof-chain}

### 3.1 Theorem: ZST Correctness

We prove that TypeLayout's verification pipeline correctly decides ZST:

\[
\text{TypeLayout reports "Serialization-free"} \iff \text{ZST}(T, P_s, P_d)
\]

### 3.2 Forward Direction (Soundness): Report → ZST

**Given**: TypeLayout reports "Serialization-free" for type T across platforms Ps, Pd.

**Step 1**: "Serialization-free" requires `layout_match == true` AND `safety == Safe`
(see `compat_check.hpp` line 278–279). This is exactly **C1 ∧ C2**.

**Step 2 (C1)**: `layout_match == true` means `sig_layout(T, Ps) == sig_layout(T, Pd)`.
By **V1 (Layout Match Theorem)** + **Encoding Faithfulness**:
\[
\text{sig}_L(T, P_s) = \text{sig}_L(T, P_d) \implies L_{P_s}(T) = L_{P_d}(T)
\]
This establishes layout identity (sizes, offsets, alignment).
Since the architecture prefix `[64-le]` is part of the signature, C1 also
guarantees same endianness and pointer width.

**Step 3 (C2)**: `safety == Safe` means `classify_safety(sig)` returned `Safe`.
By the classification algorithm:
- No `bits<` pattern ⟹ no bit-fields (deterministic bit-level layout)
- No `ptr[`, `fnptr[`, `vptr` patterns ⟹ no pointer fields (no address-space-dependent values)
- No `wchar[` pattern ⟹ no platform-dependent types

**Step 4 (A1)**: IEEE 754 is a documented assumption, valid on all modern platforms.

**Conclusion**: C1 ∧ C2 (under A1) ⟹ ZST(T, Ps, Pd). ∎

### 3.3 Backward Direction (Completeness): ZST → Report

**Given**: ZST(T, Ps, Pd) holds (the type IS safe for zero-serialization transfer).

**Claim**: TypeLayout will report "Serialization-free" or at worst "Layout OK".

**Argument**: If ZST holds, then necessarily:
- L_{Ps}(T) = L_{Pd}(T) ⟹ sig_L(T, Ps) = sig_L(T, Pd) (by encoding faithfulness injectivity)
- Therefore `layout_match == true`

The safety classification may conservatively report Warning/Risk even when the
type is actually safe for a specific platform pair. This is intentional:

\[
\text{Report} = \text{"Serialization-free"} \implies \text{ZST} \quad \text{(no false positives)}
\]
\[
\text{ZST} \centernot\implies \text{Report} = \text{"Serialization-free"} \quad \text{(possible false negatives)}
\]

**This is the correct direction for a safety tool**: it may be overly cautious,
but will never tell you something is safe when it isn't.

### 3.4 Proof Architecture Summary

```
        C1: Layout Signature Match              C2: Safety Classification
        ┌─────────────────────────┐             ┌────────────────────────┐
        │ sig_L(T,Ps)==sig_L(T,Pd)│             │ classify_safety==Safe  │
        │                         │             │                        │
        │ V1 + Encoding Faithful. │             │ Pattern scan:          │
        │ ⟹ L_Ps(T) = L_Pd(T)   │             │ no ptr[, bits<, vptr,  │
        │                         │             │ wchar[, fnptr[         │
        │ Covers:                 │             │                        │
        │  • sizeof, alignof      │             │ Covers:                │
        │  • all field offsets    │             │  • no pointer values   │
        │  • endianness (prefix)  │             │  • no impl-def bits   │
        │  • pointer width        │             │  • no platform-dep     │
        └───────────┬─────────────┘             └───────────┬────────────┘
                    │                                       │
                    └───────────────┬───────────────────────┘
                                    │
                              C1 ∧ C2 (under A1: IEEE 754)
                                    │
                                    ▼
                             ZST(T, Ps, Pd) ✓
```

---

## 4. Network Transmission Deep Dive {#4-network-deep-dive}

### 4.1 The Network Protocol Stack

When transmitting a struct over a network, the data passes through multiple layers:

```
Application Layer:  struct PacketHeader header = {...};
                           │
                    ┌──────▼──────┐
Socket Layer:       │ send(&header,│    sizeof(PacketHeader))
                    │   sizeof)   │
                    └──────┬──────┘
                           │ raw bytes
                    ┌──────▼──────┐
Transport (TCP):    │ byte stream  │    may fragment across packets
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
Network (IP):       │ IP packets   │    routed across networks
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
Receiver Socket:    │ recv(buf,   │
                    │   sizeof)   │
                    └──────┬──────┘
                           │
Application Layer:  memcpy(&header, buf, sizeof(PacketHeader));
```

**Zero-serialization means**: the `send()` and `recv()` operate directly on
the struct's memory representation, with no encode/decode step.

### 4.2 What Can Go Wrong Without Verification

| Problem | Example | Consequence |
|---------|---------|-------------|
| **Field size mismatch** | `long` is 4B on sender, 8B on receiver | All subsequent fields read at wrong offsets |
| **Alignment padding** | Compiler inserts 4B padding after `int` before `double` | Receiver expects data at different positions |
| **Endianness** | Sender: `0x01020304` → `04 03 02 01`; Receiver expects: `01 02 03 04` | Values corrupted |
| **Struct size mismatch** | `sizeof` differs → `recv` reads wrong number of bytes | Buffer overflow or truncation |
| **Pointer values** | Address `0x7fff5678` sent over network | Meaningless on receiver; potential crash |
| **Bit-field order** | Sender packs MSB-first, receiver expects LSB-first | Flag bits read incorrectly |

### 4.3 How TypeLayout Eliminates Each Problem

| Problem | TypeLayout Mechanism | Condition |
|---------|---------------------|-----------|
| Field size mismatch | Layout signature encodes every field's type and offset | **C1** (signature match) |
| Alignment padding | Layout signature includes `sizeof` and all offsets | **C1** (signature match) |
| Endianness | Architecture prefix `[64-le]` / `[32-be]` encoded in signature | **C1** (prefix is part of signature) |
| Struct size mismatch | `sizeof` encoded in signature `[s:SIZE,a:ALIGN]` | **C1** (signature match) |
| Pointer values | `classify_safety` detects `ptr[`, `fnptr[` | **C2** (Safety ≠ Safe) |
| Bit-field order | `classify_safety` detects `bits<` | **C2** (Safety ≠ Safe) |

### 4.4 Wire-Format Alignment

A common question: "Does my struct's in-memory layout match the wire format I need?"

TypeLayout answers this by checking whether two sides agree on the layout,
but it does **not** mandate a specific wire format. The approach:

```
Approach A: Struct IS the wire format (zero-serialization)
┌──────────────────────────────────────────┐
│ If sig_layout matches across all parties │
│ AND safety == Safe                       │
│ → struct memory IS the wire format       │
│ → send(&struct, sizeof) directly         │
└──────────────────────────────────────────┘

Approach B: Struct is NOT the wire format (serialization needed)
┌──────────────────────────────────────────┐
│ If sig_layout DIFFERS or safety ≠ Safe   │
│ → must serialize to a canonical format   │
│ → use protobuf / FlatBuffers / custom    │
└──────────────────────────────────────────┘
```

### 4.5 TCP Stream Boundaries

TCP is a **byte stream**, not a message protocol. A single `send()` of
`sizeof(PacketHeader)` bytes may arrive in multiple `recv()` calls.

This is **orthogonal** to zero-serialization. TypeLayout guarantees that the
*byte content* is correct; the application must still handle:

```cpp
// Correct: accumulate until full struct received
size_t received = 0;
while (received < sizeof(PacketHeader)) {
    ssize_t n = recv(sock, buf + received, sizeof(PacketHeader) - received, 0);
    if (n <= 0) handle_error();
    received += n;
}
// Now buf contains exactly sizeof(PacketHeader) bytes
// TypeLayout guarantees: memcpy(&header, buf, sizeof(PacketHeader)) is safe
```

### 4.6 Multi-Message Protocols

Real protocols often have multiple message types. TypeLayout handles this
naturally — verify each type independently:

```cpp
// Verify all protocol message types at compile time
static_assert(compat::sig_match(
    plat_a::PacketHeader_layout, plat_b::PacketHeader_layout));
static_assert(compat::sig_match(
    plat_a::DataRecord_layout, plat_b::DataRecord_layout));
static_assert(compat::sig_match(
    plat_a::AckMessage_layout, plat_b::AckMessage_layout));

// If all pass → entire protocol is zero-serialization safe
```

For collection-level verification, use `CompatReporter` to check all types at once.

---

## 5. The Complete Safety Chain: Layout Match → send/recv {#5-safety-chain}

### 5.1 The Six-Step Safety Chain

For a type to safely travel from `send()` on machine A to `recv()` on machine B:

```
Step 1: Signature Generation          (Phase 1, on each platform)
  sig_layout(T, A) = encode(L_A(T))
  sig_layout(T, B) = encode(L_B(T))
                │
Step 2: Signature Comparison          (Phase 2, on any platform)
  sig_layout(T, A) == sig_layout(T, B)?
  arch_prefix(A) == arch_prefix(B)?
                │
Step 3: Safety Classification         (Phase 2)
  classify_safety(sig) == Safe?
  No pointer fields? No bit-fields?
                │
Step 4: Structural Guarantee          (V1 Theorem application)
  sizeof(T)@A == sizeof(T)@B
  offsetof(T, field_i)@A == offsetof(T, field_i)@B  ∀i
                │
Step 5: Value Guarantee               (IEEE 754 + fixed-width)
  interpret_B(bytes_A(v)) == v  ∀v ∈ Values(T)
                │
Step 6: Network Transfer              (Application level)
  send(sock, &value, sizeof(T));    // Machine A
  recv(sock, &buf, sizeof(T));      // Machine B
  memcpy(&value, buf, sizeof(T));   // Correct value recovered ✓
```

### 5.2 Each Step's Contribution

| Step | Establishes | Without This Step |
|------|-------------|-------------------|
| 1 | Ground truth from each platform's compiler | No data to compare |
| 2 | **C1** (layout + endianness + pointer width match) | Wrong offsets/sizes, byte-order corruption |
| 3 | **C2** (no pointers, no bit-fields, no platform-dep) | Pointer values crash, bit-field misinterpretation |
| 4 | Physical layout identity | Mathematical guarantee of byte compatibility |
| 5 | Semantic value preservation (**A1**: IEEE 754) | Bits correct but meaning differs |
| 6 | Actual data transfer | Everything verified but nothing transmitted |

### 5.3 Failure Modes and Diagnostics

| Step | Failure | TypeLayout Diagnostic |
|------|---------|----------------------|
| 2 | Layout DIFFER | Report shows differing signatures per platform |
| 2 | Arch prefix DIFFER | Report shows different `[64-le]` vs `[32-le]` |
| 3 | Safety = Warning | "Layout OK (pointer values not portable)" |
| 3 | Safety = Risk | "Layout OK (verify bit-fields manually)" |
| 2+3 | Layout DIFFER + Risk | "Needs serialization" |

---

## 6. Type Remediation Guide: Unsafe → Safe {#6-remediation}

### 6.1 Remediation Strategy Overview

```
Goal: Transform types from Conditional/Unsafe → Safe
      so they qualify for zero-serialization on ALL platform pairs.

Strategy: Replace every platform-dependent element with
          a fixed-width, standard-defined equivalent.
```

### 6.2 Specific Remediation Patterns

#### Pattern 1: Replace `long` with Fixed-Width Integer

```cpp
// ❌ BEFORE: Conditional (LP64: 8B, LLP64: 4B)
struct Record {
    long value;
    unsigned long count;
};

// ✅ AFTER: Safe (fixed 8 bytes everywhere)
struct Record {
    int64_t  value;
    uint64_t count;
};
```

**Signature change**: `i64`/`i32` (ambiguous) → `i64` (deterministic)

#### Pattern 2: Replace `wchar_t` with `char32_t`

```cpp
// ❌ BEFORE: Conditional (Linux: 4B, Windows: 2B)
struct TextRecord {
    wchar_t text[64];
};

// ✅ AFTER: Safe (fixed 4 bytes per character)
struct TextRecord {
    char32_t text[64];
};
```

#### Pattern 3: Replace `long double` with `double`

```cpp
// ❌ BEFORE: Conditional (x86: 80-bit, ARM: 64-bit)
struct Measurement {
    long double precise_value;
};

// ✅ AFTER: Safe (IEEE 754 binary64 everywhere)
struct Measurement {
    double precise_value;  // 53-bit mantissa sufficient for most uses
};

// Alternative: Use fixed-point representation for extreme precision
struct Measurement {
    int64_t value_fixed;   // value × 10^scale
    int8_t  scale;         // decimal scale factor
};
```

#### Pattern 4: Eliminate Bit-Fields

```cpp
// ❌ BEFORE: Unsafe (bit ordering implementation-defined)
struct Flags {
    unsigned int read    : 1;
    unsigned int write   : 1;
    unsigned int execute : 1;
    unsigned int mode    : 5;
};

// ✅ AFTER: Safe (explicit bit manipulation)
struct Flags {
    uint8_t packed;  // bits: [0]=read, [1]=write, [2]=execute, [3:7]=mode

    bool read()    const { return packed & 0x01; }
    bool write()   const { return packed & 0x02; }
    bool execute() const { return packed & 0x04; }
    uint8_t mode() const { return (packed >> 3) & 0x1F; }

    void set_read(bool v)    { packed = (packed & ~0x01) | (v ? 0x01 : 0); }
    void set_write(bool v)   { packed = (packed & ~0x02) | (v ? 0x02 : 0); }
    void set_execute(bool v) { packed = (packed & ~0x04) | (v ? 0x04 : 0); }
    void set_mode(uint8_t m) { packed = (packed & ~0xF8) | ((m & 0x1F) << 3); }
};
```

**Signature change**: `bits<u32,1,1,1,5>` → `u8` (deterministic)

#### Pattern 5: Remove Pointers from Serialized Data

```cpp
// ❌ BEFORE: Conditional (pointer values not portable)
struct Node {
    int32_t value;
    Node*   next;
};

// ✅ AFTER: Safe (index-based references)
struct Node {
    int32_t value;
    int32_t next_index;  // -1 = null, ≥0 = index into node array
};
```

#### Pattern 6: Separate Data from Polymorphism

```cpp
// ❌ BEFORE: Conditional (vtable layout is ABI-specific)
struct SensorRecord {
    int32_t id;
    double  value;
    virtual ~SensorRecord() = default;
};

// ✅ AFTER: Safe data + separate polymorphism
struct SensorData {        // Wire format — no vtable
    int32_t id;
    double  value;
};

class SensorRecord : public SensorData {  // Application type — has vtable
    virtual ~SensorRecord() = default;
    // ... polymorphic behavior
};

// Network sends SensorData, not SensorRecord
```

### 6.3 Remediation Verification Workflow

```
1. Identify Conditional/Unsafe types
   → Run CompatReporter, look for Warning/Risk safety levels

2. Apply remediation patterns
   → Replace platform-dependent types with fixed-width equivalents

3. Regenerate signatures on all target platforms
   → Phase 1: SigExporter on each platform

4. Re-verify
   → Phase 2: CompatReporter should now show all MATCH + Safe

5. Confirm
   → "Serialization-free" verdict for all types
```

---

## 7. Decision Tree: Zero-Copy vs Serialization Frameworks {#7-decision-tree}

### 7.1 The Master Decision Tree

```
                    ┌─────────────────────────┐
                    │ Do you transfer data     │
                    │ between platforms/hosts?  │
                    └────────┬────────────────┘
                             │
                    ┌────────▼────────┐
                    │  Same binary?    │
                    │  (same OS, same  │
                    │   compiler, same │
                    │   flags)         │
                    └──┬──────────┬───┘
                    Yes│          │No
                       ▼          ▼
              ┌────────────┐  ┌──────────────────┐
              │ memcpy OK  │  │ Run TypeLayout    │
              │ (trivially │  │ CompatReporter    │
              │  safe)     │  └──────┬───────────┘
              └────────────┘         │
                              ┌──────▼──────────┐
                              │ All types:       │
                              │ Layout MATCH +   │
                              │ Safety = Safe?   │
                              └──┬──────────┬───┘
                              Yes│          │No
                                 ▼          ▼
                     ┌───────────────┐  ┌───────────────────────┐
                     │ Zero-copy     │  │ Which types DIFFER     │
                     │ send/recv OK! │  │ or Safety ≠ Safe?      │
                     │               │  └──┬────────────────┬───┘
                     │ No serializa- │     │                │
                     │ tion needed   │  Can fix?        Cannot fix?
                     └───────────────┘     │                │
                                           ▼                ▼
                                  ┌──────────────┐  ┌───────────────┐
                                  │ Apply §6     │  │ Use serializa-│
                                  │ remediation  │  │ tion framework│
                                  │ patterns     │  │               │
                                  │ Then re-check│  │ protobuf      │
                                  └──────────────┘  │ FlatBuffers   │
                                                    │ Cap'n Proto   │
                                                    │ MessagePack   │
                                                    └───────────────┘
```

### 7.2 TypeLayout's Role in the Ecosystem

TypeLayout is **not** a serialization framework. It is the **diagnostic tool**
that answers the question *before* you choose a serialization framework:

```
        ┌───────────────────────────────────────────────────┐
        │              The Serialization Question             │
        │                                                     │
        │  "Do I need to serialize this data?"                │
        │                                                     │
        │      TypeLayout answers: YES or NO                  │
        │                                                     │
        │  If NO  → zero-copy transfer (fastest possible)     │
        │  If YES → choose a framework:                       │
        │           protobuf, FlatBuffers, Cap'n Proto, etc.  │
        └───────────────────────────────────────────────────┘
```

### 7.3 Comparison with Serialization Frameworks

| Aspect | TypeLayout (Zero-Copy) | protobuf | FlatBuffers |
|--------|----------------------|----------|-------------|
| **Speed** | **Zero overhead** (raw memcpy) | Encode + decode cost | Near-zero read, encode cost |
| **Schema** | C++ struct IS the schema | `.proto` file required | `.fbs` file required |
| **Versioning** | Layout signature = version | Field numbers | Table offsets |
| **Cross-language** | C++ only | Multi-language | Multi-language |
| **Cross-endian** | ❌ Same endianness required | ✅ Handled | ✅ Handled |
| **Applicability** | Safe types + matching platforms | Universal | Universal |

### 7.4 When to Use Each

| Scenario | Recommendation | Reason |
|----------|---------------|--------|
| Same-platform IPC (shared memory) | **Zero-copy** | Same binary, trivially safe |
| Same-arch server cluster (homogeneous) | **Zero-copy** (verify with TypeLayout) | Same layout guaranteed if verified |
| Heterogeneous cloud (Linux + Windows + ARM) | **Serialization** (for non-Safe types) | Different data models |
| Client-server with version skew | **Serialization** | Different struct versions |
| Performance-critical real-time systems | **Zero-copy** (design Safe types) | Cannot afford serialization overhead |
| Multi-language systems (C++ + Python + Java) | **Serialization** | TypeLayout is C++ only |
| Embedded systems (same arch, constrained) | **Zero-copy** | Minimal overhead, no dependencies |

---

## 8. Practical Examples and Code Patterns {#8-examples}

### 8.1 Example 1: Network Protocol with Compile-Time Verification

```cpp
// === common/protocol.hpp (shared between client and server) ===
#pragma once
#include <cstdint>

struct PacketHeader {
    uint32_t magic;      // Protocol magic number
    uint16_t version;    // Protocol version
    uint16_t msg_type;   // Message type ID
    uint64_t timestamp;  // Unix timestamp (nanoseconds)
    uint32_t payload_len; // Payload length in bytes
    uint32_t checksum;   // CRC32 of payload
};
// Signature: record[s:24,a:8]{@0:u32,@4:u16,@6:u16,@8:u64,@16:u32,@20:u32}
// Safety: Safe ✅ (all fixed-width types)

struct SensorPayload {
    int32_t  sensor_id;
    double   value;
    double   min_range;
    double   max_range;
    uint8_t  unit_code;
    uint8_t  status;
    uint16_t reserved;
};
// Safety: Safe ✅ (all fixed-width types + IEEE 754 floats)
```

```cpp
// === Phase 2: compile-time verification ===
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_linux_clang.sig.hpp"
#include <boost/typelayout/tools/compat_check.hpp>

namespace linux_x86 = boost::typelayout::platform::x86_64_linux_clang;
namespace linux_arm = boost::typelayout::platform::arm64_linux_clang;

// Compile-time guarantee: protocol is zero-serialization safe
static_assert(boost::typelayout::compat::sig_match(
    linux_x86::PacketHeader_layout, linux_arm::PacketHeader_layout),
    "PacketHeader layout mismatch!");

static_assert(boost::typelayout::compat::sig_match(
    linux_x86::SensorPayload_layout, linux_arm::SensorPayload_layout),
    "SensorPayload layout mismatch!");

// If this file compiles → zero-copy send/recv is safe!
```

```cpp
// === Application: zero-copy send/recv ===
void send_reading(int sock, const SensorPayload& reading) {
    PacketHeader hdr{};
    hdr.magic       = 0xBEEFCAFE;
    hdr.version     = 1;
    hdr.msg_type    = 0x0001;  // SENSOR_DATA
    hdr.timestamp   = get_nanos();
    hdr.payload_len = sizeof(SensorPayload);
    hdr.checksum    = crc32(&reading, sizeof(reading));

    // Zero-serialization: send structs directly
    send(sock, &hdr, sizeof(hdr), 0);
    send(sock, &reading, sizeof(reading), 0);
}

SensorPayload recv_reading(int sock) {
    PacketHeader hdr;
    recv_exact(sock, &hdr, sizeof(hdr));
    assert(hdr.magic == 0xBEEFCAFE);
    assert(hdr.payload_len == sizeof(SensorPayload));

    SensorPayload reading;
    recv_exact(sock, &reading, sizeof(reading));
    assert(hdr.checksum == crc32(&reading, sizeof(reading)));
    return reading;  // No deserialization needed!
}
```

### 8.2 Example 2: Mixed Collection — Partial Zero-Copy

```cpp
// Some types are Safe, some are not
struct ConfigHeader {     // 🟢 Safe — zero-copy OK
    uint32_t version;
    uint32_t num_entries;
    uint64_t timestamp;
};

struct ConfigEntry {      // 🟡 Conditional — has pointer
    uint32_t    key_hash;
    uint32_t    value_len;
    const char* value_ptr;  // ← pointer! Not portable
};

struct ConfigEntryWire {  // 🟢 Safe — remediated version
    uint32_t key_hash;
    uint32_t value_len;
    uint32_t value_offset;  // Offset into payload buffer instead of pointer
    uint32_t reserved;
};
```

```cpp
// Runtime report to verify
void check_protocol_compat() {
    boost::typelayout::compat::CompatReporter reporter;
    reporter.add_platform(linux_x86::get_platform_info());
    reporter.add_platform(linux_arm::get_platform_info());
    reporter.print_report();
}

// Expected output:
// Type              Layout  Definition  Safety  Verdict
// ConfigHeader      MATCH   MATCH       ***     Serialization-free
// ConfigEntry       MATCH   MATCH       **-     Layout OK (pointer values not portable)
// ConfigEntryWire   MATCH   MATCH       ***     Serialization-free
```

### 8.3 Example 3: The Complete Workflow

```
Step 1: Define types with zero-serialization in mind
        → Use fixed-width types (uint32_t, double, etc.)
        → Avoid long, wchar_t, bit-fields, pointers

Step 2: Generate signatures on each target platform
        $ docker run --rm -v $PWD:/src linux-x86-builder ./sig_export
        $ docker run --rm -v $PWD:/src arm-linux-builder ./sig_export
        → Produces: sigs/x86_64_linux_clang.sig.hpp
                    sigs/arm64_linux_clang.sig.hpp

Step 3: Compile-time verification
        #include both .sig.hpp files
        static_assert(sig_match(plat_a::Type_layout, plat_b::Type_layout))
        → Compilation succeeds? Zero-copy is safe!
        → Compilation fails? Fix the type or use serialization.

Step 4: Runtime verification (optional, for human review)
        CompatReporter::print_report()
        → Review safety levels and verdicts

Step 5: Deploy with confidence
        → send(&struct, sizeof) directly
        → No protobuf, no FlatBuffers, no overhead
```

---

## Summary

### The One-Line Answer

> **Ensure `sig_layout(T)` matches across all target platforms AND `classify_safety(sig)` returns `Safe` — then you can `send()/recv()` the struct as raw bytes with zero serialization.**

### The Two Conditions (+ One Axiom)

| # | Condition | Verified By | Covers |
|---|-----------|-------------|--------|
| **C1** | Layout Signature Match | `sig_layout` string equality | Layout + endianness + pointer width |
| **C2** | Safety = Safe | `classify_safety == Safe` | No pointers + no bit-fields + no platform-dep |
| **A1** | IEEE 754 (axiom) | Documented assumption | Floating-point interpretation |

### The Type Taxonomy

| Category | Zero-Copy? | Action |
|----------|-----------|--------|
| 🟢 Safe | **Yes, always** | Use directly |
| 🟡 Conditional | **Sometimes** | Verify per-platform-pair, or remediate |
| 🔴 Unsafe | **No** | Remediate (§6) or serialize |

### The Decision

```
TypeLayout says MATCH + Safe  →  Zero-copy (fastest)
TypeLayout says anything else →  Serialize (safest)
```

---

## References

- [Cross-Platform Collection Compatibility Analysis](./CROSS_PLATFORM_COLLECTION.md) — Companion analysis of three representative types across three platforms
- Boost.TypeLayout Signature Specification: `openspec/specs/signature/spec.md`
- Cross-Platform Compatibility Specification: `openspec/specs/cross-platform-compat/spec.md`
- V1 (Layout Match Theorem), V2 (Definition Match Theorem), V3 (Projection Theorem)
- ISO C++ [class.bit] §12.2.4: Bit-field allocation is implementation-defined
- IEEE 754-2019: Standard for Floating-Point Arithmetic
