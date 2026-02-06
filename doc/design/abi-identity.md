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

> **Signatures encode a type's structural ABI identity. Matching signatures guarantee that types can be safely exchanged across compilation unit boundaries—not merely that their bytes are identical.**
>
> **Signature mismatch does not necessarily mean different byte layouts, but it does mean the types are not directly interchangeable in the C++ object model.**

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
