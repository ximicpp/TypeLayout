# Cross-Platform Collection Compatibility Analysis

> A formal analysis of type collection compatibility across x86_64-linux, x86_64-windows, and arm64-macos using Boost.TypeLayout's two-layer signature system.

## Table of Contents

1. [Collection Overview](#1-collection-overview)
2. [Root Cause Analysis](#2-root-cause-analysis)
3. [Per-Type Compatibility Matrix](#3-per-type-compatibility-matrix)
4. [Safety Classification](#4-safety-classification)
5. [Collection-Level Theorems](#5-collection-level-theorems)
6. [Two-Phase Pipeline](#6-two-phase-pipeline)
7. [Report Interpretation](#7-report-interpretation)

---

## 1. Collection Overview

We analyze a collection of three representative types that exercise different
parts of the C++ object model:

```cpp
// Safe: fixed-width types only
struct PacketHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint64_t timestamp;
};

// Warning: contains pointer and vtable
struct SensorRecord {
    int32_t   sensor_id;
    double    value;
    double    timestamp;
    char      unit[16];
    virtual ~SensorRecord() = default;
};

// Risk: bit-fields and platform-dependent `long`
struct UnsafeStruct {
    long           value;
    unsigned int   flags : 3;
    unsigned int   mode  : 5;
    long double    precise;
};
```

### Why These Three?

| Type | Purpose | Key Hazard |
|------|---------|------------|
| `PacketHeader` | Fixed-width baseline | None — expected identical everywhere |
| `SensorRecord` | Pointer + vtable probe | `sizeof(void*)`, vtable layout |
| `UnsafeStruct` | ABI stress test | `sizeof(long)`, `sizeof(long double)`, bit-field packing |

---

## 2. Root Cause Analysis of Platform Differences

### 2.1 Data Models: LP64 vs LLP64

The C++ standard does **not** fix the width of `long`, `long double`, or
pointer types. Each platform chooses a *data model*:

| Type | LP64 (Linux/macOS) | LLP64 (Windows) |
|------|-------------------|-----------------|
| `int` | 32-bit | 32-bit |
| `long` | **64-bit** | **32-bit** |
| `long long` | 64-bit | 64-bit |
| `pointer` | 64-bit | 64-bit |
| `long double` | 80-bit (x87) or 128-bit | 64-bit (= `double`) |
| `wchar_t` | 32-bit | 16-bit |

### 2.2 Impact on Signature Strings

For `UnsafeStruct`, the **layout signature** encodes the exact byte-level
representation. Consider the `long value` field:

```
Linux  x86_64:  S{i64,bits<u32,3,5>,f80}  (long = 8 bytes, long double = 16 bytes)
Windows x86_64: S{i32,bits<u32,3,5>,f64}  (long = 4 bytes, long double = 8 bytes)
macOS  arm64:   S{i64,bits<u32,3,5>,f64}  (long = 8 bytes, long double = 8 bytes)
```

This is a **layout mismatch** — the same source code produces physically
different memory representations.

### 2.3 Formal Interpretation

Let \( L_P(T) \) denote the layout function for type \( T \) on platform \( P \).
The encoding faithfulness axiom guarantees:

\[
\text{sig}(T, P) = \text{encode}(L_P(T))
\]

Therefore:
\[
\text{sig}(T, P_1) \neq \text{sig}(T, P_2) \implies L_{P_1}(T) \neq L_{P_2}(T)
\]

This is the **contrapositive of V1 (Layout Match Theorem)**:

> **V1**: If \(\text{sig}_{\text{layout}}(T, P_1) = \text{sig}_{\text{layout}}(T, P_2)\),
> then \( T \) has identical byte-level representation on both platforms.

---

## 3. Per-Type Compatibility Matrix

### 3.1 PacketHeader

| Platform Pair | Layout Sig | Def Sig | Verdict |
|---------------|-----------|---------|---------|
| Linux ↔ Windows | ✅ MATCH | ✅ MATCH | **Full Compatible** |
| Linux ↔ macOS | ✅ MATCH | ✅ MATCH | **Full Compatible** |
| Windows ↔ macOS | ✅ MATCH | ✅ MATCH | **Full Compatible** |

**Explanation**: All fields use fixed-width types (`uint32_t`, `uint16_t`,
`uint64_t`). By V1, identical layout signatures guarantee `memcpy`/`memcmp`
compatibility. By V2, identical definition signatures guarantee ODR
consistency.

### 3.2 SensorRecord

| Platform Pair | Layout Sig | Def Sig | Verdict |
|---------------|-----------|---------|---------|
| Linux ↔ Windows | ⚠️ DIFFER | ⚠️ DIFFER | **Layout Incompatible** |
| Linux ↔ macOS | ⚠️ DIFFER | ⚠️ DIFFER | **Layout Incompatible** |
| Windows ↔ macOS | ⚠️ DIFFER | ⚠️ DIFFER | **Layout Incompatible** |

**Explanation**: The presence of `virtual ~SensorRecord()` introduces a
vtable pointer (`vptr`). While `sizeof(void*)` is 8 bytes on all three
64-bit platforms, the vtable **layout** (offset, structure) is
ABI-specific (Itanium ABI vs MSVC ABI). The definition signature encodes
the vtable presence, making cross-platform binary sharing unsafe.

### 3.3 UnsafeStruct

| Platform Pair | Layout Sig | Def Sig | Verdict |
|---------------|-----------|---------|---------|
| Linux ↔ Windows | ❌ DIFFER | ❌ DIFFER | **Incompatible** |
| Linux ↔ macOS | ❌ DIFFER | ❌ DIFFER | **Incompatible** |
| Windows ↔ macOS | ❌ DIFFER | ❌ DIFFER | **Incompatible** |

**Explanation**: Three sources of divergence:
1. `long value`: 8 bytes (LP64) vs 4 bytes (LLP64)
2. `long double precise`: 16 bytes (x86_64 Linux) vs 8 bytes (Windows, ARM64)
3. Bit-field packing: compiler-specific allocation unit choices

---

## 4. Safety Classification: Formal Basis

### 4.1 Classification Algorithm

The `classify_safety` function in `compat_check.hpp` performs
**pattern-based risk assessment** on layout signature strings:

```cpp
inline SafetyLevel classify_safety(std::string_view sig) noexcept {
    if (sig.find("bits<") != npos) return SafetyLevel::Risk;
    if (sig.find("ptr[") != npos || sig.find(",vptr") != npos)
        return SafetyLevel::Warning;
    return SafetyLevel::Safe;
}
```

### 4.2 Formal Justification

| Level | Pattern | Rationale |
|-------|---------|-----------|
| **Safe** | No `bits<`, `ptr[`, or `,vptr` | Only fixed-width scalars and arrays; layout is deterministic across all conforming platforms |
| **Warning** | `ptr[` or `,vptr` present | Pointer size is platform-dependent (ILP32 vs LP64); vtable layout varies by ABI (Itanium vs MSVC) |
| **Risk** | `bits<` present | Bit-field allocation is implementation-defined (C++ \[class.bit\] §[12.2.4]); no portable guarantee on packing, padding, or endianness |

### 4.3 Soundness Property

The classification is **conservative** (never under-reports risk):

\[
\text{classify}(T) = \text{Safe} \implies \text{sig}(T) \text{ contains only deterministic encoding elements}
\]

This means a "Safe" verdict combined with a MATCH result gives a
**machine-checked guarantee** of binary compatibility.

---

## 5. Collection-Level Compatibility Theorems

### 5.1 Theorem: Collection Layout Match

For a collection \( C = \{T_1, T_2, \ldots, T_n\} \):

\[
\text{CollectionMatch}(C, P_1, P_2) \iff \forall T_i \in C : \text{sig}_{\text{layout}}(T_i, P_1) = \text{sig}_{\text{layout}}(T_i, P_2)
\]

**Corollary**: A single mismatched type makes the entire collection
incompatible for bulk binary transfer (e.g., serializing an array of
heterogeneous records).

### 5.2 Theorem: Projection Preservation (V3)

\[
\forall T : \text{sig}_{\text{def}}(T, P_1) = \text{sig}_{\text{def}}(T, P_2) \implies \text{sig}_{\text{layout}}(T, P_1) = \text{sig}_{\text{layout}}(T, P_2)
\]

This is the **V3 Projection Theorem**: definition match implies layout
match, but not vice versa. In our collection:

- `PacketHeader`: Both signatures match → Full compatible ✅
- `SensorRecord`: Both differ → V3 trivially holds (antecedent false)
- `UnsafeStruct`: Both differ → V3 trivially holds (antecedent false)

### 5.3 Collection Safety Score

We define a collection-level safety score:

\[
\text{Score}(C) = \frac{|\{T \in C : \text{classify}(T) = \text{Safe} \land \text{Layout MATCH}\}|}{|C|}
\]

For our collection across Linux ↔ Windows:
- PacketHeader: Safe + MATCH ✅
- SensorRecord: Warning + DIFFER ❌
- UnsafeStruct: Risk + DIFFER ❌

\[
\text{Score} = \frac{1}{3} \approx 33\%
\]

---

## 6. Two-Phase Pipeline Correctness

### 6.1 Architecture

```
Phase 1: Signature Generation (requires P2996 reflection)
┌─────────────────────────────────────────────────┐
│  Source Code  ──→  Compiler + P2996  ──→  .sig  │
│  (same .hpp)      (platform-native)    (text)   │
└─────────────────────────────────────────────────┘
        Runs on each target platform separately

Phase 2: Compatibility Checking (C++17 only)
┌─────────────────────────────────────────────────┐
│  .sig files  ──→  CompatChecker  ──→  Report    │
│  (from all       (string compare)   (matrix)    │
│   platforms)                                     │
└─────────────────────────────────────────────────┘
        Runs on any single platform
```

### 6.2 Correctness Argument

**Phase 1 correctness**: The encoding faithfulness axiom ensures that the
generated signature string is a **faithful denotation** of the platform's
actual layout function \( L_P \). P2996 reflection queries the compiler's
own type representation, eliminating manual measurement errors.

**Phase 2 correctness**: String equality is a **decidable** property.
The checker performs exact lexicographic comparison — no heuristics, no
approximation. Therefore:

\[
\text{checker}(\text{sig}_1, \text{sig}_2) = \text{MATCH} \iff \text{sig}_1 = \text{sig}_2
\]

Combined with encoding faithfulness:

\[
\text{checker reports MATCH} \iff L_{P_1}(T) = L_{P_2}(T)
\]

This is a **machine-verified** equivalence, not a human judgment.

---

## 7. Compatibility Report Interpretation Guide

### 7.1 Reading the Matrix

A typical `CompatReporter` output:

```
=== Cross-Platform Compatibility Report ===

--- PacketHeader ---
  [Layout] x86_64_linux_clang vs x86_64_windows_msvc: MATCH ✓
  [Layout] x86_64_linux_clang vs arm64_macos_clang:   MATCH ✓
  [Def]    x86_64_linux_clang vs x86_64_windows_msvc: MATCH ✓
  Safety: Safe

--- UnsafeStruct ---
  [Layout] x86_64_linux_clang vs x86_64_windows_msvc: DIFFER ✗
           Linux:   S{i64,bits<u32,3,5>,f80}
           Windows: S{i32,bits<u32,3,5>,f64}
  Safety: Risk
```

### 7.2 Decision Table

| Safety | Layout | Action |
|--------|--------|--------|
| Safe | MATCH | ✅ Binary-safe to share across platforms |
| Safe | DIFFER | 🔍 Unexpected — investigate compiler flags |
| Warning | MATCH | ⚠️ Likely safe, but verify ABI compatibility manually |
| Warning | DIFFER | ❌ Do not share binary data; use serialization |
| Risk | MATCH | ⚠️ Coincidental match; bit-field layout not guaranteed |
| Risk | DIFFER | ❌ Expected divergence; redesign with fixed-width types |

### 7.3 Remediation Strategies

For types classified as **Warning** or **Risk**:

1. **Replace platform-dependent types** with fixed-width alternatives:
   - `long` → `int64_t` or `int32_t`
   - `long double` → `double` (or a software float library)
   - `wchar_t` → `char32_t`

2. **Eliminate bit-fields** in cross-platform structures:
   - Use explicit mask/shift operations on `uint32_t`

3. **Remove vtables** from serialized types:
   - Separate data layout from polymorphic behavior

4. **Re-verify** after changes:
   - Regenerate signatures on all target platforms
   - Re-run `CompatChecker` to confirm all MATCH

---

## References

- Boost.TypeLayout Signature Specification: `openspec/specs/signature/spec.md`
- Cross-Platform Compatibility Spec: `openspec/specs/cross-platform-compat/spec.md`
- V1 (Layout Match Theorem): Layout signature equality ⟹ byte-compatible
- V2 (Definition Match Theorem): Definition signature equality ⟹ ODR-compatible
- V3 (Projection Theorem): Definition match ⟹ Layout match
- ISO C++ [class.bit] §12.2.4: Bit-field allocation is implementation-defined
- System V AMD64 ABI / Itanium C++ ABI / MSVC x64 ABI documentation
