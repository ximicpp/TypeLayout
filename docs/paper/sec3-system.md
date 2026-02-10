# §3 The TypeLayout Signature System

This section presents the design of TypeLayout's two-layer signature system:
the signature grammar, the generation algorithm, and the key design decisions
that enable both soundness and practical utility.

## 3.1 Overview

A *type layout signature* is a deterministic, human-readable string that
encodes the complete memory layout of a C++ type. TypeLayout generates two
kinds of signatures for each type:

- **Layout signature** ⟦T⟧_L — encodes byte identity: field offsets, sizes,
  alignments, and padding. Inheritance is flattened; field names are stripped.
- **Definition signature** ⟦T⟧_D — encodes structural identity: field names,
  inheritance hierarchy, qualified enum names, and polymorphism markers, in
  addition to all layout information.

Both signatures are computed entirely at compile time via `consteval`
functions, producing `FixedString<N>` values—compile-time string literals
with no runtime footprint.

**Design principle: structural analysis.** TypeLayout performs *structural
analysis*, not *nominal analysis*. Two differently-named types with identical
field names, types, and layout produce identical Definition signatures. The
signature does *not* include the type's own name—by design, TypeLayout answers
"are these two types structurally equivalent?" rather than "are they the same
type?"

## 3.2 Signature Grammar

### 3.2.1 Layout Signature Grammar

The Layout signature follows a deterministic context-free grammar:

```
sig      ::= arch record
arch     ::= '[' BITS '-' ENDIAN ']'
record   ::= 'record' meta '{' fields '}'
meta     ::= '[s:' NUM ',a:' NUM ']'
           | '[s:' NUM ',a:' NUM ',vptr]'
fields   ::= ε | field (',' field)*
field    ::= '@' NUM ':' typesig
           | '@' NUM '.' NUM ':bits<' NUM ',' typesig '>'
typesig  ::= scalar | record | array | union | enum
scalar   ::= PREFIX meta
array    ::= 'array' meta '<' typesig ',' NUM '>'
           | 'bytes[s:' NUM ',a:1]'
union    ::= 'union' meta '{' fields '}'
enum     ::= 'enum' meta '<' typesig '>'
```

where `PREFIX ∈ {i8, u8, i16, u16, i32, u32, i64, u64, f32, f64, f80,
char, char8, char16, char32, wchar, bool, byte, nullptr, ptr, ref, rref,
memptr, fnptr}`.

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

### 3.2.2 Definition Signature Grammar

The Definition signature extends the Layout grammar with additional
productions for field names, inheritance, and qualified names:

```
def_record  ::= 'record' def_meta '{' def_content '}'
def_meta    ::= '[s:' NUM ',a:' NUM ']'
              | '[s:' NUM ',a:' NUM ',polymorphic]'
def_entry   ::= def_base | def_field
def_base    ::= '~base<' QNAME '>:' def_typesig
              | '~vbase<' QNAME '>:' def_typesig
def_field   ::= '@' NUM '[' NAME ']:' def_typesig
def_enum    ::= 'enum<' QNAME '>' meta '<' def_typesig '>'
QNAME       ::= IDENT ('::' IDENT)*
```

**Example.** The same `Message` struct:

Definition signature:
```
[64-le]record[s:16,a:8]{@0[id]:u32[s:4,a:4],@8[timestamp]:u64[s:8,a:8]}
```

The difference: field names `id` and `timestamp` appear in brackets.

**Inheritance example:**

```cpp
struct Base { int32_t x; };
struct Derived : Base { int32_t y; };
```

Layout signature (flattened):
```
[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}
```

Definition signature (hierarchy preserved):
```
[64-le]record[s:8,a:4]{~base<Base>:record[s:4,a:4]{@0[x]:i32[s:4,a:4]},@4[y]:i32[s:4,a:4]}
```

This distinction is essential: `Derived` and `struct Flat { int32_t x, y; }`
have identical Layout signatures (same bytes) but different Definition
signatures (different structure).

## 3.3 The Projection Relationship

The two layers are connected by a *projection*. At the semantic level, there
is a function π : StructureTree → ByteLayout that erases names and flattens
hierarchy. At the string level, this induces a correspondence:

```
⟦·⟧_L = ⟦·⟧_D composed with a string transformation that
        erases names and flattens inheritance
```

Formally: for all types *T*, ⟦*T*⟧_L is uniquely determined by *D_P*(*T*)
via π, because *L_P*(*T*) = π(*D_P*(*T*)).

This projection has a critical consequence: **Definition match implies Layout
match**, but not vice versa.

```
definition_signatures_match<T, U>() == true
    ⟹ layout_signatures_match<T, U>() == true
```

The converse does not hold. This asymmetry is by design: it allows users to
choose the appropriate level of strictness:

| Use case | Recommended layer | Rationale |
|----------|-------------------|-----------|
| Shared memory / IPC | Layout | Only byte compatibility matters |
| Plugin ABI verification | Layout | Binary-level check |
| API versioning | Definition | Detect field renames, hierarchy changes |
| ODR violation detection | Definition | Full structural comparison |
| "Not sure" | Definition | Strictly safer (catches everything Layout catches, plus more) |

## 3.4 Generation Algorithm

The signature generation is implemented as a recursive `consteval` function
that traverses the type structure via P2996 reflection. We describe the Layout
generation; Definition generation follows the same structure with additional
name and hierarchy encoding.

**Algorithm 1: Layout Signature Generation**

```
function LAYOUT_SIG(T, platform P):
    prefix ← "[" + str(P.bits) + "-" + P.endian + "]"
    return prefix + LAYOUT_RECORD(T)

function LAYOUT_RECORD(T):
    header ← "record[s:" + str(sizeof(T)) + ",a:" + str(alignof(T))
    if is_polymorphic(T): header ← header + ",vptr"
    header ← header + "]{"
    body ← FLATTEN(T, offset_adjustment=0)
    return header + body + "}"

function FLATTEN(T, adj):
    result ← ""
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

## 3.5 Supported Type Categories

TypeLayout generates signatures for all standard-layout and non-standard-
layout C++ types:

| Category | Layout signature | Definition additions |
|----------|-----------------|---------------------|
| Fixed-width integers | `i32[s:4,a:4]` | (same) |
| Floating point | `f64[s:8,a:8]` | (same) |
| Character types | `char16[s:2,a:2]` | (same) |
| Pointers | `ptr[s:8,a:8]` | (same) |
| References | `ref[s:8,a:8]` | (same) |
| Enums | `enum[s:4,a:4]<i32[...]>` | `enum<ns::Color>[s:4,a:4]<i32[...]>` |
| Arrays | `array[s:12,a:4]<i32[...],3>` | (same) |
| Byte arrays | `bytes[s:16,a:1]` | (same) |
| Structs | `record[s:N,a:M]{...}` | Field names in `[name]` |
| Inheritance | Flattened to leaf fields | `~base<Name>:record{...}` |
| Virtual inheritance | Flattened with vptr | `~vbase<Name>:record{...}` |
| Polymorphic types | `record[s:N,a:M,vptr]{...}` | `record[s:N,a:M,polymorphic]{...}` |
| Unions | `union[s:N,a:M]{...}` | Field names in `[name]` |
| Bit-fields | `@B.b:bits<W,type>` | `@B.b[name]:bits<W,type>` |

## 3.6 Public API

The entire public interface consists of four `consteval` functions:

```cpp
namespace boost::typelayout {
    // Definition layer (recommended default)
    template<class T>
    consteval auto get_definition_signature();

    template<class T, class U>
    consteval bool definition_signatures_match();

    // Layout layer (byte-only comparison)
    template<class T>
    consteval auto get_layout_signature();

    template<class T, class U>
    consteval bool layout_signatures_match();
}
```

All functions return compile-time values: `get_*` returns a `FixedString<N>`,
and `*_match` returns a `bool`. They can be used in `static_assert`,
`constexpr if`, template constraints (`requires`), and any other compile-time
context.
