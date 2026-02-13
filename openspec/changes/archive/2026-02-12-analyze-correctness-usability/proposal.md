# Change: Analyze Correctness and Usability of Current Approach

## Why

TypeLayout has undergone significant architectural work (header restructure,
FixedString P2484 alignment, formal framework refinement). Before investing
in further feature development or the PROOFS.md restructure, we need a
grounded assessment of the current system's **correctness** (does it do
what it claims?) and **usability** (can real users adopt it effectively?).

This is a pure analysis -- no code changes, only findings and recommendations.

## Analysis

### Part I: Correctness Assessment

#### I.1 Formal-Implementation Correspondence (STRONG)

After the `refine-formal-framework` change, the formal model and implementation
are tightly aligned:

| Formal Concept | Implementation | Verified By |
|---|---|---|
| Definition 1.3 (flatten with opaque) | `layout_all_prefixed()` + `has_opaque_signature` | test_opaque.cpp #12-#15 |
| Definition 2.1 Case 2/3 (record) | Primary template in `type_map.hpp` | test_two_layer.cpp static_asserts |
| Definition 2.1 Case 4 (array) | `TypeSignature<T[N], Mode>` | test_two_layer.cpp array tests |
| Definition 2.1 Case 5 (union) | `get_layout_union_content()` | test_two_layer.cpp union tests |
| Definition 2.1 Case 6 (enum) | Primary template enum branch | test_two_layer.cpp enum tests |
| Definition 1.6 (CV erasure) | `TypeSignature<const T>` etc. | test_two_layer.cpp CV tests |
| Theorem 4.2 (V3 Projection) | Projection static_asserts | test_two_layer.cpp lines 237-241 |
| Axiom 1.0.1 (Opaque) | `static_assert(sizeof==size)` in macros | test_opaque.cpp |

**Assessment: 9/10.** Every formal definition has a direct implementation counterpart.
The remaining gap is that the `decode` function (Theorem 3.1) exists only as a
concept -- there is no actual parser implementation to validate unambiguity
empirically. This is acceptable for a proof document but limits property-based
testing opportunities.

#### I.2 Signature Correctness (STRONG with caveats)

**Correct behaviors verified by tests:**
- Inheritance flattening: `Derived` matches `Flat` in Layout (5 test cases)
- Composition flattening: nested structs flatten correctly (2 test cases)
- Polymorphic differentiation: `vptr` marker prevents false match
- Enum qualified names: different-namespace enums distinguished in Definition
- Byte-array normalization: `char[]`, `uint8_t[]`, `std::byte[]` unify
- CV-qualifier erasure: `const int` = `int` in both layers
- Empty base optimization: correctly handled
- Opaque as field and base: correctly treated as leaf node

**Potential correctness concerns:**

**(C1) Padding bytes are invisible but gap-encoded.** The signature records
field offsets (`@0`, `@4`, `@8`...) but does NOT record padding explicitly.
If two types have the same fields at the same offsets but different padding
values (e.g., one zero-initialized, one uninitialized), they produce
identical signatures. This is correct by design (padding is not semantically
significant), but users doing `memcmp` on instances may get different results
even when signatures match. The formal model states this in S6.5 but the
user-facing documentation does not emphasize it enough.

**(C2) Empty non-base struct fields are invisible in flat field list.**
`test_opaque.cpp` F5 test shows `struct WithEmpty { int x; Empty e; int y; }`
-- the `Empty e` contributes zero leaf fields. Correctness is preserved
because `sizeof` differs, so the record header `[s:SIZE]` catches it. But
if a user sees the flat field list and doesn't notice the size header, they
might think the empty field "disappeared."

**(C3) `to_fixed_string()` returns `FixedString<20>` always.** The integer-
to-string conversion always produces a `FixedString<20>` (max uint64_t digits),
regardless of the actual number of digits. This means every offset and size
value in a signature carries up to 19 bytes of trailing zeros in the template
type. The concatenation `operator+` correctly handles this (stops at `\0`),
but the resulting FixedString's template parameter `N` accumulates padding.
For a struct with 10 fields, each `@OFFSET:` contributes `FixedString<20>`
twice (offset and size), leading to `FixedString<N>` with N much larger than
the actual content. This does not affect correctness (equality comparison
stops at `\0`) but inflates compile-time memory usage.

**(C4) `skip_first()` returns `FixedString<N>`, not `FixedString<N-1>`.**
When removing the leading comma, the returned string has the same template
size N but one fewer logical character. This is correct but wasteful -- the
extra byte is always `\0`.

**(C5) Bit-field ordering is implementation-defined across compilers.**
Two types with identical bit-field declarations compiled on GCC vs MSVC
may produce identical TypeLayout signatures but have different actual bit
layouts. The formal model documents this caveat (S5.7), and `classify_safety`
correctly marks bit-field types as `Risk`. But there is no compile-time
mechanism to detect cross-compiler bit-field differences.

#### I.3 Test Coverage Assessment (ADEQUATE)

| Category | Tests | Coverage |
|---|---|---|
| Basic record (Layout + Definition) | 4 | Good |
| Inheritance (single, multi, virtual) | 6 | Good |
| Composition flattening | 4 | Good |
| Polymorphic | 3 | Good |
| Enum identity | 4 | Good |
| Namespace collision | 4 | Good |
| Deep namespace | 4 | Good |
| Bit-fields | 2 | Minimal |
| Anonymous members | 2 | Minimal |
| Arrays | 2 | Minimal |
| CV-qualified | 3 | Good |
| Pointer/reference/fnptr | 3 | Minimal |
| Platform types (long double, wchar_t) | 2 | Minimal |
| alignas | 2 | Good |
| Empty base | 1 | Minimal |
| Empty field (F5) | 1 | Minimal |
| [[no_unique_address]] (F6) | 2 | Weak (no match assertion) |
| long alias (F8) | 1 | Good |
| Opaque type/container/map | 7 | Good |
| Opaque as field/base | 4 | Good |
| is_fixed_enum | 4 | Good |
| FixedString operations | 12 | Good (dedicated test file) |
| SigExporter output | 5 | Good (dedicated test file) |
| CompatReporter | 4 | Good (dedicated test file) |
| **Total** | **~80** | |

**Gaps:**
- No negative test for unsupported types (void, T[], function types)
- No test for member-pointer (`T C::*`) signatures
- No test for `std::nullptr_t` signatures
- No test for `rref` (rvalue reference) in a struct field
- F6 (`[[no_unique_address]]`) only checks compilation, not match behavior
- No cross-compilation test (would require multi-platform CI)

### Part II: Usability Assessment

#### II.1 API Surface (CLEAN but narrow)

The public API consists of exactly 6 functions:

```cpp
// Core (consteval, compile-time only)
get_layout_signature<T>()       -> FixedString<N>
get_definition_signature<T>()   -> FixedString<N>
layout_signatures_match<T, U>() -> bool
definition_signatures_match<T, U>() -> bool

// Safety classification (runtime)
compat::classify_safety(sig)    -> SafetyLevel
is_fixed_enum<T>()              -> bool
```

Plus 3 macros: `TYPELAYOUT_OPAQUE_TYPE`, `TYPELAYOUT_OPAQUE_CONTAINER`,
`TYPELAYOUT_OPAQUE_MAP`, and 2 export macros: `TYPELAYOUT_EXPORT_TYPES`,
`TYPELAYOUT_REGISTER_TYPES`.

**Assessment: 8/10.** The API is minimal and focused. However:

**(U1) No way to get signature as `const char*` at compile time.** Users must
use `FixedString<N>` which is a library-specific type. There is no
`constexpr const char*` accessor, making interop with other compile-time
string libraries difficult.

**(U2) No way to query individual field information.** The signature is an
opaque string. Users cannot programmatically extract "what is the type at
offset 8?" or "how many fields does this type have?" without parsing the
signature string themselves.

**(U3) `FixedString<N>` template parameter `N` is unpredictable.** Due to
`to_fixed_string()` always returning `FixedString<20>`, the actual N of a
signature depends on the number of concatenations, not the logical string
length. Users cannot write `FixedString<expected_length>` -- they must use
`auto`.

**(U4) Error messages on unsupported types are cryptic.** When a user
accidentally passes `void` or a function type, they get a `static_assert`
deep inside `type_map.hpp`. The error message is reasonable ("void has no
layout; use void*") but the location is confusing -- it appears in a detail
header the user never included directly.

**(U5) Opaque registration requires `boost::typelayout` namespace.** Users
must open `namespace boost { namespace typelayout { ... } }` to register
opaque types. This is a standard C++ limitation for template specialization,
but the documentation should make this more prominent.

#### II.2 Two-Phase Pipeline (SOUND but friction-heavy)

The sig_export -> compat_check pipeline works correctly:

1. **Phase 1 (P2996 required):** Compile `sig_export` tool, run on each platform,
   generates `.sig.hpp` files.
2. **Phase 2 (C++17 only):** Include `.sig.hpp` files, use `compat_check.hpp`
   to compare.

**Friction points:**

**(U6) No end-to-end example in the repository.** There is no runnable example
that demonstrates the full pipeline: define types -> export on platform A ->
export on platform B -> compare. The test files test individual components
but not the pipeline.

**(U7) Platform detection is simplistic.** `platform_detect.hpp` uses `#ifdef`
chains. It works but produces names like `linux_x86_64` which are not
standardized. Users in embedded or cross-compilation environments may need
to override this.

**(U8) CompatReporter output is not machine-readable.** The `print_report()`
function produces human-readable text. There is no JSON or structured output
option for CI integration.

#### II.3 Documentation (INCOMPLETE)

| Document | Status | Assessment |
|---|---|---|
| `README.md` | Exists | Outdated -- still references old structure |
| `docs/migration-guide.md` | Complete | Good for existing users |
| `PROOFS.md` | Complete | Strong but needs restructure (see prior analysis) |
| API reference | **Missing** | No Doxygen or equivalent |
| Tutorial/quickstart | **Missing** | No "hello world" example |
| Architecture overview | **Missing** | No diagram of the two-phase pipeline |

**(U9) No quickstart guide.** A new user reading the README cannot write
working code within 5 minutes. They need to understand P2996, the two-layer
concept, and the export pipeline before they can produce useful output.

**(U10) No API reference.** Function signatures, return types, and preconditions
are documented only in source code comments. There is no standalone API
reference document.

### Part III: Summary

#### Correctness Score: 8.5/10

| Dimension | Score | Key Finding |
|---|---|---|
| Formal-implementation alignment | 9/10 | Tight after refine-formal-framework |
| Signature output correctness | 9/10 | All tested categories correct |
| Edge case handling | 7/10 | Empty fields, padding, bit-fields have caveats |
| Test coverage | 8/10 | ~80 tests, but gaps in negative/edge cases |

**No critical correctness bugs found.** The system does what it claims within
stated assumptions.

#### Usability Score: 6/10

| Dimension | Score | Key Finding |
|---|---|---|
| API clarity | 8/10 | Minimal, focused, well-named |
| Error experience | 5/10 | Deep static_assert, no compile-time diagnostics |
| Documentation | 4/10 | Missing quickstart, API ref, architecture overview |
| Pipeline friction | 6/10 | Works but no end-to-end example, no CI integration |
| Interoperability | 5/10 | FixedString-only output, no structured data |

**The library is correct but hard to adopt.** A new user faces a steep
learning curve with no guided entry point.

## Recommendations

### Correctness Improvements (Quick Wins)
1. Add negative compile-time tests (void, T[], function types)
2. Add member-pointer and nullptr_t signature tests
3. Document padding/memcmp caveat more prominently in user docs
4. Add a static_assert test that F6 ([[no_unique_address]]) match behavior
   is deterministic on current compiler

### Usability Improvements (High Impact)
5. Create a quickstart guide with a complete "hello world" example
6. Create an architecture overview diagram (two-phase pipeline)
7. Add a `constexpr const char*` accessor to FixedString (`.c_str()`)
8. Create an end-to-end pipeline example in `examples/`
9. Consider JSON output option for CompatReporter

### Correctness Improvements (Longer Term)
10. Implement a signature parser (constructive decode) for property-based testing
11. Consider `FixedString<N>` template parameter tightening to reduce compile
    memory (use actual string length instead of over-allocating)

## Impact

- Affected specs: `signature` (potential new requirements for documentation and tests)
- No code changes in this proposal -- analysis only
- Recommendations feed into future change proposals
