# Change: Refine Formal Framework via Deep Audit

## Why

A systematic cross-reference between `PROOFS.md` and the implementation
(`detail/signature_impl.hpp`, `detail/type_map.hpp`, `detail/reflect.hpp`)
reveals multiple gaps where the formal model does not accurately describe
the actual system. Some are cosmetic, but several compromise the correctness
of stated theorems. Fixing these strengthens the library's core credibility
-- the formal proofs are a competitive differentiator and must be airtight.

## Audit Findings

### Category A: Formal Model vs Implementation Divergence

**A1. Opaque types break Encoding Faithfulness (Critical)**

Theorem 3.1 states `decode o [T]_L = L_P(T)` for ALL complete types.
But opaque types (registered via `TYPELAYOUT_OPAQUE_*`) produce signatures
like `xstring[s:32,a:8]` -- a user-chosen label with no defined grammar
production. The `decode` function cannot recover `L_P(T)` because:
- The internal field structure is unknown
- The label is user-chosen (not derived from the type)
- `fields_P(T)` is replaced by user annotation

The formal model mentions opaque types nowhere. Theorem 3.1's universality
claim is therefore **false** as stated.

**Fix**: Add an explicit Opaque Annotation Correctness axiom (already
hinted at in `opaque.hpp` comments but absent from PROOFS.md). Restrict
Theorem 3.1 to "reflectable types" or add an opaque caveat.

**A2. Layout flattening skip for opaque sub-types not modeled (Major)**

The implementation (`signature_impl.hpp:155-167`) has an explicit branch:
```
if constexpr (std::is_class_v<FieldType> && !std::is_union_v<FieldType>
              && !has_opaque_signature<FieldType, SignatureMode::Layout>) {
    // flatten
} else {
    // emit as leaf
}
```
This means opaque class fields are NOT flattened, but the formal Definition
1.3 (`flatten`) has no such condition. The formal `flatten` always recurses
into class members.

**Fix**: Extend Definition 1.3 to include a predicate `opaque(T)` that
short-circuits flattening. The formal version becomes:
```
flatten(T, adj) =
    if opaque(T) then [(adj, opaque_sig(T))]
    else ... (current definition)
```

**A3. Base class flattening in Layout skips opaque bases (Major)**

`layout_one_base_prefixed` (line 176-189) also checks
`has_opaque_signature<BaseType>` and emits opaque bases as leaf nodes.
Definition 1.3's `flatten` does not account for this.

**Fix**: Same treatment as A2 -- `flatten` must check `opaque(base_type)`.

**A4. Reference types use sizeof/alignof of pointer, not reference (Minor)**

Implementation (`type_map.hpp:175`):
```cpp
struct TypeSignature<T&, Mode> {
    ... format_size_align("ref", sizeof(T*), alignof(T*));
};
```
Uses `sizeof(T*)` / `alignof(T*)`, NOT `sizeof(T&)` / `alignof(T&)`.
While in practice these are identical on all known platforms, the formal
definition 1.2 writes `sizeof_P(T&)` and `alignof_P(T&)`. This is a
correctness-by-coincidence gap.

**Fix**: Use `sizeof(T&)` in implementation for formal consistency, or
note the equivalence explicitly in the formal model.

**A5. `is_byte_element` covers 7 types, PROOFS.md describes "byte arrays" vaguely (Minor)**

The formal model (Definition 2.1 Case 4) says `if is_byte_element(T)`,
but the actual `is_byte_element()` function covers 7 types:
`char, signed char, unsigned char, int8_t, uint8_t, std::byte, char8_t`.
The formal definition should list these explicitly to make the byte-array
normalization boundary precise.

### Category B: Proof Logic Issues

**B1. Theorem 3.1 circularity risk (Major)**

The "proof" of Theorem 3.1 (Encoding Faithfulness) claims `decode` exists
because "the signature string is constructed by concatenating components
with unique syntax separators" and references Lemma 1.8.1. However:

1. Lemma 1.8.1 is proven via an "LL(1) argument" that itself admits two
   productions require LL(2) and then hand-waves left-factoring. The actual
   left-factoring is not carried out formally.
2. The `decode` function is never constructively defined -- it is asserted
   to exist. A truly rigorous proof would define the parser.

This is not wrong per se, but the proof's strength is overstated. It should
be labeled a "proof sketch" or the left-factoring should be made explicit.

**B2. Theorem 4.2 (V3 Projection) proof has a logical gap (Major)**

The proof says:
> "Since [T]_D is a faithful encoding (Theorem 3.5, by Lemma 1.9.1),
> [T]_D = [U]_D implies R_P(T) and R_P(U) agree on all information
> encoded by [T]_D."

This is correct. But then it concludes:
> "Therefore R_P^layout(T) = R_P^layout(U)"

This step is valid ONLY if `[T]_D = [U]_D` implies the FULL R_P(T) = R_P(U).
But [.]_D is a faithful encoding of D_P(T), not of R_P(T). D_P(T) and R_P(T)
may differ -- specifically, D_P uses `named_fields` while R_P uses the full
member reflection data. The equivalence is assumed but not proven.

More fundamentally, the proof tries to conclude layout equality from
definition equality by going through the semantic domain, but the
Definition denotation function does NOT flatten inheritance. Two types
with `[T]_D = [U]_D` have the same tree structure, but do they have
the same flattened fields? Yes -- but only because flattening is deterministic
given the tree. This step needs explicit justification.

**Fix**: Add a lemma: "For any type T, fields_P(T) is uniquely determined
by D_P(T)" (i.e., flattening the structure tree produces a unique leaf
field sequence). Then Theorem 4.2 follows properly.

**B3. Definition 1.7 (memcmp-compatibility) is misleading (Minor)**

The definition says `T ~=_mem U iff L_P(T) = L_P(U)`. But memcmp-
compatibility is about BYTE-LEVEL identity, while L_P includes type-kind
information (record vs array). The actual guarantee from `L_P(T) = L_P(U)`
is stronger than memcmp -- it includes structural matching.

The name "memcmp-compatible" suggests raw byte equality, which is actually
the weaker `~=_byte` relation defined in Theorem 3.3. The naming is
confusing.

**Fix**: Rename Definition 1.7 to "layout-compatible" or "structurally
compatible" and reserve "memcmp-compatible" for the byte-level relation.

### Category C: Missing Coverage

**C1. No formal treatment of `int` / `short` types (Gap)**

The `type_map.hpp` implementation does NOT have specializations for `int`
and `short`. These must be aliases for `int32_t`/`int16_t` respectively
(otherwise the primary template catch-all would attempt to treat them as
class types). This works because the compiler treats `int` and `int32_t`
as the same type on most platforms.

But the formal model in Definition 1.2 also omits `int` and `short`.
This is a documentation gap -- the model should explicitly state that
platform-standard integer types are assumed to be aliases of fixed-width
types and explain the `requires (!std::is_same_v<...>)` deduplication
mechanism.

**C2. No formal treatment of opaque container/map signatures (Gap)**

`TYPELAYOUT_OPAQUE_CONTAINER` and `TYPELAYOUT_OPAQUE_MAP` generate
signatures with `<element_sig>` and `<key_sig,value_sig>` suffixes.
These are type-parameterized signatures -- a concept entirely absent from
the formal grammar (Section 1.8/1.9). The grammar should be extended or
the opaque category should be explicitly excluded with justification.

**C3. `noexcept` and variadic function pointers (Gap)**

`type_map.hpp` has 4 function pointer specializations:
- `R(*)(Args...)`
- `R(*)(Args...) noexcept`
- `R(*)(Args..., ...)`
- `R(*)(Args..., ...) noexcept`

All produce the same `fnptr[s:N,a:N]` signature. The formal model only
mentions `R(*)(Args...)`. The noexcept and variadic variants should be
noted, along with the intentional information loss (noexcept-ness is
not encoded).

**C4. Unbounded array `T[]` static_assert not formalized (Gap)**

The implementation rejects `T[]` with `static_assert`. The formal model
does not mention this -- it should state that unbounded arrays are outside
the type domain.

**C5. `void` and function types not formalized (Gap)**

Similarly, `void` and `T(Args...)` (function types, not function pointers)
trigger `static_assert`. The formal type domain should explicitly exclude
them.

**C6. PROOFS.md references wrong theorem number (Bug)**

`opaque.hpp` line 16 references "Thm 4.8" for Encoding Faithfulness,
but the actual theorem is 3.1 in PROOFS.md. This cross-reference is stale.

### Category D: Opportunities for Strengthening

**D1. Constructive decode function**

Currently `decode` is existence-claimed. Providing a recursive descent
parser definition (even in pseudocode) would make Theorem 3.1 constructive
and enable property-based testing.

**D2. Formal treatment of comma-prefix/skip_first()**

The implementation note about comma-prefix convention is informal. A short
lemma could prove that `skip_first(concat(",f1", ",f2", ..., ",fn"))` =
`"f1,f2,...,fn"` = `join(",", [f1,...,fn])`.

**D3. Explicit Type Domain definition**

The type domain `Types_P` is never formally defined -- it is implicitly
the set of types that the `TypeSignature` dispatch covers. Making this
explicit would strengthen all universal quantifiers.

## What Changes

1. **Extend PROOFS.md** with:
   - Formal Opaque Type axiom and its impact on Theorem 3.1
   - Explicit `opaque(T)` predicate in Definition 1.3 (flatten)
   - Missing Lemma for Theorem 4.2 (flatten determinism from D_P)
   - Explicit Type Domain definition (Section 1)
   - Formal exclusion of void, T[], and function types
   - Correction of Definition 1.7 naming
   - Byte-element enumeration in Definition 2.1 Case 4
   - Comma-prefix equivalence lemma
   - Function pointer variant coverage note
   - int/short alias explanation
2. **Fix opaque.hpp** stale theorem reference (4.8 -> 3.1)
3. **Fix type_map.hpp** reference types to use sizeof(T&) for formal purity
4. **Update spec** to reflect the refined formal framework

## Impact

- Affected specs: `signature` (Formal Accuracy Guarantees requirement)
- Affected code: `opaque.hpp` (comment fix), `type_map.hpp` (sizeof fix)
- Affected docs: `PROOFS.md` (major revision)
- **NOT BREAKING**: No signature output changes. All fixes are in proofs,
  comments, and one sizeof equivalence.
