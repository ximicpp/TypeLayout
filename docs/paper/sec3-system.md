# §3 The TypeLayout Signature System

This section presents the design of TypeLayout's Layout signature system:
the signature grammar, the generation algorithm, and the key design decisions
that enable both soundness and practical utility.

## 3.1 Overview

A *type layout signature* is a deterministic, human-readable string that
encodes the complete memory layout of a C++ type. TypeLayout generates a
**Layout signature** ⟦T⟧_L for each type, encoding byte identity: field
offsets, sizes, alignments, and padding. Inheritance is flattened; field
names are stripped.

Signatures are computed entirely at compile time via `consteval` functions,
producing `FixedString<N>` values—compile-time string literals with no
runtime footprint.

**Design principle: structural analysis.** TypeLayout performs *structural
analysis*, not *nominal analysis*. Two differently-named types with identical
byte layouts produce identical Layout signatures. The signature does *not*
include the type's own name—by design, TypeLayout answers "do these two
types have the same byte layout?" rather than "are they the same type?"

## 3.2 Signature Grammar

The Layout signature follows a deterministic context-free grammar:

```
sig      ::= arch record
arch     ::= '[' BITS '-' ENDIAN ']'
record   ::= 'record' meta '{' fields '}'
meta     ::= '[s:' NUM ',a:' NUM ']'
fields   ::= ε | field (',' field)*
field    ::= '@' NUM ':' typesig
           | '@' NUM '.' NUM ':bits<' NUM ',' typesig '>'
typesig  ::= scalar | record | array | union | enum | opaque
scalar   ::= PREFIX meta
array    ::= 'array' meta '<' typesig ',' NUM '>'
           | 'bytes[s:' NUM ',a:1]'
union    ::= 'union' meta '{' fields '}'
enum     ::= 'enum' meta '<' typesig '>'
opaque   ::= 'O(' TAG '|' NUM '|' NUM ')'
```

where `PREFIX` is a closed set of built-in type prefixes:
`PREFIX ∈ {i8, u8, i16, u16, i32, u32, i64, u64, f32, f64, fld,
char, char8, char16, char32, wchar, bool, byte, nullptr, ptr, ref, rref,
memptr, fnptr}`.

`TAG` is a user-defined identifier registered via `TYPELAYOUT_REGISTER_OPAQUE`.
`TAG` must not collide with any `PREFIX` or keyword (`record`, `array`,
`bytes`, `union`, `enum`, `bits`).

**Example.** A struct with a 32-bit integer and a 64-bit integer:

```cpp
struct Message { uint32_t id; uint64_t timestamp; };
```

Layout signature:
```
[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8]}
```

The signature encodes: 64-bit little-endian platform, record of size 16 and
alignment 8, with fields at offsets 0 and 8. The 4 bytes of padding between
offsets 4 and 8 are implicit (the gap between `@0` + 4 bytes and `@8`).

**Inheritance example:**

```cpp
struct Base { int32_t x; };
struct Derived : Base { int32_t y; };
```

Layout signature (inheritance flattened):
```
[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}
```

Note that `Derived` and `struct Flat { int32_t x, y; }` produce *identical*
Layout signatures when their byte layouts match — which is by design.
TypeLayout answers "same bytes?" not "same structure?".

## 3.3 Generation Algorithm

The signature generation is implemented as a recursive `consteval` function
that traverses the type structure via P2996 reflection.

**Algorithm 1: Layout Signature Generation**

```
function LAYOUT_SIG(T, platform P):
    prefix ← "[" + str(P.bits) + "-" + P.endian + "]"
    return prefix + LAYOUT_RECORD(T)

function LAYOUT_RECORD(T):
    header ← "record[s:" + str(sizeof(T)) + ",a:" + str(alignof(T)) + "]{"
    body ← FLATTEN(T, offset_adjustment=0)
    return header + body + "}"

function FLATTEN(T, adj):
    result ← ""
    if introduces_vptr(T):
        result ← result + "@" + str(adj) + ":ptr[s:" + str(sizeof(void*))
                 + ",a:" + str(alignof(void*)) + "]"
    for each base B in bases_of(T):
        result ← result + FLATTEN(type_of(B), offset_of(B) + adj)
    for each member M in nonstatic_data_members_of(T):
        off ← offset_of(M) + adj
        if is_bit_field(M):
            result ← result + BIT_FIELD_SIG(M, off)
        else if is_class(type_of(M)) and not is_union(type_of(M)):
            result ← result + FLATTEN(type_of(M), off)
        else:
            result ← result + "@" + str(off) + ":" + TYPE_SIG(type_of(M))
    return result

function introduces_vptr(T):
    return is_polymorphic(T) and not any(is_polymorphic(base) for base in bases_of(T))
```

*Implementation note:* Fields are accumulated using a comma-prefix strategy
(each field string starts with `,`); the leading comma is removed by a
`skip_first()` operation, equivalent to `join(",", fields)`.

Key design decisions in the algorithm:

1. **Inheritance flattening.** Base class fields are recursively expanded with
   adjusted offsets. This ensures that `struct Derived : Base { int y; }` and
   `struct Flat { int x; int y; }` produce identical Layout signatures when
   their byte layouts are identical.

2. **Nested struct flattening.** Non-union class members are recursively
   expanded to their leaf fields. This ensures the signature captures the
   actual memory layout, not the logical nesting structure.

3. **Union preservation.** Union members are *not* flattened, because union
   members overlap in memory. Flattening would produce misleading offsets.
   Instead, each union member's complete type signature is preserved.

4. **Byte array normalization.** Arrays of `char`, `unsigned char`,
   `uint8_t`, `int8_t`, and `std::byte` are all normalized to
   `bytes[s:N,a:1]`, since they represent identical raw byte regions.

5. **Bit-field encoding.** Bit-fields receive an extended format
   `@byte.bit:bits<width,type>` that captures the byte offset, bit offset
   within that byte, bit width, and underlying type.

## 3.4 Supported Type Categories

TypeLayout generates signatures for all standard-layout and non-standard-
layout C++ types:

| Category | Layout signature |
|----------|-----------------|
| Fixed-width integers | `i32[s:4,a:4]` |
| Floating point | `f64[s:8,a:8]` |
| Character types | `char16[s:2,a:2]` |
| Pointers | `ptr[s:8,a:8]` |
| References | `ref[s:8,a:8]` |
| Enums | `enum[s:4,a:4]<i32[...]>` |
| Arrays | `array[s:12,a:4]<i32[...],3>` |
| Byte arrays | `bytes[s:16,a:1]` |
| Structs | `record[s:N,a:M]{@off:type,...}` |
| Inheritance | Flattened to leaf fields with adjusted offsets |
| Polymorphic types | Synthesized `ptr[s:N,a:N]` field at vptr offset |
| Unions | `union[s:N,a:M]{@0:type,...}` |
| Bit-fields | `@B.b:bits<W,type>` |
| Opaque types | `O(Tag\|N\|M)` |

## 3.5 Public API

The public interface consists of two `consteval` functions and two helpers:

```cpp
namespace boost::typelayout {
    // Layout signature generation
    template<class T>
    consteval auto get_layout_signature();        // → FixedString<N>

    // Signature comparison via FixedString::operator==
    // get_layout_signature<A>() == get_layout_signature<B>()  → bool
}
```

`get_layout_signature` returns a compile-time `FixedString<N>` value.
Signature comparison uses `FixedString::operator==` directly. Both can
be used in `static_assert`, `constexpr if`, template constraints (`requires`),
and any other compile-time context.
