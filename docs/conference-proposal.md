# Conference Session Proposal

## Title

**Your Struct is Lying to You: Compile-Time Layout Verification with C++26 Reflection**

## Session Format

Preferred: Standard Session (60 minutes)
Also available as: Half Session (30 minutes)

## Session Level

Intermediate

## Abstract

If you ship C++ across shared-memory boundaries, network sockets,
memory-mapped files, or plugin interfaces, you have written
`static_assert(sizeof(T) == N)` — and watched it break. Manual
`sizeof`/`offsetof` assertions are incomplete (they miss field
offsets), fragile (O(n) maintenance per field), and local (no
cross-platform story). This talk shows how C++26 static reflection
(P2996) eliminates this entire problem class.

TypeLayout is a header-only library that uses eight P2996 primitives
to generate a compile-time **layout signature** — a deterministic
string encoding every field offset, size, alignment, and padding byte
of any C++ type. Two types with matching signatures have identical
byte-level representations. The 7-line assertion battery from
`sizeof`/`offsetof` reduces to one `static_assert`. The talk walks
through the real implementation, demonstrates live cross-platform
comparisons, and reports honestly on what works and what doesn't in
the P2996 API.

Attendees leave with: (1) a ready-to-use library for compile-time
layout verification, (2) a concrete understanding of the P2996
reflection primitives and their practical limits, and (3) patterns
for building their own reflection-based compile-time analysis tools.

## Outline

### Part 1 — The Problem (8 min)

*"Your struct is lying to you."*

- **Live demo**: compile a struct containing `long` and `wchar_t` on
  x86-64 Linux, ARM64 macOS, and x64 Windows. Show three different
  layouts from identical source (`long` is 8 bytes on LP64, 4 on
  LLP64; `wchar_t` is 4 bytes on Linux/macOS, 2 on Windows).
- Why the standard practice fails:
  - `sizeof` can stay the same even when field offsets change
    (e.g., a field is inserted and padding shifts to compensate)
  - `offsetof` assertions scale O(n) and rot silently
  - No way to express "do type A and type B have the same layout?"
  - `long` is 8 bytes on Linux (LP64) and 4 on Windows (LLP64) —
    manual assertions are platform-local
- Real failure modes: IPC data corruption, plugin ABI crashes,
  memory-mapped file incompatibility across platforms.

**Takeaway**: The question you need answered is "do these two types
have identical byte representations?" — and no existing C++ facility
answers it.

### Part 2 — Layout Signatures (15 min)

*"One line replaces seven."*

- The core idea: use P2996 to query every field's compiler-
  authoritative offset, type, and size, then encode the result as a
  `consteval` string.
- The complete public API:
  ```cpp
  // Generate
  constexpr auto sig = get_layout_signature<PacketHeader>();
  // Compare
  static_assert(get_layout_signature<A>() == get_layout_signature<B>());
  // Inspect
  std::cout << sig << "\n";
  ```
- Signature anatomy — how to read the output:
  ```
  [64-le]record[s:24,a:8]{@0:u32[s:4,a:4],@4:u32[s:4,a:4],
    @8:u32[s:4,a:4],@12:u32[s:4,a:4],@16:u64[s:8,a:8]}
  ```
  Architecture prefix, record size/alignment, field offset + type +
  size + alignment. Padding is implicit — visible as gaps between
  offsets.
- How the signature handles complex types:
  - **Inheritance**: recursively flattened — base class fields appear
    at their actual offsets, no intermediate `record{}` wrapper.
  - **Bit-fields**: `@offset.bitoff:bits<width,type>` with
    compiler-reported bit offset and width.
  - **Arrays**: `array[s:N,a:A]<elem,count>` with recursive element
    signatures; byte-element arrays collapse to `bytes[s:N,a:1]`.
  - **Unions**: `union[s:N,a:A]{...}` — members listed but **not**
    flattened (they overlap in memory).
  - **Enums**: `enum[s:N,a:A]<underlying_type>`.
  - **Empty bases / `[[no_unique_address]]`**: `s:0` in embedded
    context, correctly handling EBO overlap.
- `FixedString<N>`: the compile-time string type. N = exact content
  length. Supports `operator==`, `operator+`, `find()`, `contains()`.
  CTAD deduction from string literals.

**Takeaway**: One `static_assert` replaces your entire
`sizeof`/`offsetof` battery. The signature is human-readable — when
it fails, you can read it to see exactly what diverged.

### Part 3 — How Reflection Drives Signature Generation (15 min)

*"Eight primitives, one library."*

- The complete P2996 API surface used by TypeLayout:
  ```
  ^^T                              — reflect a type
  [:type_of(member):]              — splice back to a type
  nonstatic_data_members_of(^^T, access_context::unchecked())
  bases_of(^^T, access_context::unchecked())
  type_of(member)
  offset_of(member)                — returns {.bytes, .bits}
  is_bit_field(member)
  bit_size_of(member)
  ```
- Code walkthrough: how `TypeSignature<T>::calculate()` recurses
  through members using compile-time indexing + fold expressions.
  Show the actual `layout_field_with_comma` function.
- Why `offset_of` is the key enabler: it returns the compiler's own
  layout decisions. If the compiler says offset is 8, it is 8 — no
  external tool can disagree.
- Why `access_context::unchecked()` is essential: layout verification
  must see private members, or the library is useless for real types.
- **Dual-path padding validation** — a correctness pattern worth
  stealing:
  - Path 1: P2996 `consteval` byte-coverage bitmap (mark which bytes
    are covered by fields using `offset_of` + `sizeof`).
  - Path 2: C++17 signature parser (parse the generated string,
    detect gaps between field offsets).
  - `static_assert(bitmap_result == parser_result)` — if these
    disagree, either the signature generator or the compiler's
    `offset_of` has a bug.
- Friction points:
  - **Constexpr step limits**: `FixedString` concatenation is O(n^2).
    Types with >50 fields need `-fconstexpr-steps=5000000`.
  - **No `template for` yet**: Bloomberg Clang P2996 fork predates
    P1306 adoption. All iteration is index-based template recursion.
    Show the before/after with `template for`.

**Takeaway**: P2996 provides exactly the right primitives for layout
analysis. The main adoption barrier is constexpr step limits for
string-heavy applications — a signal for tooling work, not an API
deficiency.

### Part 4 — Beyond Signatures: Safety and Transport (12 min)

*"Matching layout is necessary but not sufficient."*

- **Byte-copy admission** (`is_byte_copy_safe_v<T>`):
  - Recursive compile-time check: T and all members/bases must be
    trivially copyable (or registered relocatable opaque).
  - Rejects polymorphic types (vtable pointer), pointer members
    (dangling after copy), reference members.
  - Formula: "signature match + byte-copy safe = safe to memcpy."
- **Five-tier safety classification** (`classify_signature()`):
  - `TrivialSafe` — fixed-width scalars only, safe everywhere.
  - `PaddingRisk` — has padding bytes (info-leak risk, not a blocker).
  - `PlatformVariant` — contains `long`, `wchar_t`, `long double`,
    or bit-fields (size varies across platforms).
  - `PointerRisk` — contains pointers/references (blocks transport).
  - `Opaque` — contains registered unanalyzable types.
  - Show how `layout_traits<T>` computes `has_pointer`, `has_padding`,
    `has_opaque` at compile time, and how the runtime classifier
    derives the same result from the signature string alone.
- **Runtime transfer verification** (`is_transfer_safe<T>(sig)`):
  - Use case: plugin exports signature via `extern "C"`, host verifies
    at `dlopen` time.
  - Three checks: byte-copy safe + no pointer risk + signature match.
- **Opaque type registration**:
  - Problem: some types are reflectable but should not be recursively
    flattened — their internal layout is an implementation detail
    (e.g., `boost::interprocess::offset_ptr` stores an offset, not a
    pointer, but recursive analysis would misclassify it).
  - `TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, HasPointer)` — registers
    a type as an atomic blob with user-declared safety properties.
    Signature: `O(tag|size|align)`.
  - Relocatable variants for non-trivially-copyable containers.

**Takeaway**: Layout match tells you the bytes line up. Safety
classification tells you whether copying those bytes is meaningful.
Together they answer: "can I memcpy this struct to that endpoint?"

### Part 5 — Cross-Platform Verification (5 min)

*"Export once, compare anywhere."*

- Two-phase pipeline:
  - Phase 1: compile a small P2996 exporter on each target platform →
    produces a `.sig.hpp` header with signatures as `constexpr` string
    literals + ABI metadata (pointer size, sizeof(long), data model).
  - Phase 2: include all `.sig.hpp` headers, run `CompatReporter` →
    produces a human-readable compatibility matrix.
- `TYPELAYOUT_EXPORT_TYPES(PacketHeader, SensorRecord)` — one-line
  macro generates `main()`.
- ABI equivalence grouping: platforms with identical ABI fingerprints
  are grouped automatically.
- Live demo: 3-platform compatibility report with diff annotations.

**Takeaway**: You don't need P2996 on every machine. Export signatures
where you have the compiler, compare them anywhere.

### Q&A (5 min)

## Key Takeaways

1. `sizeof`/`offsetof` assertions are incomplete and unmaintainable.
   Layout signatures automate them with strictly stronger guarantees.
2. C++26 reflection (P2996) enables compile-time safety tools that
   were previously impossible in standard C++.
3. Eight reflection primitives are sufficient to build a non-trivial
   library — the API is well-designed for this use case.
4. Layout match alone is not enough — byte-copy safety and safety
   classification complete the transport decision.
5. Cross-platform struct verification is now possible without
   serialization frameworks or external tooling.

## Target Audience

- **Primary**: C++ developers working with IPC, shared memory, network
  protocols, plugin systems, or binary file formats — anyone who has
  written `static_assert(sizeof(T) == N)`.
- **Secondary**: Library authors exploring P2996 reflection as a
  building block for compile-time analysis.
- **Tertiary**: Compiler implementers and language designers interested
  in real-world P2996 implementation experience.

## Prerequisites

- Familiarity with `sizeof`, `alignof`, `offsetof`
- Basic understanding of memory layout (padding, alignment)
- No prior P2996 or reflection knowledge required — explained in talk

## Why This Talk Matters

**P2996 is real, not theoretical.** C++26 static reflection was
adopted at the Sofia meeting (June 2025). Most examples shown so far
are toy demos (enum-to-string, JSON serialization sketches). This
talk presents a complete, tested library — 200+ `static_assert`
validations across 17 type categories — built entirely on P2996. It
shows what reflection actually enables for systems programming.

**Honest experience report.** This is not a sales pitch. The talk
reports what works well (`offset_of`, `access_context`,
`is_bit_field`), what's awkward (index-based recursion without
`template for`), and what's a real barrier (constexpr step limits).
Compiler teams and SG7 benefit from this feedback.

**Immediately actionable.** The library is header-only, open-source,
zero dependencies. Attendees can integrate it into their codebase
tonight using the Bloomberg Clang P2996 fork, and it will work
unchanged when mainstream C++26 compilers ship.

## 30-Minute Condensed Version

| Part | Topic | Time |
|------|-------|------|
| 1 | The Problem — live demo + why sizeof fails | 5 min |
| 2 | Layout Signatures — core concept + API | 10 min |
| 3 | Reflection usage — eight primitives, friction | 8 min |
| 4 | Safety + cross-platform (condensed) | 4 min |
| 5 | Q&A | 3 min |

Parts 4 and 5 from the full version merge into a single condensed
section covering safety classification and cross-platform pipeline
at overview level, with pointers to documentation.

## Supplementary Materials

*(Available upon acceptance; anonymous repository link provided to
reviewers on request.)*

- **Source code**: Complete library, header-only, Boost Software License
- **Test suite**: 12 tests (7 core P2996 + 5 tools), 200+ static_assert
- **Cross-platform demo**: Pre-generated signatures for 3 platforms
- **WG21 paper**: Implementation experience report drafted for SG7
