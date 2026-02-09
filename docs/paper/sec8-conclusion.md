# §8 Discussion and Limitations

## 8.1 Known Design Limits

TypeLayout makes several intentional design choices that limit its scope:

**Signature match is implication, not equivalence.** The core guarantee is
⟦T⟧_L = ⟦U⟧_L ⟹ T ≅_mem U, but the converse does not hold. For example,
`int32_t[3]` and `struct { int32_t a, b, c; }` have identical byte
representations but different signatures (array vs record). This is a
deliberate safety choice: structural differences often indicate semantic
incompatibility even when bytes align.

**Type's own name is not in the signature.** TypeLayout performs structural
analysis, not nominal analysis. `struct Point` and `struct Coord` with
identical field names and types produce identical Definition signatures.
Users who need nominal identity should combine TypeLayout with other
mechanisms (e.g., `typeid`).

**Union members are not recursively flattened.** Flattening union members
would produce misleading offset information because union members overlap
in memory. Each union member is preserved as an atomic type signature.

**Bit-fields are marked non-portable.** Bit-field layout is
implementation-defined (C++ [class.bit]). While TypeLayout correctly
encodes bit-field layout on a given platform, cross-platform comparison of
bit-field signatures is unreliable.

## 8.2 Compiler Dependency

TypeLayout currently requires Bloomberg's Clang P2996 fork—the only
compiler implementing P2996 as of this writing. This limits immediate
adoption. However:

1. P2996 is on track for C++26 standardization, and multiple compiler
   vendors (GCC, MSVC, EDG) are expected to implement it.
2. The two-phase toolchain (§5) mitigates this limitation: signatures can
   be exported once and verified on any C++17 compiler.
3. The `constexpr` step limit (§6.3.2) may be relaxed as compiler
   implementations mature.

## 8.3 Scalability

The O(n²) constexpr step consumption for n-field structs is a practical
concern for very large types (> 100 fields). Potential mitigations include:

- **Compiler improvements**: More efficient `constexpr` evaluation
- **Chunked generation**: Generate signatures in segments and combine
- **Hash-based comparison**: Compare signature hashes instead of full strings
  (sacrificing human-readability for scalability)

## 8.4 Future Work

**Vtable signature.** Extending TypeLayout to generate virtual table
signatures would cover a larger portion of the ABI surface (currently 44%
of ABI Checker's scope).

**Differential signatures.** Instead of comparing full signatures, a
differential mode could report *which* fields changed between two type
versions—useful for migration guidance.

**Mechanized proofs.** The current proofs are written in semi-formal
mathematical notation. Mechanizing them in Coq or Lean would provide
machine-checked correctness guarantees.

**Broader language support.** The signature concept is not C++-specific.
Similar systems could be built for Rust (using procedural macros), Zig
(using comptime), or any language with compile-time reflection capabilities.

---

# §9 Conclusion

We have presented TypeLayout, a compile-time type layout signature system
for C++ based on C++26 static reflection (P2996). The key contributions are:

1. **A two-layer signature system** that distinguishes byte identity (Layout)
   from structural identity (Definition), enabling users to choose the
   appropriate level of verification strictness for their use case.

2. **Formal correctness proofs** establishing that the system is *sound*
   (zero false positives), *injective* (different layouts produce different
   signatures), and *well-ordered* (Definition strictly refines Layout).
   These proofs, grounded in denotational semantics and refinement theory,
   provide a level of correctness assurance rare in C++ libraries.

3. **A cross-platform verification toolchain** that decouples signature
   generation (requiring P2996) from signature comparison (any C++17
   compiler), enabling practical cross-platform ABI monitoring.

4. **A comprehensive evaluation** demonstrating complete type coverage
   (17 categories), compilation overhead characterization, and quantitative
   superiority over manual `sizeof`/`offsetof` approaches (86% fewer lines
   of verification code with strictly stronger guarantees).

TypeLayout replaces the ad-hoc, error-prone practice of hand-written
layout assertions with a single, compiler-verified line of code—turning
type layout verification from a maintenance burden into a zero-cost
compile-time guarantee. As C++26 static reflection becomes widely available,
we believe compile-time layout verification will become a standard practice
in systems programming, and TypeLayout provides both the theoretical
foundation and practical tooling to enable that transition.

**Availability.** TypeLayout is open-source under the Boost Software License
1.0, available at https://github.com/ximicpp/TypeLayout.
