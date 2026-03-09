# §4 Formal Semantics

> **Cross-reference:** This is the condensed paper version. For the full formal
> proofs (S1-S7, ~1,000 lines), see [`PROOFS.md`](../../PROOFS.md) at the
> repository root.

This section develops the formal semantics of TypeLayout's Layout signature
system. We define the semantic domains, the signature denotation function,
and prove four key results: Encoding Faithfulness and its Injectivity corollary,
Soundness, and Conservativeness.

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

**Definition 4.3 (memcpy-compatibility).** Two types *T*, *U* are
memcpy-compatible, written *T* ≅_mem *U*, iff *L_P*(*T*) = *L_P*(*U*).

## 4.2 Signature Denotation Function

**Definition 4.4 (Layout Denotation).** The Layout signature denotation
function ⟦·⟧_L : Types_P → Σ\* is defined by structural recursion over
type constructors:

| Case | Type *T* | ⟦*T*⟧_L |
|------|----------|---------|
| Primitive | τ ∈ PrimitiveTypes | σ(τ), e.g., `i32[s:4,a:4]` |
| Record | struct/class | `record[s:S,a:A]{@o₁:sig₁,...,@oₙ:sigₙ}` with fields flattened |
| Polymorphic | virtual class | `record[s:S,a:A]{@0:ptr[...],@O:fields...}` (vptr as synthesized field) |
| Array | *T*[*N*] | `array[s:S,a:A]<⟦T⟧_L,N>` |
| Byte array | char[*N*] et al. | `bytes[s:N,a:1]` |
| Union | union | `union[s:S,a:A]{@o₁:sig₁,...}` (not flattened) |
| Enum | enum *E* : *U* | `enum[s:S,a:A]<⟦U⟧_L>` |
| Bit-field | width *W* | `@B.b:bits<W,⟦U⟧_L>` |
| CV-qualified | const/volatile *T* | ⟦*T*⟧_L (erasure) |
| Opaque | user-registered *T* | `O(Tag\|N\|M)` |

**Lemma 4.5 (Grammar Unambiguity).** The Layout grammar (§3.2) is
unambiguous: every valid signature string has exactly one parse tree.

*Proof sketch.* The grammar is LL(1) after left-factoring two productions
(`meta` → `metatail` and `field` → `fieldtail`).  The `opaque` production
(`O(` ...) is distinguished from all other productions by its fixed `O(`
prefix, which is disjoint from all built-in type prefixes and keywords.
Under this constraint, the lookahead token uniquely determines the production,
and the grammar remains LL(1).  Since LL(1) grammars produce unique leftmost
derivations, every string has a unique parse tree. ∎

## 4.3 Core Theorems

**Assumption 4.5a (Opaque Annotation Correctness).** For every type *T*
registered via `TYPELAYOUT_REGISTER_OPAQUE` with parameters `(Tag, HasPointer)`:

1. `sizeof(T)` and `alignof(T)` are faithfully encoded (deduced by the macro).
2. If two opaque types *T*, *U* produce the same opaque signature, their
   internal memory layouts are identical (user responsibility; not
   compiler-verifiable).

Under this assumption, the Encoding Faithfulness and Soundness theorems
extend to opaque types.  Without assumption (2), an opaque signature match
guarantees only sizeof/alignof identity, not field-level byte compatibility.

### Theorem 4.6 (Encoding Faithfulness)

**Statement.** ⟦·⟧_L is a faithful encoding: there exists a partial
function decode : Σ\* ⇀ Layout such that:

> decode(⟦*T*⟧_L) = *L_P*(*T*) for all complete types *T* in Types_P

**Proof.** By Lemma 4.5, any string in Im(⟦·⟧_L) can be uniquely parsed.
Define `decode` as this parsing process. By construction of ⟦·⟧_L
(Definition 4.4), each component of *L_P*(*T*) is faithfully encoded:
- sizeof from `s:N`, alignof from `a:N`
- Polymorphism from synthesized `ptr[...]` field at vptr offset
- Fields from the ordered `@offset:typesig` list

Therefore decode ∘ ⟦·⟧_L = *L_P* on Types_P. ∎

**Analogy.** This theorem is analogous to CompCert's semantic preservation:
compilation preserves program semantics; here, signature generation preserves
layout information.

### Corollary 4.7 (Injectivity)

> *L_P*(*T*) ≠ *L_P*(*U*) ⟹ ⟦*T*⟧_L ≠ ⟦*U*⟧_L

*Proof.* Contrapositive of Theorem 4.6: if ⟦*T*⟧_L = ⟦*U*⟧_L, then
*L_P*(*T*) = decode(⟦*T*⟧_L) = decode(⟦*U*⟧_L) = *L_P*(*U*). ∎

### Theorem 4.8 (Soundness — Zero False Positives)

**Statement.**

> ⟦*T*⟧_L = ⟦*U*⟧_L ⟹ *T* ≅_mem *U*

**Proof.** By Theorem 4.6:
⟦*T*⟧_L = ⟦*U*⟧_L ⟹ *L_P*(*T*) = *L_P*(*U*) ⟹ *T* ≅_mem *U* (by Def. 4.3). ∎

**Significance.** This is the core safety guarantee: if two types have matching
Layout signatures, they are guaranteed to have identical byte layouts. There
are *zero false positives* under stated assumptions.

### Theorem 4.9 (Conservativeness — Intentional False Negatives)

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

## 4.4 Summary of Formal Guarantees

| Property | Statement | Implication |
|----------|-----------|-------------|
| **Encoding Faithfulness** (Thm 4.6) | decode ∘ ⟦·⟧_L = *L_P* | Signatures preserve all layout information |
| **Injectivity** (Cor 4.7) | *L_P*(*T*) ≠ *L_P*(*U*) ⟹ ⟦*T*⟧_L ≠ ⟦*U*⟧_L | Different layouts → different signatures |
| **Soundness** (Thm 4.8) | ⟦*T*⟧_L = ⟦*U*⟧_L ⟹ *T* ≅_mem *U* | Signature match → memcpy-safe (zero false positives) |
| **Conservativeness** (Thm 4.9) | ∃ byte-equal types with different sigs | Intentional false negatives (safe direction) |

These four results establish that the Layout signature system is *sound* (no
false positives) and *conservative* (errs on the side of safety).
