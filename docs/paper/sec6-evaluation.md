# §6 Evaluation

This section evaluates TypeLayout along four dimensions: type coverage,
signature correctness, compile-time overhead, and comparison with existing
approaches.

## 6.1 Type Coverage

We systematically tested TypeLayout's signature generation across all C++
type categories to verify completeness. The test suite (`test_two_layer.cpp`)
uses `static_assert` for all checks—compilation success constitutes test
passage.

| Category | Types tested | Layout ✓ | Definition ✓ |
|----------|-------------|----------|-------------|
| Fixed-width integers | i8, u8, i16, u16, i32, u32, i64, u64 | ✅ | ✅ |
| Floating point | float, double, long double | ✅ | ✅ |
| Characters | char, char8_t, char16_t, char32_t, wchar_t | ✅ | ✅ |
| Boolean | bool | ✅ | ✅ |
| Pointers | T\*, T&, T&&, T C::\*, function pointers | ✅ | ✅ |
| Enums | scoped, unscoped, various underlying types | ✅ | ✅ |
| Arrays | T[N], byte arrays (char[N], uint8_t[N], byte[N]) | ✅ | ✅ |
| Simple structs | 1-5 fields, mixed types | ✅ | ✅ |
| Nested structs | 2-3 levels of nesting | ✅ | ✅ |
| Single inheritance | 1-3 levels | ✅ | ✅ |
| Multiple inheritance | 2 base classes | ✅ | ✅ |
| Virtual inheritance | single virtual base | ✅ | ✅ |
| Polymorphic types | virtual function, abstract class | ✅ | ✅ |
| Empty Base Optimization | empty base + derived fields | ✅ | ✅ |
| Unions | simple and nested | ✅ | ✅ |
| Bit-fields | various widths, packed | ✅ | ✅ |
| CV-qualified fields | const, volatile members | ✅ | ✅ |

**Result:** TypeLayout generates correct signatures for all 17 type
categories. The test suite contains 100+ `static_assert` statements that
verify both signature generation and cross-type comparison.

## 6.2 Signature Correctness Verification

We verify correctness through three complementary methods:

**Method 1: Reference signature comparison.** Each test type has a
hand-computed reference signature. The `static_assert` verifies that
`get_layout_signature<T>()` matches the expected string exactly.

**Method 2: Cross-type matching.** For types designed to be layout-compatible
(e.g., `Derived` vs `Flat`), we verify `layout_signatures_match` returns
`true`. For types designed to differ, we verify it returns `false`.

**Method 3: Projection consistency.** For every type pair where
`definition_signatures_match` is `true`, we verify that
`layout_signatures_match` is also `true` (Theorem 4.14).

**Result:** All 100+ verification assertions pass. Zero false positives
observed. Zero projection violations observed.

## 6.3 Compile-Time Overhead

Since TypeLayout operates entirely at compile time, the relevant performance
metric is compilation time rather than runtime. We measured the compile-time
overhead on the Bloomberg Clang P2996 fork.

### 6.3.1 Signature Length Scaling

| Struct fields | Layout sig length | Definition sig length | Characters per field |
|--------------|-------------------|----------------------|---------------------|
| 20 | ~361 chars | ~1,032 chars | ~18 (L) / ~52 (D) |
| 40 | ~717 chars | ~2,052 chars | ~18 (L) / ~51 (D) |
| 60 | ~1,077 chars | ~3,072 chars | ~18 (L) / ~51 (D) |
| 80 | ~1,437 chars | ~4,092 chars | ~18 (L) / ~51 (D) |
| 100 | ~1,797 chars | ~5,112 chars | ~18 (L) / ~51 (D) |

Signature length scales linearly with the number of fields, at approximately
18 characters per field (Layout) or 51 characters per field (Definition).
The ~3x difference is due to field names and structural markers.

### 6.3.2 Constexpr Step Consumption

The Bloomberg Clang compiler imposes a default `constexpr` evaluation step
limit. TypeLayout's compile-time string concatenation consumes steps
proportional to O(n²) in the number of fields, due to the `FixedString::operator+`
copying the entire accumulated string for each new field.

| Struct size | Recommended `-fconstexpr-steps` |
|------------|-------------------------------|
| < 50 fields | Default (no flag needed) |
| 50-80 fields | 1,500,000 |
| 80-100 fields | 3,000,000 |
| > 100 fields | 5,000,000 |

### 6.3.3 Compilation Time

Measured on a machine with Intel Core i7, 32GB RAM, SSD:

| Test case | Compilation time | Overhead vs empty file |
|-----------|-----------------|----------------------|
| Single simple struct (5 fields) | ~1.2s | +0.1s |
| 10 struct comparisons | ~2.0s | +0.9s |
| Full test suite (100+ asserts) | ~4.5s | +3.4s |

The overhead is modest for typical use cases (< 20 types). For large-scale
deployment with hundreds of types, the `.sig.hpp` caching strategy (§5)
ensures that signature generation is a one-time cost.

## 6.4 Comparison with Existing Approaches

### 6.4.1 Feature Comparison

| Feature | sizeof/offsetof | RTTI | Boost.PFR | ABI Checker | Protobuf | TypeLayout |
|---------|:---:|:---:|:---:|:---:|:---:|:---:|
| Compile-time | ✅ | ❌ | ✅ | ❌ | ✅ | ✅ |
| Field offsets | Manual | ❌ | ❌ | ✅ | N/A | ✅ |
| Field types | Manual | ❌ | Partial | ✅ | ✅ | ✅ |
| Inheritance | Manual | ❌ | ❌ | ✅ | N/A | ✅ |
| Bit-fields | ❌ | ❌ | ❌ | ✅ | N/A | ✅ |
| Compare two types | ❌ | Name only | ❌ | ✅ | Schema | ✅ |
| Cross-platform | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ |
| Zero overhead | ✅ | ❌ | ✅ | N/A | ❌ | ✅ |
| Native structs | ✅ | ✅ | ✅ | ✅ | ❌ | ✅ |
| Automatic | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Formal proofs | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |

### 6.4.2 Effort Comparison

For a struct with *n* fields, the verification effort is:

| Approach | Lines of verification code | Maintenance cost |
|----------|---------------------------|-----------------|
| Manual sizeof/offsetof | 2 + n (sizeof + n offsets) | O(n) per change |
| TypeLayout | 1 | O(1) per change |
| ABI Checker | External tool invocation | O(1) but post-build |
| Protobuf | Schema file + codegen setup | O(1) but non-native |

**Example:** For `PacketHeader` with 5 fields:
- Manual: 7 lines (`sizeof` + `alignof` + 5 `offsetof`)
- TypeLayout: 1 line (`static_assert(layout_signatures_match<A, B>())`)
- Reduction: **86% fewer lines**, with **strictly stronger** guarantees

### 6.4.3 ABI Compliance Checker Comparison

The ABI Compliance Checker (ACC) is the closest existing tool in terms of
coverage. A detailed comparison:

| Dimension | ABI Checker | TypeLayout |
|-----------|-------------|------------|
| **Input** | Compiled binaries + DWARF | Source code |
| **Timing** | Post-build | Compile-time |
| **Integration** | External tool | `static_assert` |
| **Scope** | Data layout + vtables + symbols | Data layout only |
| **Formal guarantees** | None | Soundness + Injectivity proofs |
| **Cross-platform** | Per-binary analysis | Signature comparison |

TypeLayout covers approximately 44% of ACC's total feature set (the data
layout portion), but provides it at compile time with formal correctness
guarantees. The two tools are complementary: TypeLayout for real-time
compile-time monitoring, ACC for comprehensive post-build audits.

## 6.5 Threats to Validity

**Internal validity.** Our correctness claims depend on the P2996
implementation in Bloomberg's Clang fork. If the compiler reports incorrect
offsets, the signatures will be incorrect. We mitigate this by cross-checking
against manual `offsetof` values in the test suite.

**External validity.** TypeLayout currently runs only on one compiler
(Bloomberg Clang P2996 fork). Results may differ when P2996 is implemented
in other compilers. However, since TypeLayout uses only standardized P2996
APIs (`nonstatic_data_members_of`, `offset_of`, etc.), we expect portability
to future implementations.

**Construct validity.** The compile-time overhead measurements are specific
to the Bloomberg Clang fork and may not generalize to other P2996
implementations.
