# TypeLayout Design Rationale

This document explains the key design decisions and trade-offs made in TypeLayout.

## 1. Overview

TypeLayout is a compile-time type layout verification library built on C++26 static reflection (P2996). This document explains why specific design choices were made.

## 2. Core Design Decisions

### 2.1 Signature Format

**Decision**: Use a human-readable string format for layout signatures.

```
[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}
```

**Rationale**:
- **Debuggability**: Developers can read and understand the signature directly
- **Diff-friendly**: String diffs clearly show what changed between versions
- **Portability**: Plain ASCII text works everywhere

**Alternatives Considered**:
- Binary format: More compact but not human-readable
- JSON/XML: Too verbose, parsing overhead at compile-time

**Trade-offs**:
- Longer signatures for complex types (mitigated by hash verification)
- String manipulation at compile-time requires careful implementation

### 2.2 Platform Prefix

**Decision**: Include architecture and endianness in every signature.

```
[64-le]  // 64-bit, little-endian
[32-be]  // 32-bit, big-endian
```

**Rationale**:
- Layout varies by platform (e.g., pointer size, `long` size)
- Prevents accidental cross-platform binary sharing
- Makes platform dependency explicit and visible

**Trade-offs**:
- Cannot compare signatures across platforms directly
- This is intentional: cross-platform binary compatibility requires explicit handling

### 2.3 Dual-Hash Verification

**Decision**: Use two independent hash algorithms (FNV-1a + DJB2).

```cpp
struct LayoutVerification {
    uint64_t fnv1a_hash;
    uint64_t djb2_hash;
};
```

**Rationale**:
- Single 64-bit hash has ~2^64 collision resistance
- Two independent hashes provide ~2^128 collision resistance
- Different algorithms have different bit distribution properties
- FNV-1a: Good avalanche effect, widely used
- DJB2: Simple, different mathematical basis

**Alternatives Considered**:
- Single hash: Sufficient for most cases, but we chose extra safety
- Cryptographic hash (SHA-256): Overkill for layout verification, slower

**Trade-offs**:
- Slightly more storage (16 bytes vs 8 bytes)
- Negligible impact since computation is compile-time

### 2.4 Field Name Inclusion

**Decision**: Include field names in signatures.

```
@0[x]:i32  // Field named 'x' at offset 0
@4[y]:i32  // Field named 'y' at offset 4
```

**Rationale**:
- Renaming a field may indicate semantic change
- Helps debugging: signature shows exactly what fields exist
- Matches developer mental model

**Alternatives Considered**:
- Offset-only: Would miss semantic changes from renaming
- Type-only: Would miss field reordering within same type

**Trade-offs**:
- Anonymous members require synthetic names (`<anon:N>`)

### 2.5 Compile-Time Only

**Decision**: All signature generation is `consteval` (compile-time only).

**Rationale**:
- Zero runtime overhead
- Errors detected at compile time
- No runtime dependencies
- Works with `static_assert`

**Trade-offs**:
- Cannot handle types unknown at compile time
- This is acceptable: layout verification is meaningful only for known types

## 3. API Design Decisions

### 3.1 Minimal Public API

**Decision**: Expose only 6 functions + 1 macro + 4 concepts.

```cpp
// Core functions
get_layout_signature<T>()
get_layout_hash<T>()
get_layout_verification<T>()
signatures_match<T, U>()

// Concept
LayoutCompatible<T, U>

// Macro
TYPELAYOUT_BIND(Type, ExpectedSignature)
```

**Rationale**:
- Small API surface is easier to learn and maintain
- Each function has a single, clear purpose
- Macro provides convenient static assertion

### 3.2 Header-Only Library

**Decision**: Implement as header-only with no external dependencies.

**Rationale**:
- Simple integration: just include the header
- No build system complexity
- Works with any build system
- Compile-time only: no runtime library needed

**Trade-offs**:
- Longer compile times for users (mitigated by modular headers)

### 3.3 Namespace Choice

**Decision**: Use `boost::typelayout` namespace.

**Rationale**:
- Prepared for potential Boost submission
- Follows Boost naming conventions
- `boost::` prefix indicates quality expectations

## 4. Type Support Decisions

### 4.1 STL Types

**Decision**: Support common STL types with implementation-aware signatures.

**Rationale**:
- Users need to verify layouts of types containing STL members
- STL layout varies by implementation (libstdc++, libc++, MSVC STL)

**Trade-offs**:
- Signatures are not portable across STL implementations
- This is intentional: binary layouts actually differ

### 4.2 Anonymous Members

**Decision**: Use deterministic synthetic names for anonymous members.

```cpp
struct S {
    union { int a; float b; };  // Anonymous union
};
// Signature uses: <anon:0>
```

**Rationale**:
- Anonymous members have no name in source code
- Need deterministic naming for consistent signatures
- Index-based naming (`<anon:N>`) is unambiguous

### 4.3 Bit-Fields

**Decision**: Include bit-field offset and width in signatures.

**Rationale**:
- Bit-field layout is highly platform-dependent
- Essential for binary compatibility verification
- P2996 provides bit-field reflection capabilities

## 5. Error Handling

### 5.1 Exception Safety

**Decision**: All public functions are `noexcept`.

**Rationale**:
- All operations are compile-time only
- No runtime allocation or I/O
- Errors manifest as compile-time failures, not exceptions

### 5.2 SFINAE vs Concepts

**Decision**: Use C++20 concepts for constraints.

```cpp
template<typename T>
    requires Reflectable<T>
consteval auto get_layout_signature();
```

**Rationale**:
- Cleaner error messages than SFINAE
- Better expresses intent
- C++26 target already requires C++20 features

## 6. Future Considerations

### 6.1 P2996 Evolution

The library will adapt to P2996 changes as the proposal evolves toward standardization. Key areas to monitor:
- `offset_of()` behavior for various member types
- Bit-field reflection capabilities
- Anonymous member handling

### 6.2 Potential Extensions

The current minimal design intentionally leaves room for:
- Cross-platform compatibility checking (explicit opt-in)
- Custom signature formatters (for specific use cases)
- Integration with serialization libraries

---

*Document Version: 1.0*  
*Last Updated: 2026-02*
