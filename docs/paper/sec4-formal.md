# §4 Formal Semantics

This section develops the formal semantics of TypeLayout's two-layer signature
system. We define the semantic domains, the signature denotation functions,
and prove three core theorems: Soundness, Encoding Faithfulness (Injectivity),
and Strict Refinement (Projection).

## 4.1 Semantic Domains

We ground our formalization in denotational semantics (Scott-Strachey) and
refinement theory (Z/B methods).

**Definition 4.1 (Platform).** A platform *P* is a triple
(*w*, *e*, *abi*) where *w* ∈ {32, 64} is the pointer width in bits,
*e* ∈ {le, be} is the byte order, and *abi* is an ABI specification
identifier.

**Definition 4.2 (Byte Layout).** For a type *T* on platform *P*:

> *L_P*(*T*) = (sizeof_P(*T*), alignof_P(*T*), poly_P(*T*), fields_P(*T*))

where:
- sizeof_P(*T*) ∈ ℕ — object size in bytes
- alignof_P(*T*) ∈ ℕ — alignment requirement
- poly_P(*T*) ∈ {true, false} — whether the type has a vtable pointer
- fields_P(*T*) ∈ (ℕ × Σ\*)* — an ordered sequence of (offset, type signature) pairs

The field sequence is obtained by recursively flattening inheritance
hierarchies and nested (non-union) class members to their leaf fields,
adjusting offsets accordingly.

**Definition 4.3 (Structure Tree).** For a type *T* on platform *P*:

> *D_P*(*T*) = (sizeof_P(*T*), alignof_P(*T*), poly_P(*T*), bases_P(*T*), named_fields_P(*T*))

where:
- bases_P(*T*) ∈ (Σ\* × {base, vbase} × *D_P*(·))* — base classes with qualified names and recursive structure
- named_fields_P(*T*) ∈ (ℕ × Σ\* × Σ\*)* — fields with (offset, name, type signature)

**Definition 4.4 (memcpy-compatibility).** Two types *T*, *U* are
memcpy-compatible, written *T* ≅_mem *U*, iff *L_P*(*T*) = *L_P*(*U*).

## 4.2 Signature Denotation Functions

**Definition 4.5 (Layout Denotation).** The Layout signature denotation
function ⟦·⟧_L : Types_P → Σ\* is defined by structural recursion over
type constructors:

| Case | Type *T* | ⟦*T*⟧_L |
|------|----------|---------|
| Primitive | τ ∈ PrimitiveTypes | σ(τ), e.g., `i32[s:4,a:4]` |
| Record | struct/class | `record[s:S,a:A]{@o₁:sig₁,...,@oₙ:sigₙ}` with fields flattened |
| Polymorphic | virtual class | `record[s:S,a:A,vptr]{...}` |
| Array | *T*[*N*] | `array[s:S,a:A]<⟦T⟧_L,N>` |
| Byte array | char[*N*] et al. | `bytes[s:N,a:1]` |
| Union | union | `union[s:S,a:A]{@o₁:sig₁,...}` (not flattened) |
| Enum | enum *E* : *U* | `enum[s:S,a:A]<⟦U⟧_L>` |
| Bit-field | width *W* | `@B.b:bits<W,⟦U⟧_L>` |
| CV-qualified | const/volatile *T* | ⟦*T*⟧_L (erasure) |

**Definition 4.6 (Definition Denotation).** The Definition denotation
⟦·⟧_D : Types_P → Σ\* extends ⟦·⟧_L with:

- Field names: `@o[name]:sig` instead of `@o:sig`
- Inheritance: `~base<QName>:⟦Base⟧_D` instead of flattening
- Polymorphism: `,polymorphic` instead of `,vptr`
- Enum names: `enum<QName>[s:S,a:A]<⟦U⟧_D>`

**Lemma 4.7 (Grammar Unambiguity).** Both the Layout grammar (§3.2.1) and
the Definition grammar (§3.2.2) are unambiguous: every valid signature string
has exactly one parse tree.

*Proof sketch.* Both grammars are LL(1) after left-factoring two productions
(`meta` → `metatail` and `field` → `fieldtail`). Since LL(1) grammars
produce unique leftmost derivations, every string has a unique parse tree. ∎

## 4.3 Core Theorems

### Theorem 4.8 (Encoding Faithfulness)

**Statement.** ⟦·⟧_L is a faithful encoding: there exists a partial
function decode : Σ\* ⇀ Layout such that:

> decode(⟦*T*⟧_L) = *L_P*(*T*) for all complete types *T* in Types_P

**Proof.** By Lemma 4.7, any string in Im(⟦·⟧_L) can be uniquely parsed.
Define `decode` as this parsing process. By construction of ⟦·⟧_L
(Definition 4.5), each component of *L_P*(*T*) is faithfully encoded:
- sizeof from `s:N`, alignof from `a:N`
- Polymorphism from presence/absence of `,vptr`
- Fields from the ordered `@offset:typesig` list

Therefore decode ∘ ⟦·⟧_L = *L_P* on Types_P. ∎

**Analogy.** This theorem is analogous to CompCert's semantic preservation:
compilation preserves program semantics; here, signature generation preserves
layout information.

### Corollary 4.9 (Injectivity)

> *L_P*(*T*) ≠ *L_P*(*U*) ⟹ ⟦*T*⟧_L ≠ ⟦*U*⟧_L

*Proof.* Contrapositive of Theorem 4.8: if ⟦*T*⟧_L = ⟦*U*⟧_L, then
*L_P*(*T*) = decode(⟦*T*⟧_L) = decode(⟦*U*⟧_L) = *L_P*(*U*). ∎

### Theorem 4.10 (Soundness — Zero False Positives)

**Statement.**

> ⟦*T*⟧_L = ⟦*U*⟧_L ⟹ *T* ≅_mem *U*

**Proof.** By Theorem 4.8:
⟦*T*⟧_L = ⟦*U*⟧_L ⟹ *L_P*(*T*) = *L_P*(*U*) ⟹ *T* ≅_mem *U* (by Def. 4.4). ∎

**Significance.** This is the core safety guarantee: if two types have matching
Layout signatures, they are guaranteed to have identical byte layouts. There
are *zero false positives* under stated assumptions.

### Theorem 4.11 (Conservativeness — Intentional False Negatives)

**Statement.** ∃ *T*, *U* such that *T* and *U* have identical byte
representations but ⟦*T*⟧_L ≠ ⟦*U*⟧_L.

**Proof.** Counterexample:

```cpp
struct A { int32_t x, y, z; };   // record{@0:i32,@4:i32,@8:i32}
using  B = int32_t[3];           // array<i32,3>
```

Both occupy 12 bytes with three contiguous `int32_t` values. However:
- ⟦A⟧_L = `record[s:12,a:4]{@0:i32[...],@4:i32[...],@8:i32[...]}`
- ⟦B⟧_L = `array[s:12,a:4]<i32[s:4,a:4],3>`

The signatures differ because *L_P*(*A*) ≠ *L_P*(*B*) (different type
kinds). This is an intentional safety choice: the system prefers false
negatives over false positives. ∎

### Theorem 4.12 (Definition Encoding Faithfulness)

**Statement.** ⟦·⟧_D is a faithful encoding from *D_P* to Σ\*:

> decode_D(⟦*T*⟧_D) = *D_P*(*T*) for all complete types *T*

*Proof.* Analogous to Theorem 4.8, using Lemma 4.7 for the Definition
grammar. ∎

## 4.4 The Projection Theorem

### Definition 4.13 (Projection Function)

Define π : *D_P*(*T*) → *L_P*(*T*) by:
1. Erase field names
2. Flatten inheritance hierarchy (expand base class subobjects with adjusted offsets)
3. Replace `polymorphic` marker with `vptr`
4. Erase qualified enum names

### Theorem 4.14 (Strict Refinement — Projection)

**Statement.**

> ⟦*T*⟧_D = ⟦*U*⟧_D ⟹ ⟦*T*⟧_L = ⟦*U*⟧_L

Equivalently: `definition_signatures_match<T,U>()` ⟹ `layout_signatures_match<T,U>()`.

**Proof.**

(1) ⟦*T*⟧_D = ⟦*U*⟧_D
    ⟹ *D_P*(*T*) = *D_P*(*U*) [by Theorem 4.12]

(2) *D_P*(*T*) = *D_P*(*U*)
    ⟹ π(*D_P*(*T*)) = π(*D_P*(*U*)) [π is a function]
    ⟹ *L_P*(*T*) = *L_P*(*U*) [by definition of π]

(3) *L_P*(*T*) = *L_P*(*U*)
    ⟹ ⟦*T*⟧_L = ⟦*U*⟧_L [by Corollary 4.9, contrapositively:
       if *L_P*(*T*) = *L_P*(*U*), then decode maps ⟦*T*⟧_L and ⟦*U*⟧_L
       to the same value, and by faithfulness of ⟦·⟧_L,
       ⟦*T*⟧_L = ⟦*U*⟧_L]. ∎

### Theorem 4.15 (Strictness — Converse Does Not Hold)

**Statement.** ∃ *T*, *U* such that ⟦*T*⟧_L = ⟦*U*⟧_L but ⟦*T*⟧_D ≠ ⟦*U*⟧_D.

**Proof.** Counterexample:

```cpp
struct Base { int32_t x; };
struct Derived : Base { int32_t y; };
struct Flat { int32_t x; int32_t y; };
```

Layout: both produce `record[s:8,a:4]{@0:i32[...],@4:i32[...]}` (identical).
Definition: `Derived` has `~base<Base>:record{...}` while `Flat` does not
(different). ∎

## 4.5 Summary of Formal Guarantees

| Property | Statement | Implication |
|----------|-----------|-------------|
| **Encoding Faithfulness** (Thm 4.8) | decode ∘ ⟦·⟧_L = *L_P* | Signatures preserve all layout information |
| **Injectivity** (Cor 4.9) | *L_P*(*T*) ≠ *L_P*(*U*) ⟹ ⟦*T*⟧_L ≠ ⟦*U*⟧_L | Different layouts → different signatures |
| **Soundness** (Thm 4.10) | ⟦*T*⟧_L = ⟦*U*⟧_L ⟹ *T* ≅_mem *U* | Signature match → memcpy-safe (zero false positives) |
| **Conservativeness** (Thm 4.11) | ∃ byte-equal types with different sigs | Intentional false negatives (safe direction) |
| **Projection** (Thm 4.14) | def_match ⟹ layout_match | Definition is strictly more precise |
| **Strictness** (Thm 4.15) | layout_match ⇏ def_match | Two layers are truly distinct |

These six results establish that the two-layer system is *sound* (no false
positives), *conservative* (errs on the side of safety), and *well-ordered*
(Definition strictly refines Layout).
