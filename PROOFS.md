# Formal Accuracy Proofs for Two-Layer Signature System

This document provides rigorous mathematical proofs for the correctness of
Boost.TypeLayout's two-layer signature system, grounded in **Denotational Semantics**
and **Refinement Theory**.

**Formal Framework.** The proof methodology draws from two established traditions:

- **Denotational Semantics** (Scott-Strachey): Signature functions are treated as
  *semantic denotation functions* mapping C++ types to mathematical string objects.
  This framework naturally captures the compositionality of recursive signature
  generation and enables the Encoding Faithfulness theorem.

- **Refinement Theory** (Z/B methods, seL4-style): The two-layer relationship
  (Definition ⊑ Layout) is modeled as an *observational refinement*, where the
  Definition layer strictly refines the Layout layer.

**Document Architecture.** The proof document follows a *Layered Denotational*
structure with seven sections, each building on the previous:

| Section | Title | Role |
|---------|-------|------|
| S1 | Type Domain & Semantic Objects | WHAT we model |
| S2 | Encoding: Signatures as Strings | HOW we encode (denotation functions + grammar) |
| S3 | Encoding Properties | WHY the encoding is correct (faithfulness) |
| S4 | Safety Theorems | WHAT guarantees users get (soundness, conservativeness) |
| S5 | Refinement | HOW the two layers relate (projection, strict refinement) |
| S6 | Structural Verification | PER-CATEGORY correctness (offsets, 8 type constructors) |
| S7 | Summary & Reference | Index, classification, assumptions |

All proofs reference the implementation in
`include/boost/typelayout/detail/signature_impl.hpp` and
`include/boost/typelayout/detail/type_map.hpp`.

---

## S1 Type Domain & Semantic Objects (类型域与语义对象)

### Definition 1.1 (Type Domain)

For a platform P, the **type domain** Types_P is the set of all complete C++
types that the signature function can process. Formally:

    Types_P = { T | T is a complete C++ type } \ Excluded_P

where

    Excluded_P = { void }
               ∪ { T[] | T is any type }               -- unbounded arrays
               ∪ { R(Args...) | function types }        -- bare function types (not pointers)

The type domain is partitioned into two subsets:

    Types_P = Reflectable_P ∪ Opaque_P

where:
- **Reflectable_P**: Types whose internal structure is available via P2996 reflection
  (primitives, records, arrays, unions, enums, pointers, references, bit-fields).
- **Opaque_P**: Types registered via `TYPELAYOUT_OPAQUE_*` macros, whose internal
  structure is replaced by a user-provided annotation.

A predicate `opaque : Types_P → {true, false}` is defined as:

    opaque(T) = true   iff TypeSignature<T, Mode>::is_opaque exists and is true
    opaque(T) = false   otherwise

All theorems in this document apply to Reflectable_P unless explicitly stated.
When opaque types are involved, the Opaque Annotation Correctness axiom (below)
is required.

**Note on standard integer aliases.** The C++ standard does not require that
`int` and `int32_t` be the same type, but on most platforms they are. The
implementation uses `requires (!std::is_same_v<T, U>)` constraints to provide
specializations for `long`, `long long`, `signed char`, etc., only when they are
distinct from the corresponding fixed-width types. This deduplication mechanism
ensures that each concrete type has exactly one active TypeSignature specialization.

### Axiom 1.2 (Opaque Annotation Correctness)

For any opaque type T in Opaque_P with user-provided annotation (name, size, align):

    sizeof(T) = size ∧ alignof(T) = align     (enforced by static_assert)

Under this axiom, the signature of T encodes its sizeof and alignof correctly,
but does NOT encode its internal field structure. Consequently:
- The `decode` function for opaque signatures can recover (size, align) but NOT
  `fields_P(T)`.
- Two distinct opaque types with the same annotation produce identical signatures
  even if their internal layouts differ -- this is the user's responsibility.
- Opaque types produce the SAME signature in Layout and Definition modes.

### Definition 1.3 (Platform)

A platform P is a triple:

    P = (w, e, abi)

where w ∈ {32, 64} is the pointer width (bits), e ∈ {le, be} is the byte order,
and abi is the ABI specification identifier. The platform is encoded as an
architecture prefix:

    arch(w, le) = str(w) ++ "-le"      e.g., "64-le"
    arch(w, be) = str(w) ++ "-be"      e.g., "32-be"

### Definition 1.4 (Primitive Type Signature)

For a fixed platform P, the primitive type signature function σ : PrimitiveTypes → Σ*
is defined as:

    -- Fixed-width integers
    σ(int8_t)    = "i8[s:1,a:1]"
    σ(uint8_t)   = "u8[s:1,a:1]"
    σ(int16_t)   = "i16[s:2,a:2]"
    σ(uint16_t)  = "u16[s:2,a:2]"
    σ(int32_t)   = "i32[s:4,a:4]"
    σ(uint32_t)  = "u32[s:4,a:4]"
    σ(int64_t)   = "i64[s:8,a:8]"
    σ(uint64_t)  = "u64[s:8,a:8]"
    -- Floating point
    σ(float)     = "f32[s:4,a:4]"
    σ(double)    = "f64[s:8,a:8]"
    σ(long double) = "f80[s:" ++ str(sizeof_P(long double)) ++ ",a:" ++ str(alignof_P(long double)) ++ "]"
    -- Platform-dependent integers
    σ(long)      = "i" ++ str(8·sizeof_P(long)) ++ "[s:" ++ str(sizeof_P(long)) ++ ",a:" ++ str(alignof_P(long)) ++ "]"
    σ(unsigned long) = "u" ++ str(8·sizeof_P(unsigned long)) ++ "[...]"  -- analogous
    σ(long long) = "i64[s:8,a:8]"   -- (when distinct from int64_t)
    -- Character types
    σ(char)      = "char[s:1,a:1]"
    σ(wchar_t)   = "wchar[s:" ++ str(sizeof_P(wchar_t)) ++ ",a:" ++ str(alignof_P(wchar_t)) ++ "]"
    σ(char8_t)   = "char8[s:1,a:1]"
    σ(char16_t)  = "char16[s:2,a:2]"
    σ(char32_t)  = "char32[s:4,a:4]"
    -- Boolean and byte
    σ(bool)      = "bool[s:1,a:1]"
    σ(std::byte) = "byte[s:1,a:1]"
    σ(std::nullptr_t) = "nullptr[s:" ++ str(sizeof_P(std::nullptr_t)) ++ ",a:" ++ str(alignof_P(std::nullptr_t)) ++ "]"
    -- Pointers and references
    σ(T*)        = "ptr[s:" ++ str(w/8) ++ ",a:" ++ str(w/8) ++ "]"
    σ(T&)        = "ref[s:" ++ str(w/8) ++ ",a:" ++ str(w/8) ++ "]"
    σ(T&&)       = "rref[s:" ++ str(w/8) ++ ",a:" ++ str(w/8) ++ "]"
    σ(T C::*)    = "memptr[s:" ++ str(sizeof_P(T C::*)) ++ ",a:" ++ str(alignof_P(T C::*)) ++ "]"
    -- Function pointers (all variants produce the same signature)
    σ(R(*)(Args...))            = "fnptr[s:" ++ str(sizeof_P(...)) ++ ",a:" ++ str(alignof_P(...)) ++ "]"
    σ(R(*)(Args...) noexcept)   = σ(R(*)(Args...))      -- noexcept is not encoded
    σ(R(*)(Args..., ...))       = σ(R(*)(Args...))      -- C variadic is not encoded
    σ(R(*)(Args..., ...) noexcept) = σ(R(*)(Args...))   -- combination of both

where sizeof_P and alignof_P are the sizeof and alignof values on platform P.

Note on type aliases: When two C++ type names refer to the same underlying type
(e.g., `int8_t` and `signed char` on many platforms), the implementation uses
`requires (!std::is_same_v<...>)` constraints to ensure only one specialization
is active. Therefore σ is well-defined (no duplicate definitions for the same type).

**Property 1.4.1 (Primitive injectivity).**
For any two primitive types τ₁, τ₂:

    σ(τ₁) = σ(τ₂) ⟹ sizeof_P(τ₁) = sizeof_P(τ₂) ∧ alignof_P(τ₁) = alignof_P(τ₂) ∧ kind(τ₁) = kind(τ₂)

where kind is the type kind identifier (i/u/f/char/bool/ptr/...).

*Proof:* The signature prefix (i8/u8/f32/ptr/...) uniquely identifies the type kind,
and `[s:N,a:M]` uniquely identifies size and alignment. By Lemma 2.3.1 (grammar
unambiguity), the combination of prefix + size + alignment is injective. ∎

### Definition 1.5 (Leaf Field Sequence)

The leaf field sequence of type T on platform P is defined as:

    fields_P(T) = flatten(T, 0)

where flatten is recursively defined as:

    flatten(T, adj) =
      let bases = bases_of(T) in
      let members = nonstatic_data_members_of(T) in
      concat([
        if opaque(base_type(b))
          then [(offset_of(b) + adj, ⟦base_type(b)⟧_L)]    -- opaque base: leaf node
          else flatten(base_type(b), offset_of(b) + adj)     -- reflectable base: recurse
        | b ∈ bases
      ])
      ++
      concat([
        if is_bit_field(m)
          then [(offset_of(m).bytes + adj, offset_of(m).bits, bit_size_of(m), ⟦type(m)⟧_L)]
        else if is_class(type(m)) ∧ ¬is_union(type(m)) ∧ ¬opaque(type(m))
          then flatten(type(m), offset_of(m) + adj)          -- reflectable class: recurse
          else [(offset_of(m) + adj, ⟦type(m)⟧_L)]          -- leaf: primitive, union, enum, opaque, etc.
        | m ∈ members
      ])

Note: The `opaque(T)` predicate (Definition 1.1) short-circuits flattening for both
base classes and fields. This matches the implementation in `signature_impl.hpp` where
`has_opaque_signature<T, Mode>` causes the Layout engine to emit the opaque signature
as a leaf node instead of recursing into the type's members.

The else branch uses the full signature function ⟦·⟧_L (not just σ), because
leaf fields may be arrays, enums, unions, opaque types, or other non-class types.
Union members are NOT flattened; they are preserved as atomic signatures.
Bit-fields produce extended tuples containing bit offset and bit width.

### Definition 1.6 (Byte Layout)

For type T on platform P:

    L_P(T) = (sizeof_P(T), alignof_P(T), poly_P(T), fields_P(T))

where:
- sizeof_P(T) ∈ ℕ
- alignof_P(T) ∈ ℕ
- poly_P(T) ∈ {true, false}
- fields_P(T) ∈ (ℕ × Σ*)* — an **ordered sequence** of (offset, signature) pairs

The list is ordered — field order affects the signature. This is a sequence, not a set.

### Definition 1.7 (Structure Tree)

For type T on platform P:

    D_P(T) = (sizeof_P(T), alignof_P(T), poly_P(T), bases_P(T), named_fields_P(T))

where:
- bases_P(T) ∈ (Σ* × {base, vbase} × D_P(·))* — (qualified name, virtual/non-virtual, recursive structure)
- named_fields_P(T) ∈ (ℕ × Σ* × Σ*)* — (offset, field name, type signature)

### Definition 1.8 (CV-Qualification Erasure)

The signature functions erase const and volatile qualifiers:

    ⟦const T⟧_L = ⟦T⟧_L
    ⟦volatile T⟧_L = ⟦T⟧_L
    ⟦const volatile T⟧_L = ⟦T⟧_L

The same applies to ⟦·⟧_D. This is correct because CV qualifiers do not affect
memory layout (C++ [basic.type.qualifier]).

### Definition 1.9 (memcmp-compatibility)

Two types T, U are memcmp-compatible, written T ≅_mem U, if and only if:

    T ≅_mem U ⟺ L_P(T) = L_P(U)

That is: same size, same alignment, same polymorphism status, and identical
leaf field sequences (same count, same offsets, same types, same order).


---

## S2 Encoding: Signatures as Strings (签名编码)


### Definition 2.1 (Layout Denotation — Complete)

The Layout signature denotation function ⟦·⟧_L : Types_P → Σ* is a case-split
recursive function defined over all type constructors. It is a *semantic denotation
function* in the Scott-Strachey sense.

**Case 1: Primitive type τ ∈ PrimitiveTypes**

    ⟦τ⟧_L = σ(τ)

**Case 2: Record type (struct/class, non-polymorphic)**

    ⟦T⟧_L =
      "record[s:" ++ str(sizeof_P(T)) ++
            ",a:" ++ str(alignof_P(T)) ++
      "]{" ++ join(",", ["@" ++ str(oᵢ) ++ ":" ++ sigᵢ for (oᵢ, sigᵢ) in fields_P(T)]) ++ "}"

**Case 3: Polymorphic record type**

    ⟦T⟧_L =
      "record[s:" ++ str(sizeof_P(T)) ++
            ",a:" ++ str(alignof_P(T)) ++
            ",vptr]{" ++
      join(",", ["@" ++ str(oᵢ) ++ ":" ++ sigᵢ for (oᵢ, sigᵢ) in fields_P(T)]) ++ "}"

**Case 4: Array type T[N]**

    ⟦T[N]⟧_L =
      if is_byte_element(T)
        then "bytes[s:" ++ str(N) ++ ",a:1]"
        else "array[s:" ++ str(sizeof_P(T[N])) ++ ",a:" ++ str(alignof_P(T[N])) ++
             "]<" ++ ⟦T⟧_L ++ "," ++ str(N) ++ ">"

where `is_byte_element(T)` is true iff T is one of the 7 byte-sized types:
`char`, `signed char`, `unsigned char`, `int8_t`, `uint8_t`, `std::byte`, `char8_t`.
This normalization ensures that `char[N]`, `uint8_t[N]`, and `std::byte[N]` all
produce the same `bytes[s:N,a:1]` signature, reflecting their identical memory
representation.

**Case 5: Union type**

    ⟦T⟧_L =
      "union[s:" ++ str(sizeof_P(T)) ++ ",a:" ++ str(alignof_P(T)) ++
      "]{" ++ join(",", ["@" ++ str(offset_of(m)) ++ ":" ++ ⟦type(m)⟧_L for m in members_of(T)]) ++ "}"

**Case 6: Enum type E with underlying type U**

    ⟦E⟧_L = "enum[s:" ++ str(sizeof_P(E)) ++ ",a:" ++ str(alignof_P(E)) ++ "]<" ++ ⟦U⟧_L ++ ">"

**Case 7: Bit-field (handled within flatten, see Definition 1.3)**

    ⟦bf⟧_L = "@" ++ str(byte_off) ++ "." ++ str(bit_off) ++ ":bits<" ++ str(width) ++ "," ++ ⟦underlying⟧_L ++ ">"

**Case 8: CV-qualified type** — see Definition 1.8 (erasure).

For top-level structs, the full signature includes the architecture prefix:

    full_sig_L(T) = "[" ++ arch(P) ++ "]" ++ ⟦T⟧_L

**Implementation note (comma-prefix convention):** In the implementation, Layout fields
are accumulated using a comma-prefix strategy (each field string starts with `,`), and
the leading comma is removed by `skip_first()`. This is a purely mechanical string
transformation equivalent to `join(",", fields)` — removing one leading character from
`,f1,f2,...,fn` yields `f1,f2,...,fn`. Since the comma is not part of any field's data
(it is a delimiter), this transformation preserves all encoded information and does not
affect the faithfulness of `decode` (Theorem 3.1).

### Definition 2.2 (Definition Denotation — Complete)

The Definition signature denotation function ⟦·⟧_D : Types_P → Σ* mirrors ⟦·⟧_L
with the following differences:

**Cases 1, 4, 7, 8:** Same as ⟦·⟧_L.

**Case 2/3: Record type (struct/class)**

    ⟦T⟧_D =
      "record[s:" ++ str(sizeof_P(T)) ++
            ",a:" ++ str(alignof_P(T)) ++
            (if poly_P(T) then ",polymorphic" else "") ++
      "]{" ++ join(",", bases_part_D(T) ++ fields_part_D(T)) ++ "}"

    where bases_part_D(T) = [
        (if is_virtual(b) then "~vbase<" else "~base<") ++
        qualified_name(base_type(b)) ++ ">:" ++ ⟦base_type(b)⟧_D
        for b in bases_of(T)
      ]
    and fields_part_D(T) = [
        "@" ++ str(offset_of(m)) ++ "[" ++ name(m) ++ "]:" ++ ⟦type(m)⟧_D
        for m in nonstatic_data_members_of(T)
      ]

Key differences from ⟦·⟧_L: field names `[name]` are preserved, inheritance
hierarchy `~base<>`/`~vbase<>` is preserved (not flattened), and the polymorphic
marker is `polymorphic` instead of `vptr`.

**Case 5: Union type** — same as ⟦·⟧_L but with field names: `@OFF[name]:SIG`.

**Case 6: Enum type E**

    ⟦E⟧_D = "enum<" ++ qualified_name(E) ++ ">[s:" ++ str(sizeof_P(E)) ++
            ",a:" ++ str(alignof_P(E)) ++ "]<" ++ ⟦U⟧_D ++ ">"

### Correspondence to Implementation

Both denotation functions directly correspond to the implementation in
`signature_detail.hpp`:

| Denotation component | Implementation function | P2996 API |
|---|---|---|
| sizeof_P(T) | `sizeof(T)` | Compiler operator |
| alignof_P(T) | `alignof(T)` | Compiler operator |
| poly_P(T) | `std::is_polymorphic_v<T>` | Type trait |
| fields_P(T) offsets | `offset_of(member).bytes + OffsetAdj` | `std::meta::offset_of` |
| fields_P(T) types | `TypeSignature<FieldType, M>::calculate()` | Recursive |
| bases_of(T) | `std::meta::bases_of(^^T, ...)` | P2996 intrinsic |
| qualified_name | `qualified_name_for<>()` | `std::meta::identifier_of` + `parent_of` |


### 2.3 Layout Signature Grammar

The Layout signature follows a deterministic grammar (simplified):

    sig      ::= arch record
    arch     ::= '[' BITS '-' ENDIAN ']'
    record   ::= 'record' meta '{' fields '}'
    meta     ::= '[s:' NUM ',a:' NUM ']'
               | '[s:' NUM ',a:' NUM ',vptr]'
    fields   ::= ε | field (',' field)*
    field    ::= '@' NUM ':' typesig
    typesig  ::= scalar | record | array | union | enum | bits
    scalar   ::= ('i8'|'u8'|'i16'|...|'ptr'|'fnptr'|...) meta
    array    ::= 'array' meta '<' typesig ',' NUM '>'
               | 'bytes[s:' NUM ',a:1]'
    union    ::= 'union' meta '{' fields '}'
    enum     ::= 'enum' meta '<' typesig '>'
    bits     ::= '@' NUM '.' NUM ':bits<' NUM ',' typesig '>'

**Lemma 2.3.1 (Grammar unambiguity).** The grammar is unambiguous: each valid signature
string has exactly one parse tree.

*Proof sketch (LL(1) argument):* A grammar is LL(1) if, for every non-terminal with
multiple productions, the FIRST sets of those productions are pairwise disjoint. We
verify this for every decision point in the grammar:

| Non-terminal | Productions | FIRST sets |
|---|---|---|
| `typesig` | `scalar \| record \| array \| union \| enum \| bits` | `{i,u,f,char*,bool,byte,nullptr,ptr,ref,rref,memptr,fnptr}`, `{record}`, `{array,bytes}`, `{union}`, `{enum}`, `{@…·…}` |
| `scalar` | `i8 \| u8 \| i16 \| ... \| ptr \| fnptr \| ...` | Each prefix is a distinct string literal |
| `array` | `array... \| bytes...` | `{array}`, `{bytes}` |
| `meta` | `[s:N,a:N] \| [s:N,a:N,vptr]` | Both start with `[s:`, but `vptr` vs `]` after the second `NUM` is decided by lookahead on a fixed delimiter — resolved as LL(2) or equivalently by a single-token lookahead after parsing `a:NUM` |
| `field` | `@NUM:typesig \| @NUM.NUM:bits<...>` | Both start with `@NUM`, but `.` vs `:` at the next character distinguishes them (LL(2), or factored as `field → '@' NUM fieldtail`) |

Strictly, two productions (`meta` and `field`) require LL(2) lookahead. However, both
can be left-factored into LL(1) form:

    field    → '@' NUM fieldtail
    fieldtail → ':' typesig | '.' NUM ':bits<' NUM ',' typesig '>'
    meta     → '[s:' NUM ',a:' NUM metatail
    metatail → ']' | ',vptr]'

After left-factoring, FIRST sets at every decision point are pairwise disjoint.
Therefore the grammar is LL(1)-parsable, which implies:

1. **Deterministic parsing:** A recursive-descent parser with one-token lookahead
   uniquely determines the production at every step.
2. **Unique parse tree:** Every valid string has exactly one derivation.
3. **Injectivity on strings:** Different underlying data produces different strings,
   and equal strings can only arise from equal data.

Additional structural properties that support unambiguity:
- Delimiters `{`, `}`, `[`, `]`, `<`, `>` are balanced and nested only via recursion
  through `typesig` (no cross-production nesting)
- Numeric values (offsets, sizes) are separated by fixed punctuation (`:`, `,`, `@`)
- The `@` prefix uniquely identifies field offset entries within `{...}` blocks

### 2.4 Definition Signature Grammar

The Definition signature extends the Layout grammar (S2.3) with additional productions
for field names, inheritance hierarchy, polymorphic markers, and qualified enum names:

    def_sig    ::= arch def_record
    def_record ::= 'record' def_meta '{' def_content '}'
    def_meta   ::= '[s:' NUM ',a:' NUM ']'
                 | '[s:' NUM ',a:' NUM ',polymorphic]'
    def_content ::= ε | def_entry (',' def_entry)*
    def_entry  ::= def_base | def_field
    def_base   ::= '~base<' QNAME '>:' def_typesig
                 | '~vbase<' QNAME '>:' def_typesig
    def_field  ::= '@' NUM '[' NAME ']:'  def_typesig
                 | '@' NUM '.' NUM '[' NAME ']:bits<' NUM ',' def_typesig '>'
    def_typesig ::= scalar | def_record | def_array | def_union | def_enum | def_bits
    def_array  ::= array                           -- same as Layout
    def_union  ::= 'union' meta '{' def_fields '}' -- fields have names
    def_fields ::= ε | def_field (',' def_field)*
    def_enum   ::= 'enum<' QNAME '>' meta '<' def_typesig '>'
    QNAME      ::= IDENT ('::' IDENT)*
    NAME       ::= IDENT | '<anon:' NUM '>'

**Lemma 2.4.1 (Definition grammar unambiguity).** The Definition grammar is also
unambiguous. The proof follows the same LL(1) argument as Lemma 2.3.1, with these
additional observations:

- `def_entry` is disambiguated by its prefix: `~` for bases (then `base<` vs `vbase<`),
  `@` for fields — disjoint FIRST sets
- `def_meta` uses `polymorphic` vs `]` after `a:NUM`, same left-factoring as `meta`
- `def_enum` has prefix `enum<` followed by a qualified name, whereas Layout's `enum`
  has `enum[` — the `<` vs `[` character after `enum` uniquely distinguishes them
- `NAME` is enclosed in `[...]` brackets and `QNAME` is enclosed in `<...>` delimiters,
  both with unique surrounding punctuation that prevents ambiguity with other productions
- All other productions are identical to the Layout grammar

Therefore the Definition signature also has a unique parse tree for every valid string. ∎

---

## S3 Encoding Properties (编码性质)

### Theorem 3.1 (Encoding Faithfulness / 编码忠实性)

**Statement:** ⟦·⟧_L is a faithful encoding from L_P to Σ* — there exists a
**partial** function `decode : Σ* ⇀ Layout` such that:

    decode(⟦T⟧_L) = L_P(T)  for all complete types T in Types_P

where `decode` is defined (i.e., terminates successfully) on exactly the image
Im(⟦·⟧_L) ⊂ Σ*, and undefined on strings that do not conform to the grammar of S2.3.

**Proof:**

(1) **Existence of decode:** By Definition 2.1, the signature string is constructed by
concatenating components with unique syntax separators. By Lemma 2.3.1 (grammar
unambiguity), any string in Im(⟦·⟧_L) can be uniquely parsed back to its constituent
parts. Define `decode` as this parsing process; it is a partial function whose domain
is exactly the set of well-formed signature strings.

(2) **Correctness of decode on Im(⟦·⟧_L):**
- `decode` extracts sizeof from `s:N` → sizeof_P(T) ✓
- `decode` extracts alignof from `a:N` → alignof_P(T) ✓
- `decode` extracts poly from `,vptr` presence → poly_P(T) ✓
- `decode` extracts fields from `@OFF:SIG` list → fields_P(T) ✓

(3) **Partiality note:** `decode` is undefined on arbitrary strings (e.g.,
`"hello world"` or `"record[s:4,a:4]{@0:unknown}"`) because they do not parse under
the grammar of S2.3. This is expected: not every string is a valid signature. The
faithfulness guarantee applies only to strings actually produced by ⟦·⟧_L.

Therefore decode ∘ ⟦·⟧_L = L_P on Types_P, i.e., ⟦·⟧_L is a faithful encoding. ∎

**CompCert Parallel:** This theorem is analogous to CompCert's *semantic preservation*:
- CompCert: compile(P) behaves identically to P (compilation preserves semantics)
- TypeLayout: ⟦T⟧_L faithfully encodes L_P(T) (signature preserves layout information)

Both proofs share the same strategy: prove that the encoding/compilation is a
**faithful functor** — no information is lost in the transformation.

### Corollary 3.1.1 (Injectivity)

    L_P(T) ≠ L_P(U) ⟹ ⟦T⟧_L ≠ ⟦U⟧_L

*Proof:* If ⟦T⟧_L = ⟦U⟧_L, then L_P(T) = decode(⟦T⟧_L) = decode(⟦U⟧_L) = L_P(U),
contradiction. ∎

### Theorem 3.2 (Definition Encoding Faithfulness / 定义编码忠实性)

**Statement:** ⟦·⟧_D is a faithful encoding from D_P to Σ* — there exists a
**partial** function `decode_D : Σ* ⇀ StructureTree` such that:

    decode_D(⟦T⟧_D) = D_P(T)  for all complete types T in Types_P

**Proof:** Analogous to Theorem 3.1, using Lemma 2.4.1 (Definition grammar unambiguity)
instead of Lemma 2.3.1.

(1) **Existence:** By Definition 2.2, the Definition signature string encodes all
components of D_P(T) — sizeof, alignof, polymorphism, base classes (with qualified names
and virtual/non-virtual status), and named fields (with offsets and names). By
Lemma 2.4.1, this encoding is unambiguous: any string in Im(⟦·⟧_D) can be uniquely
parsed back to recover all components.

(2) **Correctness of decode_D on Im(⟦·⟧_D):**
- `decode_D` extracts sizeof/alignof from `s:N,a:N` → sizeof_P(T), alignof_P(T) ✓
- `decode_D` extracts poly from `,polymorphic` presence → poly_P(T) ✓
- `decode_D` extracts bases from `~base<QNAME>:SIG` / `~vbase<QNAME>:SIG` → bases_P(T) ✓
- `decode_D` extracts fields from `@OFF[name]:SIG` list → named_fields_P(T) ✓
- `decode_D` extracts enum names from `enum<QNAME>[...]` → qualified name ✓

Therefore decode_D ∘ ⟦·⟧_D = D_P on Types_P. ∎

### Corollary 3.2.1 (Definition Soundness)

    ⟦T⟧_D = ⟦U⟧_D ⟹ D_P(T) = D_P(U)

That is: if two types have matching Definition signatures, their complete structural
information (names, types, offsets, inheritance hierarchy) is identical.

*Proof:* If ⟦T⟧_D = ⟦U⟧_D, then D_P(T) = decode_D(⟦T⟧_D) = decode_D(⟦U⟧_D) = D_P(U). ∎

**Significance:** This is the V2 core guarantee. Definition signature equality implies
complete structural identity — not just byte layout, but also field names, base class
hierarchy, and qualified names.


---

## S4 Safety Theorems (安全定理)

### Theorem 4.1 (Soundness / 可靠性 — Zero False Positives)

**Statement:**

    ⟦T⟧_L = ⟦U⟧_L ⟹ T ≅_mem U

**Proof:** By the contrapositive of Corollary 3.1.1:

    ⟦T⟧_L = ⟦U⟧_L
    ⟹ L_P(T) = L_P(U)      [by faithfulness: decode ∘ ⟦·⟧_L = L_P]
    ⟹ T ≅_mem U             [by Definition 1.9]  ∎

**Significance:** This is the core safety guarantee. If two types have matching Layout
signatures, they are guaranteed to have identical byte layouts. There are **zero false
positives** under stated assumptions.

### Theorem 4.2 (Conservativeness / 保守性 — Intentional False Negatives)

To state this theorem precisely, we first distinguish two equivalence relations:

**Definition (Byte-level equivalence).** T ≅_byte U iff for every instance of T and U
with the same field values, their raw memory representations are identical byte-for-byte.
This is strictly weaker than ≅_mem: it ignores type-kind information (array vs record)
and considers only raw byte content.

**Relationship:** ≅_mem (= L_P equality) is strictly finer than ≅_byte, because L_P
encodes type-kind information (record vs array vs union vs enum). Two types may have
identical bytes but different L_P values.

**Statement:** ∃ T, U such that T ≅_byte U but ⟦T⟧_L ≠ ⟦U⟧_L.

**Proof (Counterexample):**
```cpp
struct A { int32_t x, y, z; };  // record[s:12,a:4]{@0:i32[...],@4:i32[...],@8:i32[...]}
using  B = int32_t[3];          // array[s:12,a:4]<i32[s:4,a:4],3>
```

A and B have identical byte-level layouts: 12 bytes, three contiguous int32_t values
at offsets 0, 4, 8. They are byte-equivalent: A ≅_byte B.

However, their signatures differ:
- ⟦A⟧_L = `record[s:12,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4],@8:i32[s:4,a:4]}`
- ⟦B⟧_L = `array[s:12,a:4]<i32[s:4,a:4],3>`

Note: L_P(A) ≠ L_P(B) either (A is a record with 3 fields, B is an array), so
A ≇_mem B. This is correct — the signature function ⟦·⟧_L is a faithful encoding
of L_P (Theorem 3.1), and L_P itself is type-kind-aware.

**Formal characterization:**

    ker(⟦·⟧_L) = ker(L_P) ⊂ ≅_byte

The equivalence kernel of ⟦·⟧_L exactly coincides with L_P equality (by Theorem 3.1),
and is strictly smaller than pure byte equivalence. The conservativeness is with
respect to the weaker ≅_byte relation. ∎

**Design rationale:** This is an intentional safety choice. The system prefers false
negatives (rejecting byte-compatible but structurally different types) over false
positives, because structural differences (array vs struct) often indicate
semantic incompatibility even when bytes happen to align.


---

## S5 Refinement: Definition > Layout (精化关系)


### Definition 5.1 (Reflection Data)

For any type T on platform P, define the **P2996 reflection data** R_P(T) as the
complete set of information available via P2996 intrinsics:

    R_P(T) = {
      sizeof_P(T), alignof_P(T), poly_P(T),
      [offset_of(m), type_of(m), is_bit_field(m), bit_size_of(m) | m ∈ members],
      [offset_of(b), type_of(b), is_virtual(b) | b ∈ bases],
      [identifier_of(m) | m ∈ members],        -- names (Definition-only)
      [qualified_name(base_type(b)) | b ∈ bases] -- qualified names (Definition-only)
    }

Both ⟦T⟧_L and ⟦T⟧_D are deterministic functions of R_P(T). The Layout denotation
uses a **strict subset** of R_P(T) — it discards names, qualified names, and
inheritance structure. The Definition denotation uses all of R_P(T).

### Lemma 5.2 (Information Ordering)

Define the layout-relevant projection of R_P(T) as:

    R_P^layout(T) = {
      sizeof_P(T), alignof_P(T), poly_P(T),
      fields_P(T)  -- the flattened leaf field sequence (Definition 1.5)
    }

Then:
- ⟦T⟧_L is a deterministic function of R_P^layout(T) only
- ⟦T⟧_D is a deterministic function of the full R_P(T)
- R_P^layout(T) is recoverable from R_P(T) (projection is information-losing)

*Proof:* By inspection of Definition 2.1 (⟦·⟧_L uses only sizeof, alignof, poly,
and fields_P) and Definition 2.2 (⟦·⟧_D additionally uses names, bases, qualified names). ∎

### Lemma 5.3 (Definition determines Layout)

    R_P(T) determines ⟦T⟧_L uniquely.

*Proof:* ⟦T⟧_L = f(R_P^layout(T)) and R_P^layout(T) = g(R_P(T)) for deterministic
functions f, g. Therefore ⟦T⟧_L = f(g(R_P(T))). ∎

### Theorem 5.4 (V3 Projection / 投影定理)

**Statement:**

    ⟦T⟧_D = ⟦U⟧_D ⟹ ⟦T⟧_L = ⟦U⟧_L

**Proof:** By Encoding Faithfulness (Theorem 3.1) applied to ⟦·⟧_D:

Since ⟦·⟧_D is also a faithful encoding (Theorem 3.2, by Lemma 2.4.1),
⟦T⟧_D = ⟦U⟧_D implies R_P(T) and R_P(U) agree on all
information encoded by ⟦·⟧_D. In particular, they agree on all layout-relevant
information: sizeof, alignof, poly, and all field offsets and types.

Therefore R_P^layout(T) = R_P^layout(U), and by Lemma 5.3:
⟦T⟧_L = f(R_P^layout(T)) = f(R_P^layout(U)) = ⟦U⟧_L. ∎

**Note:** This proof does not require constructing a syntactic erasure function
π : Σ* → Σ*. The key insight is that both denotations are derived from the same
reflection data R_P(T), with ⟦·⟧_L using strictly less information than ⟦·⟧_D.

**Conceptual erasure (informational, not a formal proof step):**
The relationship can be visualized as a conceptual erasure with four steps:
1. Remove field names: `@OFF[name]:SIG` → `@OFF:SIG`
2. Flatten inheritance: `~base<N>:record{F}` → flattened fields
3. Replace poly marker: `polymorphic` → `vptr`
4. Remove enum qualified names: `enum<ns::C>[...]` → `enum[...]`

However, step 2 (flatten_inheritance) requires base-in-derived offset information
that is not directly encoded in the Definition signature string. Therefore the
erasure is not a pure string-to-string function, and the formal proof uses the
semantic argument above instead.

### Theorem 5.5 (Strict Refinement / 严格精化)

**Statement:** ker(⟦·⟧_D) ⊊ ker(⟦·⟧_L)

*Proof:*
- ker(⟦·⟧_D) ⊆ ker(⟦·⟧_L): by Theorem 5.4
- ker(⟦·⟧_D) ≠ ker(⟦·⟧_L): counterexample —

```cpp
struct Base { int32_t id; };
struct Derived : Base { int32_t value; };
struct Flat { int32_t id; int32_t value; };
```

- ⟦Derived⟧_L = ⟦Flat⟧_L (inheritance flattened → identical byte layout)
- ⟦Derived⟧_D ≠ ⟦Flat⟧_D (Derived has `~base<Base>:record{...}`, Flat does not)

Therefore (Derived, Flat) ∈ ker(⟦·⟧_L) \ ker(⟦·⟧_D), proving strict inclusion. ∎

**Commentary:** This is *observational refinement* from refinement theory — ⟦·⟧_D's
observable distinctions (everything it can tell apart) are strictly more than ⟦·⟧_L's.
The Definition layer is a *strict refinement* of the Layout layer.


---

## S6 Structural Verification (结构验证)

### Theorem 6.1 (Offset Correctness / 偏移正确性)

**Statement:** For nested/inheriting types, all offsets in ⟦·⟧_L equal the
compiler-reported absolute offsets.

**Proof by structural induction on the type AST:**

*Base case:* Primitive type τ. No offset concept; signature is directly σ(τ). ✓

*Inductive case (composition):* `struct T { Inner x; ... }`

    offset_in_sig(@x.field_j) = offset_of(x) + offset_within_Inner(field_j)

By implementation: `field_offset = offset_of(member).bytes + OffsetAdj`

- `offset_of(member).bytes` — provided by P2996 compiler intrinsic (axiomatic trust)
- `OffsetAdj` — cumulative ancestor offset (correct by inductive hypothesis)
- Sum = absolute offset ✓

*Inductive case (inheritance):* `struct Derived : Base { ... }`

    offset_in_sig(@base.field_j) = offset_of(Base in Derived) + offset_within_Base(field_j)

Same argument; `offset_of(base_info).bytes` provided by P2996. ✓

By structural induction, all offsets are correct. ∎

**seL4 Parallel:** This proof is analogous to seL4's *refinement chain* — each layer's
offsets are correctly propagated from the layer above, just as seL4's security properties
are preserved through each refinement step.

**Note on vptr:** For polymorphic types, the vptr occupies `sizeof(void*)` bytes at an
implementation-defined position. It is NOT a `nonstatic_data_member`, so it does not
appear as a `@OFF:SIG` entry. However, its space IS included in `sizeof(T)` (reflected
in `s:SIZE`), and the `,vptr` marker records its existence. Two polymorphic types with
matching signatures have the same sizeof (including vptr space) and the same field offsets.

---


### 6.1.1 Induction Principle

C++ types are inductively defined by the following grammar:

    Type ::= Primitive                    (base types)
           | Record(fields)               (struct/class)
           | Record(bases, fields)        (inheritance)
           | PolyRecord(bases, fields)    (polymorphic)
           | Array(Type, N)               (arrays)
           | Union(fields)                (unions)
           | Enum(underlying)             (enumerations)
           | BitField(Type, width)        (bit-fields)

For each Type constructor, we verify the correctness of ⟦·⟧_L and ⟦·⟧_D.

### 6.2 Primitive

    ⟦τ⟧_L = σ(τ)    — by Definition 2.1
    ⟦τ⟧_D = σ(τ)    — same for primitives (no extra info)

Correctness by Definition 1.4 (sigma definition) and C++ standard type size guarantees. ✓

| Type | Signature | Correctness |
|---|---|---|
| `int8_t` / `uint8_t` | `i8`/`u8[s:1,a:1]` | Fixed-width, guaranteed by standard |
| `int16_t` / `uint16_t` | `i16`/`u16[s:2,a:2]` | Fixed-width, guaranteed by standard |
| `int32_t` / `uint32_t` | `i32`/`u32[s:4,a:4]` | Fixed-width, guaranteed by standard |
| `int64_t` / `uint64_t` | `i64`/`u64[s:8,a:8]` | Fixed-width, guaranteed by standard |
| `float` | `f32[s:4,a:4]` | IEEE 754 guarantees 4-byte size |
| `double` | `f64[s:8,a:8]` | IEEE 754 guarantees 8-byte size |
| `long double` | `f80[s:N,a:N]` | Uses actual sizeof/alignof |
| `long` / `unsigned long` | `i32`/`u32` or `i64`/`u64` | Uses actual `sizeof(long)` |
| `char` | `char[s:1,a:1]` | sizeof(char)==1 guaranteed by standard |
| `wchar_t` | `wchar[s:N,a:N]` | Uses actual sizeof/alignof |
| `char8_t` | `char8[s:1,a:1]` | C++20 fixed-width |
| `char16_t` / `char32_t` | `char16`/`char32` | Fixed-width, guaranteed by standard |
| `bool` | `bool[s:1,a:1]` | sizeof(bool)==1 on all known platforms |
| `std::byte` | `byte[s:1,a:1]` | Same representation as unsigned char |
| `std::nullptr_t` | `nullptr[s:N,a:N]` | Uses actual sizeof/alignof |
| `T*` | `ptr[s:N,a:N]` | Uses actual sizeof/alignof |
| `T&` / `T&&` | `ref`/`rref[s:N,a:N]` | Same physical representation as pointer |
| `T C::*` | `memptr[s:N,a:N]` | Uses actual sizeof/alignof |
| `R(*)(Args...)` | `fnptr[s:N,a:N]` | Uses actual sizeof/alignof |

All primitive types follow the same `kind[s:SIZE,a:ALIGN]` pattern where `kind` is
a unique prefix (i/u/f/char/bool/byte/ptr/ref/rref/memptr/fnptr/nullptr). This ensures
Property 1.4.1 (primitive injectivity) holds across the complete set.

### 6.3 Record(fields) — struct/class

Correctness by Theorem 6.1 (offset correctness), with induction on field count n
and nesting depth d. ✓

### 6.4 Record(bases, fields) — inheritance

Layout: Flattened. Correctness by Theorem 6.1's inheritance induction case.
Definition: Preserves `~base<>` / `~vbase<>` structure. Correctness by `bases_of`
and `qualified_name_for` P2996 APIs. ✓

### 6.5 PolyRecord — polymorphic

Signature includes `,vptr` (Layout) or `,polymorphic` (Definition) marker.
vptr space is included in sizeof but NOT as a leaf field — this is correct because
vptr is not a `nonstatic_data_member`, but the compiler accounts for it in sizeof
and all offset_of calculations. ✓

### 6.6 Array(Type, N)

    ⟦T[N]⟧_L = "array[s:" ++ str(sizeof(T[N])) ++ ",a:" ++ str(alignof(T[N])) ++ "]<" ++ ⟦T⟧_L ++ "," ++ str(N) ++ ">"

Special case: Byte arrays normalized to `bytes[s:N,a:1]`.
Normalization correctness: `char[N]` ≡ `uint8_t[N]` ≡ `std::byte[N]` in memory. ✓

### 6.7 Union(fields)

Members NOT flattened; preserved as atomic signatures.
Correctness: Union members share offset @0; flattening would create ambiguity. ✓

### 6.8 BitField(Type, width)

    ⟦bf⟧ = "@BYTE.BIT:bits<WIDTH," ++ σ(underlying) ++ ">"

All data from P2996 API (`offset_of.bytes`, `offset_of.bits`, `bit_size_of`).
**Known limitation:** Bit ordering across compilers is implementation-defined. ✓ (with caveat)

### 6.9 Enum(underlying)

    ⟦E⟧_L = "enum[s:S,a:A]<" ++ σ(underlying) ++ ">"
    ⟦E⟧_D = "enum<" ++ qualified_name(E) ++ ">[s:S,a:A]<" ++ σ(underlying) ++ ">"

Underlying type from `std::underlying_type_t<E>`. ✓


---

## S7 Summary & Reference (总结与索引)


```
                   ┌────────────────────────┐
                   │  S1 Type Domain &      │
                   │     Semantic Objects    │
                   │  1.1 Types_P           │
                   │  1.3 Platform          │
                   │  1.4 sigma (prims)     │
                   │  1.5 flatten           │
                   │  1.6 L_P (layout)      │
                   │  1.7 D_P (struct tree) │
                   │  1.8 CV-erasure        │
                   │  1.9 memcmp-compat     │
                   └───────────┬────────────┘
                               │
                   ┌───────────▼────────────┐
                   │  S2 Encoding           │
                   │  2.1 [.]_L denotation  │
                   │  2.2 [.]_D denotation  │
                   │  2.3 L-Grammar         │
                   │  2.4 D-Grammar         │
                   └───────────┬────────────┘
                               │
            ┌──────────────────┼──────────────────┐
            │                  │                  │
   ┌────────▼─────────┐ ┌─────▼──────┐ ┌────────▼──────────┐
   │  S3 Encoding     │ │ S4 Safety  │ │ S5 Refinement     │
   │  Properties      │ │ Theorems   │ │  5.1 R_P          │
   │  3.1 L-Faithful  │ │ 4.1 Sound  │ │  5.2 Info Order   │
   │  3.1.1 Inject    │ │ 4.2 Cons.  │ │  5.4 V3 Project   │
   │  3.2 D-Faithful  │ └────────────┘ │  5.5 Strict Ref   │
   │  3.2.1 D-Sound   │               └───────────────────┘
   └──────────────────┘
                               │
                   ┌───────────▼────────────┐
                   │  S6 Structural         │
                   │  Verification          │
                   │  6.1 Offset Correct    │
                   │  6.2-6.9 Per-Category  │
                   └────────────────────────┘
```

### 7.1 Theorem Index

| # | Theorem | Statement | Status |
|---|---------|-----------|--------|
| 3.1 | Encoding Faithfulness | ∃ decode: decode ∘ ⟦·⟧_L = L_P | ✅ Proven |
| 3.1.1 | Injectivity | L_P(T) ≠ L_P(U) ⟹ ⟦T⟧_L ≠ ⟦U⟧_L | ✅ Proven (corollary) |
| 4.1 | Soundness | ⟦T⟧_L = ⟦U⟧_L ⟹ T ≅_mem U | ✅ Proven (contrapositive) |
| 4.2 | Conservativeness | ∃ T ≅_byte U with ⟦T⟧_L ≠ ⟦U⟧_L | ✅ Proven (counterexample) |
| 6.1 | Offset Correctness | All offsets = compiler absolute offsets | ✅ Proven (structural induction) |
| 3.2 | Definition Faithfulness | ∃ decode_D: decode_D ∘ ⟦·⟧_D = D_P | ✅ Proven |
| 3.2.1 | Definition Soundness | ⟦T⟧_D = ⟦U⟧_D ⟹ D_P(T) = D_P(U) | ✅ Proven (corollary) |
| 5.4 | V3 Projection | ⟦T⟧_D = ⟦U⟧_D ⟹ ⟦T⟧_L = ⟦U⟧_L | ✅ Proven (semantic R_P argument) |
| 5.5 | Strict Refinement | ker(⟦·⟧_D) ⊊ ker(⟦·⟧_L) | ✅ Proven (counterexample) |

### 7.2 Accuracy Classification

| Type Category | Layout Accuracy | Definition Accuracy | Known Limitation |
|---|---|---|---|
| Fixed-width scalars | Exact | Exact | None |
| Platform-dependent scalars | Exact (actual sizeof) | Exact | Cross-platform sigs naturally differ |
| POD structs | Exact | Exact | None |
| Inheritance | Exact (flattened) | Exact (tree preserved) | None |
| Polymorphic | Existence marker | Existence marker | vptr offset not encoded |
| Arrays | Exact | Exact | Byte-array normalization |
| Unions | Exact (not flattened) | Exact | None |
| Bit-fields | Same-compiler exact | Same-compiler exact | Cross-compiler bit ordering impl-defined |
| Enums | Underlying type exact | Underlying + qualified name | None |

### 7.3 Formal Guarantees

The TypeLayout signature system provides the following formally proven guarantees:

1. **Layout Encoding Faithfulness:** The signature function ⟦·⟧_L has an inverse `decode`,
   meaning no layout information is lost during encoding. (Theorem 3.1)

2. **Definition Encoding Faithfulness:** The signature function ⟦·⟧_D has an inverse
   `decode_D`, meaning no structural information (names, hierarchy, qualified names)
   is lost during encoding. (Theorem 3.2)

3. **Definition Soundness:** If two types have matching Definition signatures, their
   complete structural information is identical. (Corollary 3.2.1)

4. **Soundness (zero false positives):** If two types have matching Layout signatures,
   their memory layouts are guaranteed to be identical. (Theorem 4.1)

5. **Conservativeness (safe direction):** Signature mismatch does not necessarily mean
   layout incompatibility — the system may report differences for types that are
   byte-identical but semantically distinct. (Theorem 4.2)

6. **V3 Projection:** Definition match strictly implies Layout match, via a
   semantic argument on shared reflection data R_P. (Theorem 5.4)

7. **Strict Refinement:** The Definition layer is a strict refinement of the Layout
   layer: ker(⟦·⟧_D) ⊊ ker(⟦·⟧_L). (Theorem 5.5)

8. **Compiler-verified:** All offsets and sizes come from P2996 compiler intrinsics
   (`std::meta::offset_of`, `sizeof`, `alignof`), not manual calculation. (Theorem 6.1)

9. **Platform-aware:** The architecture prefix encodes pointer width and endianness,
   preventing cross-platform false positives.

### 7.4 Formal Methodology

The proof system is grounded in two established verification traditions:

| Method | Applied to | Reference |
|--------|-----------|-----------|
| **Denotational Semantics** | Signature functions as denotations, Encoding Faithfulness | Scott-Strachey framework |
| **Refinement Theory** | V3 Projection, Strict Refinement (Definition ⊑ Layout) | Z/B methods, seL4 refinement |
| **Structural Induction** | Per-category verification (8 type constructors) | Standard mathematical induction |

Key insight: The signature function ⟦·⟧_L is a *faithful functor* from the type domain
to the string domain — it preserves all layout-relevant structure while being amenable
to string equality comparison. This is directly analogous to CompCert's semantic
preservation theorem, where compilation preserves program behavior.

### 7.5 Assumptions

These proofs assume:

**Platform assumptions:**
- IEEE 754 floating-point representation on all compared platforms
- Same endianness between compared types (encoded in architecture prefix)
- Same pointer width between compared types (encoded in architecture prefix)

**Type system assumptions:**
- All types passed to the signature function are **complete types** (i.e., their
  definitions are fully visible at the point of signature computation). Incomplete
  types (forward declarations) are not supported and would cause a compilation error
  in the P2996 reflection API
- **Anonymous enums** are not supported by ⟦·⟧_D (the Definition layer requires
  `qualified_name(E)`, which is ill-defined for anonymous enums). The Layout layer
  ⟦·⟧_L handles them correctly since it only uses the underlying type. The
  implementation guards against this with a `requires` clause or static_assert
- **`[[no_unique_address]]`**: Members annotated with `[[no_unique_address]]` may have
  `offset_of` values that overlap with other members (the compiler is permitted to
  reuse tail padding). The signature function faithfully records whatever `offset_of`
  the compiler reports. Two types with the same `[[no_unique_address]]` layout will
  have matching signatures; however, comparing a type with `[[no_unique_address]]` to
  a "manually flattened" equivalent may yield different signatures if the compiler
  chose different offset assignments

**Semantic assumptions:**
- Padding bytes are not semantically significant (their positions are implicitly
  encoded by offset gaps between fields)
- P2996 compiler intrinsics (`offset_of`, `sizeof`, `alignof`) return correct values
  (axiomatic trust in the compiler)
- `std::meta::bases_of` and `std::meta::nonstatic_data_members_of` return members
  in declaration order (required for signature determinism)
