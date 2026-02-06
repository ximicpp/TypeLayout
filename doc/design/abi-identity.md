# ABI Identity: Core Value Analysis

> This document provides a deep analysis of what Boost.TypeLayout's two-layer signature system encodes and the precise boundaries of its guarantees.

## Executive Summary

The core module's fundamental proposition:

> **Encode a C++ type's ABI identity into a compile-time deterministic string, transforming layout compatibility from implicit compiler behavior into an explicit, assertable compile-time property.**

TypeLayout v2.0 provides two complementary layers of analysis:
- **Layout Signature**: Pure byte-level identity (flattened, name-erased)
- **Definition Signature**: Full type structural definition (tree, with names)

Mathematical relationship: `Layout = project(Definition)` — many Definition signatures may project to the same Layout signature.

## Two-Layer Architecture

### Layer 1: Layout Signature

Layout signatures capture pure byte-level identity:
- Uses `record` prefix for all class/struct types
- Flattens inheritance hierarchy into absolute offsets
- Excludes field names, base class names, and structural markers
- **Guarantee**: Identical byte layout → Identical signature

```
[64-le]record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}
```

### Layer 2: Definition Signature

Definition signatures capture complete type structure:
- Uses `record` prefix for all class/struct types
- Preserves inheritance tree with `~base<Name>` / `~vbase<Name>` subtrees
- Includes field names as `@OFF[name]:TYPE`
- Includes `polymorphic` marker for types with virtual functions
- Excludes the outer type's own name (redundant with template parameter)

```
[64-le]record[s:16,a:8]{~base<Base>:record[s:4,a:4]{@0[x]:i32[s:4,a:4]},@8[y]:f64[s:8,a:8]}
```

### Projection Relationship

```
Definition Signature ──project()──→ Layout Signature
    (many)                              (one)

definition_match(T, U) ⟹ layout_match(T, U)
layout_match(T, U) ⇏ definition_match(T, U)
```

## Core Guarantee

### Layout Layer Guarantee

```
layout_signatures_match<T, U>()
    ⟹ sizeof(T) == sizeof(U)
    ∧ alignof(T) == alignof(U)
    ∧ ∀ corresponding flattened fields: offset, size, type category match
```

**Completeness**: Layout-identical types will always match (inheritance is flattened, names are erased).

### Definition Layer Guarantee

```
definition_signatures_match<T, U>()
    ⟹ layout_signatures_match<T, U>()
    ∧ inheritance structure is identical
    ∧ field names match at each position
    ∧ polymorphism markers match
```

**Strictness**: Structurally different types (different inheritance, different field names) will not match even if their byte layouts are identical.

### Comparison

```cpp
struct Base { int x; };
struct Derived : Base { double y; };
struct Flat { int x; double y; };

// Layout layer: IDENTICAL (same bytes at same offsets)
static_assert(layout_signatures_match<Derived, Flat>());

// Definition layer: DIFFERENT (inheritance vs flat)
static_assert(!definition_signatures_match<Derived, Flat>());

struct PointA { float x, y; };
struct PointB { float horizontal, vertical; };

// Layout layer: IDENTICAL (same bytes)
static_assert(layout_signatures_match<PointA, PointB>());

// Definition layer: DIFFERENT (different field names)
static_assert(!definition_signatures_match<PointA, PointB>());
```

## What Signatures Encode

### Platform Context

Architecture prefix `[64-le]` encodes pointer width and byte order. Types on platforms with different pointer sizes or endianness will never match.

### Size Characteristics

Every type node carries `[s:SIZE,a:ALIGN]` annotation. For platform-dependent types (`long double`, `wchar_t`) and compound types, this is essential.

### Internal Structure

For class/struct types (always prefixed `record`):

**Layout mode** encodes:
- Each field's absolute offset and recursive type signature
- Fields from all base classes flattened to absolute offsets
- Declaration order preserved

**Definition mode** additionally encodes:
- Field names (e.g., `@0[field_name]:type`)
- Base class subtrees (e.g., `~base<ClassName>:record{...}`)
- Virtual base subtrees (e.g., `~vbase<ClassName>:record{...}`)
- `polymorphic` flag for types with virtual functions

### Byte-Array Normalization

`char[32]`, `int8_t[32]`, `uint8_t[32]`, `std::byte[32]`, `char8_t[32]` are identical in memory. Both layers normalize these to `bytes[s:32,a:1]`.

### CV Qualifier Stripping

`const T`, `volatile T`, and `T` have identical memory layouts. Both layers strip CV qualifiers.

## When to Use Each Layer

| Use Case | Recommended Layer | Why |
|----------|------------------|-----|
| Shared memory IPC | **Layout** | Only byte-level match matters |
| C interop / FFI | **Layout** | C has no inheritance; flat matching ideal |
| Network protocol | **Layout** | Wire format is byte-level |
| Binary file format | **Layout** | On-disk layout is byte-level |
| Plugin ABI contracts | **Definition** | Structural changes are breaking changes |
| ODR violation detection | **Definition** | Must match type definition exactly |
| Version evolution | **Definition** | Field name changes indicate semantic changes |
| Documentation/debugging | **Definition** | Richer, human-readable output |

## Limitations

### Virtual Inheritance (Layout Layer)

Virtual base classes have **runtime-determined offsets** that depend on the most-derived type. Layout mode **skips virtual bases** during flattening. The `sizeof`/`alignof` remain accurate, but the flattened field list may be incomplete for types with virtual inheritance.

Definition mode correctly encodes virtual bases as `~vbase<Name>` subtrees.

### Trivially Copyable Property

```cpp
struct A { int x; double y; };                          // trivially copyable
struct C { int x; double y; C(const C&) {} };           // NOT trivially copyable
```

`A` and `C` have **identical signatures at both layers** — TypeLayout examines non-static data members, not constructors/destructors.

TypeLayout signature match guarantees "data occupies the same bytes" but NOT "functions can be safely called interchangeably."

## Precise Positioning

TypeLayout is a **structural layout introspection and consistency verification tool**.

### Two-Layer Application Model

**Layout Layer Use** (Data exchange):
- Shared memory, FFI, serialization, network protocols
- Answer: "Do these types occupy memory identically?"
- Layout match is NECESSARY for safe data exchange

**Definition Layer Use** (ABI contracts):
- Plugin systems, cross-version compatibility, ODR detection
- Answer: "Are these types structurally identical definitions?"
- Definition match provides stricter guarantees

### What TypeLayout Guarantees

> **If Layout signatures match, every byte at every offset means the same thing in both types.**

> **If Definition signatures match, the types also share identical structural definitions (inheritance, field names, polymorphism).**

### What TypeLayout Does NOT Guarantee

- Safe memcpy (requires trivially copyable)
- Safe pointer dereference after copy (requires no address-sensitive members)
- Safe virtual function calls after copy (requires same vtable)
- Safe cross-ABI function calls (requires calling convention match)

---

# Appendix: Physical Layer — Byte-Level Compatibility

## Motivation

Structural mode intentionally distinguishes types with different C++ structures (inheritance, polymorphism) even when their byte-level memory layouts are identical. This is the right default for ABI safety but is too strict for certain use cases:

- **C interop**: A C struct and a C++ derived class may share identical memory but differ structurally
- **Flat data buffers**: Shared memory regions where only byte-level identity matters
- **Cross-language FFI**: Binding generators that flatten C++ hierarchies into flat types
- **Legacy migration**: Replacing an inheritance-based type with a flat equivalent

## Physical Mode

Physical mode (`SignatureMode::Physical`) strips all C++ structural metadata and produces a **flattened, offset-based** signature that captures only what exists in memory:

```cpp
struct Base { int x; };
struct Derived : Base { double y; };
struct Flat { int x; double y; };

// Structural mode: DIFFERENT (inheritance encoded)
static_assert(!signatures_match<Derived, Flat>());

// Physical mode: IDENTICAL (same bytes at same offsets)
static_assert(physical_signatures_match<Derived, Flat>());
```

### What Physical Signatures Encode

- `sizeof` and `alignof` of the type
- Every field's **absolute byte offset** from the object start
- Every field's type signature (recursively physical)
- Byte-array normalization (same as Structural mode)

### What Physical Signatures Strip

- `struct` vs `class` distinction
- `polymorphic` / `inherited` flags
- Base class subobject boundaries (`~base:` / `~vbase:` prefixes)
- Virtual inheritance metadata

### Signature Format

Physical signatures use the `record` prefix instead of `struct`/`class`:

```
record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}
```

No metadata flags (`polymorphic`, `inherited`) appear.

### Inheritance Flattening

Physical mode recursively flattens the inheritance tree. For each non-virtual base class, the base's fields are re-emitted at their **absolute offsets** (base offset + field offset within base):

```cpp
struct A { int x; };           // x at offset 0 within A
struct B : A { double y; };    // A subobject at offset 0, y at offset 8
struct C : B { char z; };      // B subobject at offset 0, z at offset 16

// Physical signature of C:
// record[s:24,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8],@16:i8[s:1,a:1]}
```

This is identical to a flat struct `{ int x; double y; char z; }`.

### Virtual Inheritance Limitation

Virtual base classes have **runtime-determined offsets** that depend on the most-derived type. Physical mode **skips virtual bases** during flattening — only direct (non-virtual) fields are included. The `sizeof`/`alignof` remain accurate, but the flattened field list may be incomplete for types with virtual inheritance.

This is a deliberate v1 limitation. Users requiring virtual inheritance support should use Structural mode, which correctly encodes `~vbase:` subobjects.

## Physical vs Structural: Selection Guide

| Criterion | Structural (default) | Physical |
|-----------|---------------------|----------|
| Inheritance-aware | ✅ Yes | ❌ No (flattened) |
| Polymorphism markers | ✅ Yes | ❌ No |
| `Derived` vs `Flat` equivalence | ❌ Different | ✅ Same |
| ABI safety (vtable, calling convention) | Stricter | Relaxed |
| C interop / FFI | Too strict | ✅ Ideal |
| Shared memory (POD types) | ✅ Good | ✅ Good |
| Virtual inheritance | ✅ Correct | ⚠️ Incomplete |

### When to Use Structural Mode

- Plugin systems (vtable layout matters)
- Cross-version ABI contracts (inheritance changes are breaking)
- Any scenario where C++ type structure affects correctness

### When to Use Physical Mode

- C/FFI interop (foreign types have no C++ inheritance)
- Flat buffer verification (only byte offsets matter)
- Migrating from inherited to flat types (verifying equivalence)
- Hardware register overlays (pure memory mapping)

## Guarantee Comparison

**Structural mode**:
```
signatures_match<T, U>()
    ⟹ sizeof, alignof, field offsets, field types match
    ∧ structural properties (polymorphic, inherited) match
```

**Physical mode**:
```
physical_signatures_match<T, U>()
    ⟹ sizeof, alignof, flattened field offsets, field types match
    (structural properties NOT checked)
```

Physical mode provides **completeness** (layout-identical types will match) at the cost of reduced **strictness** (structurally different types may also match).

---

# Appendix: ABI vs Layout — Precise Distinction

## The Difference

**Memory Layout** is a purely spatial concept:
- Size (`sizeof`)
- Alignment (`alignof`)
- Field offsets
- Padding bytes
- Byte ordering

**ABI** encompasses layout PLUS:
- Calling conventions (register vs stack parameter passing)
- Name mangling
- Vtable layout mechanics
- RTTI representation
- Exception handling tables
- Object construction/destruction protocols
- Whether a type is "trivially copyable" (affects parameter passing)
- How inheritance affects calling conventions

## Where TypeLayout Sits

```
Pure Memory Layout ⊂ TypeLayout Signature ⊂ Complete ABI
```

TypeLayout captures:
- ✅ All memory layout information
- ✅ Platform context (pointer width, endianness)
- ✅ Structural properties (polymorphic, inherited)

TypeLayout does NOT capture:
- ❌ Calling conventions
- ❌ Name mangling rules
- ❌ Vtable internal layout
- ❌ Exception handling mechanisms
- ❌ **Trivially copyable property** (critical blind spot)

## Critical Blind Spot: Trivially Copyable

```cpp
struct A { int x; double y; };                          // trivially copyable
struct C { int x; double y; C(const C&) {} };           // NOT trivially copyable
```

`A` and `C` have **identical TypeLayout signatures**—TypeLayout only examines non-static data members, not constructors/destructors.

But in Itanium C++ ABI (x86-64 Linux/macOS):
- `A` as function parameter: passed via registers (RDI + XMM0)
- `C` as function parameter: passed via pointer to stack copy

**Consequence**: TypeLayout signature match guarantees "data can be safely memcpy'd" but NOT "functions can be safely called interchangeably."

## Four Key Propositions

1. **Layout consistency is necessary but not sufficient for ABI compatibility.**
   Two ABI-compatible types must have identical layouts, but identical layouts don't guarantee ABI compatibility.

2. **TypeLayout signature match is sufficient for layout consistency.**
   Signatures encode all layout info plus structural properties.

3. **TypeLayout signature match is NOT sufficient for ABI compatibility.**
   Signatures don't encode calling conventions, trivially_copyable, or name mangling.

4. **TypeLayout signature match is NOT necessary for layout consistency.**
   Structural property markers cause layout-identical but structurally-different types to have different signatures.

## Use Case Boundaries

| Scenario | TypeLayout Sufficient? |
|----------|------------------------|
| Shared memory data exchange (POD types) | ✅ Yes |
| Serialization/deserialization (POD types) | ✅ Yes |
| File format verification | ✅ Yes |
| Layout stability regression testing | ✅ Yes |
| Cross-version layout contract verification | ✅ Yes |
| Type layout documentation | ✅ Yes |
| Shared memory with pointer members | ⚠️ Necessary, not sufficient |
| Polymorphic types across boundaries | ⚠️ Necessary, not sufficient |
| FFI function pointer casting | ❌ No (need calling convention check) |
| Dynamic linker symbol replacement | ❌ No (need full ABI check) |
| Cross-compiler binary compatibility | ❌ No (different ABIs possible) |

## Precise Positioning

TypeLayout is a **structural layout introspection and consistency verification tool**.

### Three-Layer Application Model

**Layer 1: Layout Contract Verification** (Primary use case)
- Compile-time assertions that type layout matches expectations
- Cross-version layout stability checking
- Regression detection when layout accidentally changes
- *No runtime behavior assumed*

**Layer 2: Data Compatibility Precondition** (Derived use case)
- Layout match as NECESSARY condition for safe data exchange
- User must additionally verify: trivially copyable, no pointers, no vtables
- *Layout isomorphism ≠ memcpy safety*

**Layer 3: Diagnostics & Documentation** (Auxiliary use case)
- Precise, comparable layout description language
- Human-readable type structure documentation
- Cross-platform/cross-compiler layout comparison

### What TypeLayout Guarantees

> **If signatures match, every byte at every offset means the same thing in both types.**

This is a **layout identity** guarantee, not a **usage safety** guarantee.

### What TypeLayout Does NOT Guarantee

- Safe memcpy (requires trivially copyable)
- Safe pointer dereference after copy (requires no address-sensitive members)
- Safe virtual function calls after copy (requires same vtable)
- Safe cross-ABI function calls (requires calling convention match)