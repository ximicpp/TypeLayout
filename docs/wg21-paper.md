# Type Layout Verification with C++26 Reflection: Implementation Experience and Library Design

**Document**: PxxxxR0
**Date**: 2026-xx-xx
**Audience**: SG7 (Reflection), LEWG (Library Evolution)
**Reply-to**: Fanchen Su \<your-email\>
**Project**: ISO/IEC JTC1/SC22/WG21 — Programming Language C++

## Revision History

- R0 (2026-xx-xx): Initial publication.

## Abstract

We report on the design and implementation of TypeLayout, a header-only
library that uses C++26 static reflection ([P2996R13], adopted into C++26
at the Sofia meeting in June 2025) to generate compile-time type layout
signatures — deterministic strings encoding the complete byte-level memory
identity of C++ types. The library enables zero-cost, compile-time
verification of binary compatibility between types, replacing hand-written
`sizeof`/`offsetof` assertions with a single `static_assert`. This paper
presents the library as a non-trivial P2996 use case, reports
implementation experience with the reflection API, and identifies areas
where the reflection facilities work well and where they present friction.

## 1. Problem: Manual Layout Verification is Broken

Systems programming in C++ frequently requires that two modules agree on
the byte-level layout of shared data structures — for shared-memory IPC,
memory-mapped file I/O, network zero-copy protocols, and plugin ABIs.

The standard practice today is a battery of hand-written assertions:

```cpp
struct PacketHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t length;
    uint64_t timestamp;
};

static_assert(sizeof(PacketHeader) == 24);
static_assert(alignof(PacketHeader) == 8);
static_assert(offsetof(PacketHeader, magic)     == 0);
static_assert(offsetof(PacketHeader, version)   == 4);
static_assert(offsetof(PacketHeader, flags)     == 8);
static_assert(offsetof(PacketHeader, length)    == 12);
static_assert(offsetof(PacketHeader, timestamp) == 16);
```

This approach has four fundamental limitations:

1. **Incomplete.** `sizeof` alone does not guarantee correct field offsets.
   A struct may have the right total size but wrong internal padding.

2. **O(n) maintenance.** Every field change requires updating every
   corresponding assertion. In practice these fall out of date silently.

3. **Cannot compare two types.** The assertions verify one type against
   constants. They cannot express: "Is type A layout-compatible with
   type B?" — the fundamental IPC question.

4. **No cross-platform story.** `long` is 4 bytes on Windows (LLP64) and
   8 on Linux (LP64). Manual assertions are platform-local and cannot
   detect cross-platform divergence.

## 2. Solution: Compile-Time Layout Signatures via P2996

With P2996 reflection, we can automatically generate a complete layout
description at compile time. Consider two independently developed modules
that share a `PacketHeader` type:

```cpp
#include <boost/typelayout/typelayout.hpp>
using namespace boost::typelayout;

// Module A's definition
namespace sender {
    struct PacketHeader {
        uint32_t magic, version, flags, length;
        uint64_t timestamp;
    };
}

// Module B's definition (independently maintained)
namespace receiver {
    struct PacketHeader {
        uint32_t magic, version, flags, length;
        uint64_t timestamp;
    };
}

// The 7-line manual verification from section 1 reduces to:
static_assert(
    get_layout_signature<sender::PacketHeader>() ==
    get_layout_signature<receiver::PacketHeader>());
```

`get_layout_signature<T>()` is a `consteval` function that returns a
`FixedString<N>` — a compile-time string encoding the type's complete
byte-level identity. For `PacketHeader` on x86-64 little-endian:

```
[64-le]record[s:24,a:8]{@0:u32[s:4,a:4],@4:u32[s:4,a:4],@8:u32[s:4,a:4],@12:u32[s:4,a:4],@16:u64[s:8,a:8]}
  ^      ^         ^         ^
  |      |         |         field: type[s:size,a:alignment]
  |      |         @offset
  |      record[s:total_size,a:alignment]
  [pointer_width-endianness]
```

The signature captures: architecture prefix, record size and alignment,
every field's offset, type, size, and alignment. Inheritance is
recursively flattened. Field names are stripped. Padding, when present,
is implicit — visible as gaps between adjacent field offsets.

Two types with matching layout signatures have identical byte-level
representations. This is stricter than C++20's
`std::is_layout_compatible` ([meta.rel]), which checks common initial
sequence only — layout signatures verify byte-for-byte representation
equivalence, including for non-standard-layout types.

The signature is the single core concept. The library's other
capabilities — byte-copy safety checking, runtime transfer
verification, cross-platform comparison, safety classification — are
all derived from it (§4).

## 3. P2996 Reflection API Usage

TypeLayout uses a focused subset of P2996. This section documents exactly
which facilities are used and how, providing concrete implementation
experience for SG7.

### 3.1 Core API Surface Used

The entire library is built on two language operators and six
`std::meta` metafunctions:

```cpp
// Language operators
^^T                                          // reflection: type → meta::info
[:type_of(member):]                          // splice: meta::info → type

// Metafunctions
std::meta::nonstatic_data_members_of(
    ^^T, access_context::unchecked())        // → vector of member infos
std::meta::bases_of(
    ^^T, access_context::unchecked())        // → vector of base infos
std::meta::type_of(member)                   // → meta::info (type)
std::meta::offset_of(member)                 // → {.bytes, .bits}
std::meta::is_bit_field(member)              // → bool
std::meta::bit_size_of(member)               // → size_t
```

### 3.2 How They Are Combined

The signature generation algorithm recursively traverses a type's
structure via `consteval` template functions. Our implementation
uses compile-time indexing (template parameters) with fold expressions
to iterate over members. (P1306 `template for` would provide a
cleaner alternative; see §3.4.)

```cpp
// Simplified from actual implementation (error handling and edge cases omitted).
template <typename T, std::size_t Index, std::size_t OffsetAdj>
consteval auto layout_field_with_comma() noexcept {
    constexpr auto member =
        nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
    using FieldType = [:type_of(member):];

    if constexpr (is_bit_field(member)) {
        constexpr auto off = offset_of(member);           // {.bytes, .bits}
        constexpr std::size_t bwidth = bit_size_of(member);
        return FixedString{",@"} + to_fixed_string<off.bytes>()
             + FixedString{"."} + to_fixed_string<off.bits>()
             + FixedString{":bits<"} + to_fixed_string<bwidth>()
             + FixedString{","} + TypeSignature<FieldType>::calculate()
             + FixedString{">"};
    } else if constexpr (std::is_class_v<FieldType> && !std::is_empty_v<FieldType>) {
        constexpr std::size_t off = offset_of(member).bytes + OffsetAdj;
        return layout_all_prefixed<FieldType, off>();      // recurse, flatten
    } else {
        constexpr std::size_t off = offset_of(member).bytes + OffsetAdj;
        return FixedString{",@"} + to_fixed_string<off>()
             + FixedString{":"} + TypeSignature<FieldType>::calculate();
    }
}

// Fold expression drives recursion over all members (0, 1, ..., N-1)
template <typename T, std::size_t OffsetAdj, std::size_t... Is>
consteval auto layout_direct_fields(std::index_sequence<Is...>) noexcept {
    return (layout_field_with_comma<T, Is, OffsetAdj>() + ...);
}
```

The key insight: P2996's `offset_of` reports **compiler-authoritative**
offsets — the actual values the compiler uses for code generation. This
eliminates the error class where a verification tool disagrees with the
compiler about layout.

### 3.3 What Works Well

**`offset_of` with structured return.** The `{.bytes, .bits}` return
type elegantly handles both regular fields and bit-fields in a single
API. Our signature generation uses `.bytes` for regular fields and both
components for bit-fields — no special-casing needed.

**`access_context::unchecked()`.** Layout verification inherently needs
to inspect all members regardless of access specifiers. The
`unchecked()` access context is essential — without it, private fields
would be invisible, making the library useless for most real-world types.

**`bases_of` returning offsets.** Having `offset_of` work on base class
info objects (not just data members) simplifies inheritance flattening
considerably. We can treat bases and members uniformly.

**Indexable results.** The ability to index into the vector returned by
`nonstatic_data_members_of` and `bases_of` (e.g., `members[I]` where
`I` is a template parameter) enables compile-time-indexed recursion.
While P1306 expansion statements (`template for`) provide a cleaner
iteration model, indexing remains useful for fold-expression-driven
patterns like ours.

**`is_bit_field` + `bit_size_of`.** These allow complete bit-field
encoding without platform-specific workarounds. Before P2996, there was
no portable way to query bit-field width at compile time.

### 3.4 Points of Friction

**Constexpr step limits.** TypeLayout's signature generation accumulates
a `FixedString<N>` by repeated concatenation. Each `operator+` copies
the entire accumulated string, resulting in O(n^2) constexpr evaluation
steps for n fields. For types with >50 fields, we must raise
`-fconstexpr-steps` to 5,000,000. This is a practical barrier for
adoption.

This is not a P2996 issue per se, but P2996 applications that produce
compile-time strings will commonly hit this ceiling. The committee may
want to consider whether the default step limit is appropriate for the
reflection era, or whether compile-time string building deserves a more
efficient primitive (e.g., a `consteval` string builder with amortized
O(1) append).

**Index-based recursion vs `template for`.** Our implementation was
developed on the Bloomberg Clang P2996 fork, which predates the
adoption of P1306 expansion statements (`template for`). As a result,
all member/base iteration uses index-based template recursion:

```cpp
template <typename T, std::size_t I, std::size_t N>
consteval bool any_base_is_virtual() noexcept {
    if constexpr (I >= N) return false;
    else {
        constexpr auto base_info =
            bases_of(^^T, access_context::unchecked())[I];
        using BaseType = [:type_of(base_info):];
        if constexpr (is_virtual(base_info)) return true;
        else if constexpr (has_virtual_base<BaseType>()) return true;
        else return any_base_is_virtual<T, I + 1, N>();
    }
}
```

With `template for` (P1306, also adopted into C++26), this simplifies
substantially:

```cpp
template <typename T>
consteval bool has_virtual_base() noexcept {
    template for (constexpr auto base :
                  bases_of(^^T, access_context::unchecked())) {
        if (is_virtual(base)) return true;
        if (has_virtual_base<[:type_of(base):]>()) return true;
    }
    return false;
}
```

The index-based pattern is an artifact of compiler maturity, not an
API deficiency. We expect to migrate to `template for` once compiler
support is available, which will significantly reduce boilerplate
throughout the codebase.

## 4. Derived Capabilities

All downstream functionality builds on `get_layout_signature`. This
section briefly describes each derived capability.

**Byte-copy admission (`is_byte_copy_safe_v<T>`).**
Determines whether a type can be safely transported via `memcpy`.
Recursively checks that `T` and all its members/bases are trivially
copyable (or registered as relocatable opaque types). This predicate
is orthogonal to signatures — it checks a precondition for transport,
while signatures check layout equivalence. Used together: "signature
match + byte-copy safe = safe to memcpy."

**Runtime transfer verification (`is_transfer_safe<T>(remote_sig)`).**
Verifies layout compatibility when the remote endpoint's signature
arrives at runtime (e.g., plugin handshake over IPC). Generates the
local signature via `get_layout_signature<T>()`, compares it against
the runtime string, and additionally checks byte-copy safety. This
enables `extern "C"` signature exchange without shared headers.

**Cross-platform pipeline.**
Compares type layouts across different platforms (e.g., x86-64 Linux
vs ARM64 macOS). Phase 1 compiles a small P2996 exporter on each
target platform, producing a `.sig.hpp` header with signatures as
`constexpr` string literals. Phase 2 includes all generated headers
and compares signatures — no P2996 compiler needed. The exported
files are human-readable and version-controllable.

**Dual-path padding validation.**
An internal correctness mechanism. Two independent paths detect
padding: a P2996 `consteval` byte-coverage bitmap (using `offset_of`
to mark covered bytes) and a C++17 signature-string parser (detecting
gaps between field offsets). A `static_assert` enforces agreement,
implicitly validating the compiler's `offset_of` implementation.

**Safety classification (`classify<T>()`).**
Categorizes a type's transport safety into five ordered tiers by
parsing the signature string: `TrivialSafe` (no concerns),
`PaddingRisk` (uninitialized padding bytes), `PlatformVariant`
(platform-dependent types like `long`), `PointerRisk` (contains
pointers), `Opaque` (contains unanalyzable registered types).

## 5. Evaluation Summary

**Type coverage:** 17 categories tested including fixed-width integers,
floats, characters, pointers, enums, arrays, structs (simple and nested),
single/multiple/virtual inheritance, EBO, `[[no_unique_address]]`, unions,
bit-fields, and CV-qualified fields. 200+ `static_assert` statements pass.

**Effort reduction:** For a struct with n fields, manual verification
requires 2+n assertions. TypeLayout requires 1. For `PacketHeader`
(5 fields): 7 lines to 1 line, an 86% reduction with strictly stronger
guarantees.

**Compile-time overhead:** Signature length scales linearly (~18
characters per field). Constexpr steps scale O(n^2) due to string
concatenation. Types with <50 fields compile with default settings;
larger types need `-fconstexpr-steps=5000000`.

**Comparison with existing approaches:**

| Property | sizeof/offsetof | Boost.PFR | ABI Checker | Protobuf | TypeLayout |
|----------|:---:|:---:|:---:|:---:|:---:|
| Compile-time (C++) | Yes | Yes | No | No* | Yes |
| Complete (offsets+types) | No | No | Yes | Yes | Yes |
| Automatic | No | Yes | Yes | Yes | Yes |
| Zero overhead | Yes | Yes | N/A | No | Yes |
| Native C++ structs | Yes | Yes | Yes | No | Yes |
| Cross-platform | No | No | Yes | Yes | Yes |

*Protobuf verification occurs at schema compilation / code generation
time, not during C++ compilation.

TypeLayout is the first system to achieve all six properties
simultaneously.

## 6. Future Directions

**Standardization potential.** The core operation — "generate a
canonical layout description from a type's reflection" — may be
useful enough to warrant a standard library facility. A minimal
version could provide a `consteval` predicate:

```cpp
namespace std::meta {
    // Do T and U have identical byte-level representations?
    // (Stricter than std::is_layout_compatible, which checks
    // common initial sequence only.)
    consteval bool layout_identical(info t, info u);
}
```

This would enable `static_assert(layout_identical(^^A, ^^B))`
without a third-party library. Note the distinction from C++20's
`std::is_layout_compatible`: the existing trait checks the common
initial sequence guarantee per [basic.types.general], while
`layout_identical` would verify byte-for-byte representation
equivalence. We are not proposing this in this paper, but present
the use case for committee consideration.

**Efficient compile-time strings.** A standard `consteval` string
builder with O(1) amortized append would benefit not only TypeLayout
but any P2996 application that generates code or signatures as strings
at compile time.

## 7. Conclusion

TypeLayout demonstrates that P2996 reflection enables a category of
library that was previously impossible in standard C++: fully automatic,
compile-time, zero-overhead verification of type memory layout. The
implementation uses only eight P2996 primitives (two language operators
and six metafunctions) and has been validated with 200+ compile-time
assertions across 17 type categories.

Our implementation experience is overwhelmingly positive. The P2996 API
is well-designed for this use case: `offset_of` provides
compiler-authoritative values, `access_context::unchecked()` enables
inspection of all members, and bit-field support is complete. The
primary friction point is constexpr step limits for types with many
fields — a tooling concern rather than an API design issue.

We believe compile-time layout verification will become standard
practice in systems C++ as P2996 implementations mature, and present
this work both as a practical library and as evidence that the
reflection facilities adopted in C++26 are sufficient for non-trivial
library construction.

**Availability.** TypeLayout is open-source under the Boost Software
License 1.0, available at [https://github.com/ximicpp/TypeLayout](https://github.com/ximicpp/TypeLayout).

## References

- [P2996R13] Childers, Dimov, Katz, Revzin, Sutton, Vali, Vandevoorde.
  "Reflection for C++26." ISO/IEC JTC1/SC22/WG21, 2025.
- [P1306R2] Sutton, Childers, Vandevoorde. "Expansion Statements."
  ISO/IEC JTC1/SC22/WG21, 2024.
- [P3687R1] "Final Adjustments to C++26 Reflection."
  ISO/IEC JTC1/SC22/WG21, 2025.
- [Boost.PFR] Polukhin. "Boost.PFR: Precise and Flat Reflection." 2020.
- [Boost.Describe] Dimov. "Boost.Describe." 2021.
- [ABI Checker] Ponomarenko. "ABI Compliance Checker." 2009.

## Acknowledgments

TypeLayout was developed using the Bloomberg Clang P2996 fork. We thank
the P2996 authors for their work on static reflection for C++26 and the
Bloomberg team for providing the reference implementation.
