# Change: Audit Custom Implementations for Standard/Mainstream Alternatives

## Why

TypeLayout contains several custom utility implementations. Before the library
matures for Boost submission, we should identify which of these have standard
or widely-adopted alternatives, and decide whether to adopt, align with, or
deliberately retain the custom implementations.

This is a pure analysis change -- no code modifications.

## What Changes

Analysis of 9 custom components against standard/mainstream alternatives.

## Analysis Results

### 1. FixedString<N> -- [DONE] Aligned with P2484

Already addressed in `align-fixed-string-p2484`. N now means character count.
When `std::basic_fixed_string` is standardized, a drop-in replacement will
be straightforward.

**Verdict**: [DONE] -- No further action needed.

---

### 2. Endianness Detection (config.hpp) -- std::endian (C++20)

TypeLayout's custom `TYPELAYOUT_LITTLE_ENDIAN` macro replicates what
`std::endian` (C++20, `<bit>`) already provides. The library does use
`std::endian` when available, but falls back to compiler-specific macros.

**Standard alternative**: `std::endian::native == std::endian::little` (C++20)

**Assessment**: The fallback chain is necessary because:
- The library targets C++26 but the export tools need C++17 compatibility
- `<bit>` may not be available on all toolchains
- The macro is evaluated at preprocessor time, not constexpr time

**Verdict**: [KEEP] -- The fallback chain is correct and necessary.
Consider using `if constexpr` with `std::endian` in the signature engine
(which requires C++26 anyway) and keeping the macro only for C++17 tool code.

---

### 3. FOR_EACH Macro (tools/detail/foreach.hpp) -- Boost.PP / P1306

A 65-line preprocessor FOR_EACH implementing variadic macro iteration
(up to 32 arguments). Used by `TYPELAYOUT_EXPORT_TYPES` and
`TYPELAYOUT_REGISTER_TYPES`.

**Mainstream alternatives**:
- **Boost.Preprocessor** `BOOST_PP_SEQ_FOR_EACH` -- the gold standard,
  handles arbitrary argument counts, well-tested across all compilers.
- **P1306 (pack indexing)** + C++26 fold expressions -- could eliminate
  the macro entirely with template parameter packs.

**Assessment**:
- Adding a Boost.PP dependency for one macro is heavyweight.
- A C++26 template-based approach using fold expressions and P2996 reflection
  could replace the macro: `(exporter.add<Ts>(name_of<Ts>()), ...)`
- The current macro works, is self-contained, and has no bugs.

**Verdict**: [KEEP for now] -- The macro is isolated in `detail/`. When P2996
matures, the EXPORT/REGISTER macros should become template-based APIs,
eliminating the FOR_EACH entirely.

---

### 4. Platform Detection (tools/platform_detect.hpp) -- Boost.Predef

A custom set of macros detecting arch/OS/compiler via preprocessor defines.

**Mainstream alternative**: **Boost.Predef** -- a Boost library specifically
designed for this purpose. Covers 20+ architectures, 15+ OSes, 10+ compilers
with version detection and normalized identifiers.

**Assessment**:
- Boost.Predef is header-only, lightweight, and the de facto standard in Boost.
- TypeLayout's custom detection covers 8 arch, 6 OS, 3 compilers -- adequate
  but less comprehensive.
- If TypeLayout is targeting Boost submission, using Boost.Predef is idiomatic.
- However, `platform_detect.hpp` is only used in the tools layer (sig_export),
  not in the core signature engine.

**Verdict**: [CONSIDER] -- If Boost submission is a goal, migrate to
Boost.Predef. Otherwise, the current implementation is sufficient.
Priority: low (tools-only, not user-facing).

---

### 5. to_fixed_string (integer-to-string) -- std::to_chars / P2291

A constexpr integer-to-FixedString converter.

**Standard alternatives**:
- `std::to_chars` (C++17) -- not constexpr until C++26 P2291.
- `std::format` (C++20) -- not constexpr.
- P2291 proposes constexpr `std::to_chars` for C++26.

**Assessment**:
- `to_fixed_string` is `consteval` and produces a `FixedString<20>`, which
  is directly concatenatable with other FixedStrings.
- Even if `std::to_chars` becomes constexpr, it writes to a `char[]` buffer,
  not a `FixedString`. A wrapper would still be needed.
- The function is 30 lines, correct, and well-tested.

**Verdict**: [KEEP] -- No standard replacement produces FixedString directly.
The function is essential to the library's compile-time string building.

---

### 6. always_false<T...> -- std::always_false (C++26 P2593)

A helper for `static_assert(false)` in `if constexpr` branches.

**Standard alternative**: `std::always_false` proposed in P2593 for C++26.
Not yet standardized in any shipping compiler.

**Assessment**:
- Trivially replaceable when standardized (1-line alias).
- Used in 4 places in `detail/type_map.hpp`.

**Verdict**: [KEEP, add TODO] -- Add a comment noting P2593. Replace when
`std::always_false` is available in the target compiler.

---

### 7. qualified_name_for (detail/reflect.hpp) -- P2996 qualified_name_of

A custom function that walks `parent_of` chains to build qualified names,
because the Bloomberg P2996 fork lacks `qualified_name_of`.

**Standard alternative**: `std::meta::qualified_name_of` -- defined in P2996
but not implemented in the Bloomberg experimental fork.

**Assessment**:
- When the P2996 toolchain implements `qualified_name_of`, this function
  becomes a one-liner: `return FixedString{qualified_name_of(R)}`.
- The current implementation is correct but adds ~20 lines of complexity.

**Verdict**: [KEEP, add TODO] -- Replace when `qualified_name_of` ships.

---

### 8. Compatibility Reporter (tools/compat_check.hpp) -- No standard alternative

A custom runtime reporting tool that compares signatures across platforms
and generates a formatted compatibility matrix.

**Assessment**: This is domain-specific functionality. No standard or
mainstream library provides cross-platform type layout comparison. This is
a unique value proposition of TypeLayout.

**Verdict**: [KEEP] -- Core library feature, no alternative exists.

---

### 9. SigExporter (tools/sig_export.hpp) -- No standard alternative

A code generator that writes `.sig.hpp` headers containing constexpr
signature data. Domain-specific to TypeLayout's workflow.

**Verdict**: [KEEP] -- Core library feature, no alternative exists.

---

### 10. is_byte_element<T>() -- No standard alternative

A consteval predicate checking if T is a "byte-like" type (char, signed char,
unsigned char, int8_t, uint8_t, std::byte, char8_t). Used to collapse byte
arrays into the compact `bytes[s:N,a:1]` signature form.

**Standard alternatives**:
- C++17 has no `std::is_byte_like` trait.
- C++20 concepts could express this but no standard concept exists.
- The closest standard facility is `std::has_unique_object_representations`,
  but it does not distinguish byte-sized types from other trivial types.

**Assessment**: This is a domain-specific predicate -- "types that are
interchangeable as raw bytes in a memory layout." No standard equivalent.

**Verdict**: [KEEP] -- Domain-specific, correct, 4 lines.

---

### 11. has_opaque_signature<T, Mode> -- No standard alternative

A concept detecting whether a type has a custom opaque TypeSignature
specialization (via the `is_opaque` static member).

**Assessment**: This is a library-internal detection mechanism for the opaque
registration system. No standard equivalent exists or makes sense here.

**Verdict**: [KEEP] -- Library-internal concept, 3 lines.

---

### 12. format_size_align() -- No standard alternative

A consteval helper that formats `"name[s:SIZE,a:ALIGN]"` from a name string
and numeric size/align values. Used 12+ times in type_map.hpp.

**Assessment**: This is a domain-specific string formatting helper. Even with
C++26 constexpr `std::format`, the output must be a `FixedString` for
compile-time concatenation. No replacement possible.

**Verdict**: [KEEP] -- Essential signature formatting utility.

---

### 13. classify_safety() -- No standard alternative

Runtime function scanning a layout signature string for pointers, bit-fields,
and platform-dependent types to classify cross-platform safety level.

**Assessment**: Domain-specific analysis logic. No standard or library
equivalent -- this is unique to TypeLayout's compatibility checking workflow.

**Verdict**: [KEEP] -- Core library feature.

---

### 14. Endianness in Signature (get_arch_prefix) vs config.hpp

`get_arch_prefix()` in `signature.hpp` uses the `TYPELAYOUT_LITTLE_ENDIAN`
macro to embed `[64-le]` / `[64-be]` in signatures.

**Assessment**: In C++26 (which the signature engine requires), `std::endian`
is always available. The macro indirection is unnecessary here -- we could
use `if constexpr (std::endian::native == std::endian::little)` directly.
The macro is still needed for C++17 tool code.

**Verdict**: [CONSIDER] -- Signature engine could use `std::endian` directly,
removing dependency on config.hpp macro for the core path. Low priority.

---

## Summary Table

| Component | Alternative | Verdict | Priority |
|-----------|-----------|---------|----------|
| FixedString<N> | P2484 | [DONE] | -- |
| Endianness detection | std::endian (C++20) | [KEEP] | -- |
| FOR_EACH macro | Boost.PP / C++26 folds | [KEEP for now] | Low |
| Platform detection | Boost.Predef | [CONSIDER] | Low |
| to_fixed_string | P2291 constexpr to_chars | [KEEP] | -- |
| always_false | P2593 | [KEEP, add TODO] | Trivial |
| qualified_name_for | P2996 qualified_name_of | [KEEP, add TODO] | Trivial |
| CompatReporter | None | [KEEP] | -- |
| SigExporter | None | [KEEP] | -- |
| is_byte_element<T> | None | [KEEP] | -- |
| has_opaque_signature | None | [KEEP] | -- |
| format_size_align | None | [KEEP] | -- |
| classify_safety | None | [KEEP] | -- |
| get_arch_prefix macro dep | std::endian direct | [CONSIDER] | Low |

## Actionable Items

1. **[Trivial]** Add `// TODO(P2593): replace with std::always_false when available`
2. **[Trivial]** Add `// TODO(P2996): replace with qualified_name_of when available`
3. **[Low]** Evaluate Boost.Predef adoption if Boost submission proceeds
4. **[Future]** Replace FOR_EACH + EXPORT/REGISTER macros with template-based
   API when P2996 template argument reflection is stable

## Impact

- No code changes -- this is a pure analysis document.
- Affected specs: none
