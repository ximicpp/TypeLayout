# Value Proposition: TypeLayout

## One-Line Pitch

> **TypeLayout**: Compile-time memory layout verification for C++ using P2996 static reflection.

---

## The Problem

When two separately compiled C++ modules share data structures, layout mismatches cause **silent data corruption**:

- A library update changes struct padding
- A plugin compiled with different compiler flags
- A shared memory region accessed by mismatched binaries

These bugs don't cause compile errors. The code builds, links, and runs—until it corrupts memory. Symptoms appear far from the root cause, making diagnosis extremely difficult.

**Current workarounds fail:**
- Manual version numbers get forgotten
- IDL-based serialization requires rewriting all types
- Static analyzers detect problems after the fact, not prevent them

---

## The Solution

TypeLayout generates a **deterministic memory layout signature** for any C++ type at compile time:

```cpp
constexpr auto hash = get_layout_hash<MyStruct>();
```

**The core guarantee**: *Same hash ⟺ Same memory layout*

With this hash, you can:

```cpp
// Compile-time: Fail build if layout changed from contract
static_assert(get_layout_hash<Message>() == PROTOCOL_V1_HASH,
              "ABI contract violated!");
```

---

## The Core Value

### Compile-Time ABI Prevention

TypeLayout shifts ABI verification from **runtime detection** to **compile-time prevention**:

| Traditional Approach | TypeLayout |
|---------------------|------------|
| Bug discovered in production | Bug caught at compile time |
| Crash / corruption / security hole | Compilation error |
| Hours of debugging | Immediate, clear failure |

This is only possible because P2996 static reflection provides:
- Access to private members
- Exact byte offsets
- Bit-field layouts
- Inheritance hierarchies

All computed at compile time, with zero runtime cost.

---

## Summary

| Aspect | TypeLayout |
|--------|------------|
| **What** | Memory layout fingerprinting |
| **When** | Compile time |
| **Cost** | Zero runtime overhead |
| **Requires** | No code changes, no IDL, no macros |
| **Enables** | ABI contract enforcement via `static_assert` |