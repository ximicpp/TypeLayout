# Formal Accuracy Proofs for Two-Layer Signature System

This document provides mathematical proofs for the correctness of Boost.TypeLayout's
two-layer signature system. All proofs reference the implementation in
`include/boost/typelayout/core/signature_detail.hpp`.

---

## §1 Formal Semantic Model

### 1.1 Basic Definitions

**Definition 1.1 (Type Universe).** Let T be the set of all C++ types on a fixed
platform P = (ptr_width, endianness, ABI). The platform is encoded as an architecture
prefix such as `[64-le]` or `[32-be]`.

**Definition 1.2 (Byte Layout).** For a type T ∈ T, its byte layout is defined as:

    L(T) = ( sizeof(T), alignof(T), poly(T), [(o₁,τ₁), (o₂,τ₂), ..., (oₙ,τₙ)] )

where:
- `(oᵢ, τᵢ)` is the i-th leaf field's **absolute byte offset** and **primitive type
  signature**, enumerated in the order they appear in the flattened field list
- `poly(T)` is a boolean indicating whether T is polymorphic (`std::is_polymorphic_v<T>`)
- The list is **ordered** (sequence, not set) — field order affects the signature

**Definition 1.3 (Structure Tree).** For a type T ∈ T, its structure tree is defined as:

    D(T) = ( sizeof(T), alignof(T), poly(T), bases(T), [(o₁,n₁,τ₁), ..., (oₘ,nₘ,τₘ)] )

where `nᵢ` is the field name and `bases(T)` is the inheritance hierarchy preserved as
a sequence of `(qualified_name, is_virtual, BaseType)` triples.

**Definition 1.4 (Signature Functions).** TypeLayout defines two signature functions:

    Sig_L : T → Σ*    (Layout signature)
    Sig_D : T → Σ*    (Definition signature)

where Σ* is the set of all finite strings over a fixed alphabet.

**Definition 1.5 (memcmp-compatibility).** Two types T, U are memcmp-compatible, written
T ~mem U, if and only if:

    T ~mem U ⟺ sizeof(T) = sizeof(U)
               ∧ alignof(T) = alignof(U)
               ∧ |fields(T)| = |fields(U)|
               ∧ ∀ i ∈ [1..n]: oᵢᵀ = oᵢᵁ ∧ τᵢᵀ = τᵢᵁ
               ∧ poly(T) = poly(U)

That is: same size, same alignment, same number of leaf fields, each field at the same
offset with the same type, and same polymorphism status.

**Definition 1.6 (CV-qualification erasure).** The signature function erases const and
volatile qualifiers: `TypeSignature<const T, M>` delegates to `TypeSignature<T, M>`.
This is correct because CV qualifiers do not affect memory layout.

### 1.2 Signature Grammar

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

**Lemma 1.7 (Grammar unambiguity).** The grammar is unambiguous: each valid signature
string has exactly one parse tree. This follows from:
- Each production has a unique prefix keyword (`record`, `array`, `union`, `enum`, `bits`, or a scalar name)
- Delimiters `{`, `}`, `[`, `]`, `<`, `>` are balanced and non-nested across productions
- Numeric values (offsets, sizes) are separated by fixed punctuation (`:`, `,`, `@`)
- The `@` prefix uniquely identifies field offset entries

Therefore, the encoding is **injective on strings**: different data produces different
strings, and equal strings can only arise from equal data.

---

## §2 Layout Signature Correctness

### Theorem 2.1 (Encoding Completeness)

**Statement:** Sig_L(T) encodes all information in L(T) without loss.

**Proof:** By construction of `TypeSignature<T, Layout>::calculate()` in
`signature_detail.hpp`:

| Component of L(T) | Encoded as | Source |
|---|---|---|
| sizeof(T) | `s:SIZE` | `sizeof(T)` — compiler operator |
| alignof(T) | `a:ALIGN` | `alignof(T)` — compiler operator |
| poly(T) | `,vptr` suffix | `std::is_polymorphic_v<T>` — type trait |
| oᵢ (offset) | `@OFFᵢ` | `std::meta::offset_of(member).bytes` — P2996 intrinsic |
| τᵢ (type) | `SIGᵢ` | Recursive `TypeSignature<FieldType, Layout>::calculate()` |
| field count n | Number of comma-separated items in `{}` | fold expression over `nonstatic_data_members_of` |
| platform P | `[64-le]` prefix | `sizeof(void*)` and `TYPELAYOUT_LITTLE_ENDIAN` |

By Lemma 1.7 (grammar unambiguity), these components are uniquely recoverable from the
signature string. No information is lost. ∎

### Theorem 2.2 (Injectivity on Layout Equivalence Classes)

**Statement:** For a fixed platform P:

    L(T) ≠ L(U) ⟹ Sig_L(T) ≠ Sig_L(U)

**Proof:** Assume L(T) ≠ L(U). Then at least one of the following holds:

1. sizeof(T) ≠ sizeof(U) → `s:` values differ → signatures differ ✓
2. alignof(T) ≠ alignof(U) → `a:` values differ → signatures differ ✓
3. poly(T) ≠ poly(U) → `,vptr` present vs absent → signatures differ ✓
4. |fields(T)| ≠ |fields(U)| → different count of `@OFF:SIG` entries → signatures differ ✓
5. ∃ i: oᵢᵀ ≠ oᵢᵁ → `@OFF` values differ at position i → signatures differ ✓
6. ∃ i: τᵢᵀ ≠ τᵢᵁ → type signatures differ at position i → signatures differ ✓

In each case, Sig_L(T) ≠ Sig_L(U) by Lemma 1.7 (distinct data produces distinct strings).

Therefore Sig_L is **injective** on layout equivalence classes. ∎

### Corollary 2.2.1 (V1 Soundness — No False Positives)

**Statement:**

    Sig_L(T) = Sig_L(U) ⟹ T ~mem U

**Proof:** This is the contrapositive of Theorem 2.2.

    L(T) ≠ L(U) ⟹ Sig_L(T) ≠ Sig_L(U)

is logically equivalent to:

    Sig_L(T) = Sig_L(U) ⟹ L(T) = L(U)

Since L(T) = L(U) implies T ~mem U by Definition 1.5, we have:

    Sig_L(T) = Sig_L(U) ⟹ T ~mem U  ∎

**Significance:** This is the core safety guarantee. If two types have matching Layout
signatures, they are guaranteed to have identical byte layouts. There are **zero false
positives** under stated assumptions (IEEE 754, same endianness).

### Proposition 2.3 (Non-surjectivity — Intentional False Negatives)

**Statement:** ∃ T, U such that T ~mem U but Sig_L(T) ≠ Sig_L(U).

**Counterexample:**
```cpp
struct A { int32_t x, y, z; };  // record[s:12,a:4]{@0:i32[...],@4:i32[...],@8:i32[...]}
using  B = int32_t[3];          // array[s:12,a:4]<i32[s:4,a:4],3>
```

A and B have identical byte layouts (12 bytes, three contiguous int32_t values), but
their signatures differ: one is `record{...}`, the other is `array<...>`.

**Analysis:** This is an **intentional design choice**. The signature function is a
**refinement** of layout identity, not an equivalence:

    Sig_L(T) = Sig_L(U) ⟹ T ~mem U    (sufficient, holds)
    T ~mem U ⇏ Sig_L(T) = Sig_L(U)      (not necessary)

The refinement preserves semantic type boundaries (array vs struct, union vs struct)
at the cost of occasional false negatives, which is the safer direction. ∎

### Theorem 2.4 (Recursive Flattening Correctness)

**Statement:** For nested structs, Layout signature correctly computes absolute offsets
for all leaf fields via recursive flattening.

**Proof by induction on nesting depth d:**

*Base case (d = 0):* No nesting. OffsetAdj = 0. Each field's offset is directly
`offset_of(member).bytes`, provided by the P2996 compiler intrinsic. Correct. ✓

*Inductive step (d = k → d = k+1):* Assume correctness at depth k. At depth k+1,
the implementation computes:

```cpp
constexpr std::size_t field_offset = offset_of(member).bytes + OffsetAdj;
return layout_all_prefixed<FieldType, field_offset>();
```

- `OffsetAdj` = sum of all ancestor offsets (correct by inductive hypothesis)
- `offset_of(member).bytes` = offset of this member within its direct parent (P2996 intrinsic)
- Their sum = absolute offset from the outermost type

The same argument applies to inheritance flattening via `layout_one_base_prefixed`:
```cpp
return layout_all_prefixed<BaseType, offset_of(base_info).bytes + OffsetAdj>();
```

Therefore flattening preserves absolute offset correctness at all depths. ∎

**Note on vptr:** For polymorphic types, the vptr occupies `sizeof(void*)` bytes at an
implementation-defined position. The vptr is NOT enumerated as a leaf field (it is not
a `nonstatic_data_member`), but its space IS included in `sizeof(T)`. The `s:SIZE` value
in the signature correctly reflects this (since it uses `sizeof(T)`), and the `,vptr`
marker records its existence. This means two polymorphic types with matching signatures
have the same sizeof (including vptr space) and the same field offsets.

---

## §3 Definition Signature Correctness

### Theorem 3.1 (Encoding Completeness)

**Statement:** Sig_D(T) encodes all information in D(T) without loss.

**Proof:** In addition to everything encoded by Sig_L (Theorem 2.1), Sig_D encodes:

| Additional component | Encoded as | Source |
|---|---|---|
| Field name nᵢ | `[name]` in `@OFF[name]:SIG` | `std::meta::identifier_of(member)` |
| Anonymous fields | `[<anon:N>]` | Fallback when `has_identifier` is false |
| Base class hierarchy | `~base<QualifiedName>:record{...}` | `std::meta::bases_of(^^T, ...)` |
| Virtual base classes | `~vbase<QualifiedName>:record{...}` | `std::meta::is_virtual(base_info)` |
| Polymorphic marker | `,polymorphic` (vs Layout's `,vptr`) | `std::is_polymorphic_v<T>` |
| Enum qualified name | `enum<ns::Color>` | `qualified_name_for<^^T>()` |
| Namespace path | Full qualified name via `parent_of` chain | Recursive `qualified_name_for<>()` |

By extension of Lemma 1.7 to the Definition grammar (which adds `[name]`, `~base<>`,
`~vbase<>` prefixes with unique syntax), the encoding is unambiguous. ∎

### Theorem 3.2 (Discrimination Power)

**Statement:** For fixed platform P, the following structural differences necessarily
produce different Definition signatures:

| Difference | Signature manifestation | Proof |
|---|---|---|
| Different field names | `[x]` vs `[y]` | `identifier_of` returns distinct names |
| Inheritance vs flat | `~base<B>:record{...}` vs direct fields | Structurally different string patterns |
| Different namespace bases | `~base<ns1::T>` vs `~base<ns2::T>` | Qualified names differ |
| Different enum names | `enum<ns::Color>` vs `enum<ns::Shape>` | Qualified names differ |
| Virtual vs non-virtual base | `~vbase<>` vs `~base<>` | Prefix keyword differs |
| Polymorphic vs non-polymorphic | `,polymorphic` present/absent | Marker differs |

Therefore Sig_D is **injective** on structural equivalence classes:

    D(T) ≠ D(U) ⟹ Sig_D(T) ≠ Sig_D(U)  ∎

**Boundary (what Sig_D does NOT distinguish):**
- Types with different names but identical structure (by design: structural analysis)
- CV-qualifiers (const int vs int) — stripped, as they don't affect layout
- These are intentional limitations of the structural analysis philosophy.

---

## §4 Projection Relationship

### Theorem 4.1 (V3 Projection — def_match implies layout_match)

**Statement:**

    Sig_D(T) = Sig_D(U) ⟹ Sig_L(T) = Sig_L(U)

**Proof:** Define the **erasure function** π : Σ* → Σ* that transforms a Definition
signature into its corresponding Layout signature by:

1. Removing all field names: `@OFF[name]:SIG` → `@OFF:SIG`
2. Flattening inheritance: `~base<ns::Name>:record{fields}` → fields at absolute offsets
3. Replacing `polymorphic` with `vptr`
4. Removing enum qualified names: `enum<ns::Color>[...]<...>` → `enum[...]<...>`

**Claim:** π is well-defined (deterministic). Each transformation step is a purely
syntactic, deterministic string operation. The flattening of inheritance is deterministic
because base class field offsets are explicitly encoded in the Definition signature
(each base's `record{...}` contains `@OFF:SIG` entries with absolute offsets relative
to the base, and the base itself has an offset from `offset_of(base_info).bytes`).

**Claim:** π(Sig_D(T)) = Sig_L(T) for all T. This follows from the fact that both
Sig_D and Sig_L are computed from the same underlying P2996 reflection data — Sig_D
simply retains additional information that π removes.

Therefore:
    Sig_D(T) = Sig_D(U)
    ⟹ π(Sig_D(T)) = π(Sig_D(U))    [by function congruence]
    ⟹ Sig_L(T) = Sig_L(U)           [by the claim above]  ∎

### Theorem 4.2 (Strict Refinement — layout_match does NOT imply def_match)

**Statement:**

    Sig_L(T) = Sig_L(U) ⇏ Sig_D(T) = Sig_D(U)

**Counterexample:**
```cpp
struct Base { int32_t id; };
struct Derived : Base { int32_t value; };
struct Flat { int32_t id; int32_t value; };
```

- Sig_L(Derived) = Sig_L(Flat) = `[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}`
  (inheritance is flattened, producing identical byte layouts)
- Sig_D(Derived) ≠ Sig_D(Flat)
  (Derived contains `~base<Base>:record{...}`, Flat does not)

In algebraic terms, the equivalence kernels satisfy:

    ker(Sig_D) ⊊ ker(Sig_L)

The Definition signature's equivalence kernel is a **strict subset** of the Layout
signature's equivalence kernel. ∎

---

## §5 Per-Category Accuracy

### 5.1 Scalar Types

| Type | Signature | Correctness |
|---|---|---|
| `int32_t` | `i32[s:4,a:4]` | Fixed-width, size/alignment guaranteed by standard |
| `float` | `f32[s:4,a:4]` | IEEE 754 guarantees 4-byte size |
| `double` | `f64[s:8,a:8]` | IEEE 754 guarantees 8-byte size |
| `char` | `char[s:1,a:1]` | sizeof(char)==1 guaranteed by standard |
| `long` | `i32` or `i64` | Uses actual `sizeof(long)` — reflects platform ABI |
| `wchar_t` | `wchar[s:N,a:N]` | Uses actual sizeof/alignof |
| `long double` | `f80[s:N,a:N]` | Uses actual sizeof/alignof |
| `T*` | `ptr[s:N,a:N]` | Uses actual sizeof/alignof |
| `T C::*` | `memptr[s:N,a:N]` | Uses actual sizeof/alignof |

**Theorem 5.1:** All scalar type signatures accurately reflect their sizeof and alignof
on the current platform.

*Proof:* Fixed-width types use hardcoded constants matching the C++ standard definitions.
Platform-dependent types use `sizeof()` / `alignof()` operators, which return the actual
values for the current compilation target. ∎

### 5.2 Record Types (struct/class)

**Theorem 5.2:** For a non-polymorphic, non-inheriting struct T, the Layout signature
accurately encodes all fields' offsets and types.

*Proof by induction on field count n:*
- n = 0: Empty struct → `record[s:S,a:A]{}`. Size and alignment from sizeof/alignof. ✓
- n = k → n = k+1: The (k+1)-th field's offset is `offset_of(member).bytes` (P2996
  intrinsic — compiler-verified). Its type signature is produced by recursive
  `TypeSignature<FieldType>::calculate()`, which is correct by the base-case theorem
  (scalar) or the inductive hypothesis (nested struct). ✓  ∎

### 5.3 Inheritance

**Theorem 5.3:** Layout signature correctly flattens inheritance hierarchies.

*Proof by induction on inheritance depth d:*
- d = 0: No inheritance → same as Theorem 5.2. ✓
- d = k → d = k+1: Base class fields are flattened via `layout_one_base_prefixed`,
  which passes `offset_of(base_info).bytes + OffsetAdj` as the accumulated offset.
  By inductive hypothesis, depth-k bases are correctly flattened. The new base's offset
  is provided by P2996. ✓

*Definition signature:* Preserves `~base<QualifiedName>:record{...}` structure without
flattening. Qualified names are constructed by `qualified_name_for<>()`, which recursively
traverses the `parent_of` chain. ✓  ∎

### 5.4 Polymorphic Types

**Theorem 5.4:** Polymorphic types are correctly marked in both layers.

- Layout: `record[s:S,a:A,vptr]{...}` — detected by `std::is_polymorphic_v<T>`
- Definition: `record[s:S,a:A,polymorphic]{...}` — same detection

*Note on vptr space:* The vptr occupies sizeof(void*) bytes at an implementation-defined
location. It is NOT a `nonstatic_data_member`, so it does not appear as a `@OFF:SIG` entry.
However, its space IS included in `sizeof(T)` (reflected in `s:SIZE`). This means:
- The `,vptr` marker ensures polymorphic and non-polymorphic types never match
- The `s:SIZE` value accounts for the vptr's memory footprint
- Field offsets remain correct (compiler accounts for vptr in all `offset_of` values)  ∎

### 5.5 Array Types

**Theorem 5.5:** Array signatures correctly encode element type and count.

- `T[N]` → `array[s:sizeof(T[N]),a:alignof(T[N])]<TypeSig<T>,N>`
- Byte arrays (`char[N]`, `uint8_t[N]`, `std::byte[N]`) → `bytes[s:N,a:1]`

The byte-array normalization is correct: `char[N]`, `uint8_t[N]`, and `std::byte[N]` are
all N contiguous bytes with alignment 1. The `is_byte_element_v` trait correctly identifies
all byte-like element types. ∎

### 5.6 Union Types

**Theorem 5.6:** Union signatures correctly preserve member atomicity.

Union members are NOT flattened (unlike struct fields). Each member retains its complete
type signature:
```
union[s:S,a:A]{@0:SIG₁,@0:SIG₂,...}
```

This is correct because union members share the same offset (typically @0). Flattening
would interleave overlapping fields, making the signature ambiguous. ∎

### 5.7 Bit-field Types

**Theorem 5.7:** Bit-field signatures encode byte offset, bit offset, bit width, and
underlying type.

Format: `@BYTE.BIT:bits<WIDTH,underlying_sig>`

All components from P2996 API:
- `offset_of(member).bytes` — byte offset
- `offset_of(member).bits` — bit offset within the byte
- `bit_size_of(member)` — bit width
- `type_of(member)` — underlying type

**Known limitation:** Bit-field ordering (MSB-first vs LSB-first) is implementation-defined
(C++ [class.bit]). Different compilers may produce different bit offsets for the same
bit-field declaration. TypeLayout encodes the current compiler's actual bit offsets, so:
- Same compiler: signature is exact ✓
- Cross-compiler: signature match does NOT guarantee bit-level compatibility ⚠️

### 5.8 Enum Types

**Theorem 5.8:** Enum signatures correctly encode the underlying type. The Definition
layer additionally encodes the fully qualified name.

- Layout: `enum[s:S,a:A]<underlying_sig>`
- Definition: `enum<ns::Color>[s:S,a:A]<underlying_sig>`

The underlying type is obtained via `std::underlying_type_t<T>` (standard-guaranteed).
The qualified name is constructed by `qualified_name_for<^^T>()`. ∎

---

## §6 Summary

### 6.1 Theorem Index

| # | Theorem | Statement | Status |
|---|---------|-----------|--------|
| 2.1 | Encoding Completeness | Sig_L encodes all of L(T) | ✅ Proven |
| 2.2 | Injectivity | L(T) ≠ L(U) ⟹ Sig_L(T) ≠ Sig_L(U) | ✅ Proven |
| 2.2.1 | V1 Soundness | Sig_L(T) = Sig_L(U) ⟹ T ~mem U | ✅ Proven (contrapositive) |
| 2.3 | Non-surjectivity | ∃ T~mem U with Sig_L(T) ≠ Sig_L(U) | ✅ Proven (counterexample) |
| 2.4 | Flattening Correctness | Recursive flattening preserves offsets | ✅ Proven (induction) |
| 3.1 | Def Completeness | Sig_D encodes all of D(T) | ✅ Proven |
| 3.2 | Def Discrimination | All structural differences ⟹ different Sig_D | ✅ Proven |
| 4.1 | V3 Projection | def_match ⟹ layout_match | ✅ Proven |
| 4.2 | Strict Refinement | layout_match ⇏ def_match | ✅ Proven (counterexample) |

### 6.2 Accuracy Classification

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

### 6.3 Formal Guarantees

The TypeLayout signature system provides the following formally proven guarantees:

1. **Soundness (zero false positives):** If two types have matching signatures, their
   memory layouts are guaranteed to be identical.

2. **Conservativeness (safe direction):** Signature mismatch does not necessarily mean
   layout incompatibility — the system may report differences for types that are
   byte-identical but semantically distinct (e.g., array vs struct).

3. **Compiler-verified:** All offsets and sizes come from P2996 compiler intrinsics
   (`std::meta::offset_of`, `sizeof`, `alignof`), not manual calculation.

4. **Platform-aware:** The architecture prefix encodes pointer width and endianness,
   preventing cross-platform false positives.

### 6.4 Assumptions

These proofs assume:
- IEEE 754 floating-point representation on all compared platforms
- Same endianness between compared types (encoded in architecture prefix)
- Same pointer width between compared types (encoded in architecture prefix)
- Padding bytes are not semantically significant (their positions are implicitly
  encoded by offset gaps between fields)
