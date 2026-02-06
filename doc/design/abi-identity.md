# ABI Identity: Core Value Analysis

> This document provides a deep analysis of what Boost.TypeLayout's structural signature truly encodes and the precise boundaries of its guarantees.

## Executive Summary

The core module's fundamental proposition:

> **Encode a C++ type's ABI identity into a compile-time deterministic string, transforming layout compatibility from implicit compiler behavior into an explicit, assertable compile-time property.**

Key insight: Signatures encode **type ABI identity** (layout + structural properties), not just byte-level memory layout. This is stricter than raw byte comparison, but this strictness is exactly what ABI safety requires.

## Core Guarantee

### The Guarantee is Unidirectional

**Signatures match → Layouts compatible (Soundness)**: ✅ Holds

If two types have identical Structural signatures, they have identical size, alignment, field offsets, and field types on the current platform.

**Layouts identical → Signatures match (Completeness)**: ❌ Does NOT hold

Counter-example:
```cpp
struct Base { int x; };
struct Derived : Base { double y; };
struct Flat { int x; double y; };
```

On LP64 platforms, `Derived` and `Flat` have identical byte layouts (int at offset 0, double at offset 8, total size 16, alignment 8), but different signatures:
- `Derived`: `class[s:16,a:8,inherited]{@0~base:struct[s:4,a:4]{@0:i32[...]},@8:f64[...]}`
- `Flat`: `struct[s:16,a:8]{@0:i32[...],@8:f64[...]}`

### Precise Definition

```
get_layout_signature<T>() == get_layout_signature<U>()
    ⟹ sizeof(T) == sizeof(U)
    ∧ alignof(T) == alignof(U)
    ∧ ∀ corresponding fields: offset, size, type category match
    ∧ structural properties (polymorphic, inherited) match
```

## What Signatures Encode

### Layer 1: Platform Context

Architecture prefix `[64-le]` encodes pointer width and byte order. Types on platforms with different pointer sizes or endianness will never match.

### Layer 2: Size Characteristics

Every type node carries `[s:SIZE,a:ALIGN]` annotation. For basic types like `i32`, this is redundant. For platform-dependent types (`long double`, `wchar_t`) and compound types, this is essential.

### Layer 3: Internal Structure

For structs and unions, signatures encode:
- Each field's offset and recursive type signature
- Base class subobject offsets and signatures
- Fields in declaration order

### Layer 4: Structural Properties (Beyond Byte Layout)

Signatures additionally encode:
- `struct` vs `class` distinction (based on polymorphism or inheritance)
- `polymorphic` flag for types with virtual functions
- `inherited` flag for types with base classes
- `~base:` / `~vbase:` prefixes for base class subobjects

## Design Rationale

### Why Distinguish struct/class?

The distinction captures ABI-relevant properties:

1. **Polymorphic types have vtable pointers**: Reinterpreting non-polymorphic memory as polymorphic causes undefined behavior—vtable isn't initialized, virtual calls crash.

2. **Inheritance affects ABI**: Some platforms use different parameter passing conventions for inherited vs non-inherited types. The `is_trivially_copyable` trait can be affected by inheritance.

3. **Conservative safety**: For ABI verification, false negatives (rejecting compatible types) are safer than false positives (accepting incompatible types).

### Why Include Inheritance Information?

Even non-polymorphic, non-virtual inheritance affects:
- Construction/destruction semantics
- Slicing behavior
- Some compiler ABIs treat inherited types differently

The signature captures "can these types be safely exchanged across boundaries" rather than "do these types have identical bytes."

### Why Normalize Byte Arrays?

`char[32]`, `int8_t[32]`, `uint8_t[32]`, `std::byte[32]`, `char8_t[32]` are identical in memory. Normalizing to `bytes[s:32,a:1]` ensures:
- A library declaring `char buffer[256]`
- User code treating it as `uint8_t data[256]`
- Are recognized as compatible

### Why Strip CV Qualifiers?

`const T`, `volatile T`, and `T` have identical memory layouts. Ignoring CV qualifiers ensures a struct with `const int x` is recognized as layout-compatible with one having `int x`.

## Accurate Value Statement

The library's core value should be stated as:

> **Signatures encode a type's structural layout identity. Matching signatures guarantee that types have field-level layout isomorphism—same offsets, same sizes, same field types—not that they can be safely memcpy'd.**
>
> **Layout isomorphism is a NECESSARY but not SUFFICIENT condition for safe data exchange. Sufficient conditions also require trivially copyable types, no address-space-sensitive members (pointers, handles), and no compiler-managed implicit pointers (vtables).**

### What "Layout Isomorphism" Means

If `get_layout_signature<T>() == get_layout_signature<U>()`, then:
- `sizeof(T) == sizeof(U)`
- `alignof(T) == alignof(U)`
- Every field of T has a corresponding field in U at the same offset with the same type

This is a **structural equivalence** guarantee, not a **usage safety** guarantee.

## Implications for Users

### Safe Use Cases

1. **ABI stability checking**: Compare current type signature against a known baseline
2. **Cross-compilation-unit verification**: Ensure both sides of an interface agree on layout
3. **Serialization validation**: Verify serialized data matches current type layout

### Potential Surprises

1. **Flat struct vs inherited struct**: Even with identical fields and layouts, these will have different signatures
2. **Empty base optimization**: A derived class with an empty base and a flat struct with the same fields will differ
3. **Type aliases**: Handled correctly—`int32_t` and `int` (when same) produce identical signatures

## Technical Implementation Notes

### P2996 Reflection Limitations

The current implementation calls `nonstatic_data_members_of()` O(n) times for n members due to toolchain limitations (vector return type incompatible with `template for`). This hits constexpr step limits for structs with >40-50 members.

### Platform-Dependent Type Handling

Types like `long double` use `format_size_align()` to encode actual `sizeof`/`alignof` values rather than hardcoded strings. The `f80` prefix may be misleading on platforms where `long double` is 64-bit, but soundness is preserved through the size/alignment suffix.

## Conclusion

Boost.TypeLayout provides a sound, automatic mechanism for ABI identity verification. Its guarantees are:

- **Deterministic**: Same type + same platform → same signature
- **Sound**: Matching signatures → compatible layouts
- **Not Complete**: Compatible layouts ↛ matching signatures (by design)

This incompleteness is a feature, not a bug—it provides conservative ABI safety by encoding structural properties that affect type interchangeability beyond raw byte patterns.

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
