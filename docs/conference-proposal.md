# Conference Session Proposal

## Title

**Your Class is Lying to You: Zero-Copy Transport Verification with C++26 Reflection**

## Session Format

Preferred: Standard Session (60 minutes)
Also available as: Half Session (30 minutes)

## Session Level

Intermediate

## Abstract

This talk is a P2996 experience report: what happens when you build a
non-trivial consteval library on C++26 static reflection. It targets
systems programmers who ship binary data across process boundaries,
plugin interfaces, or network sockets — anyone who has asked: *which of
my types can I memcpy between which build targets without serialization?*

Standard C++ cannot answer this question. `sizeof` misses field offsets.
`offsetof` is conditionally-supported on non-standard-layout types
(private members, inheritance) and completely inapplicable to
bit-fields. Manual assertions are platform-local, O(n) per field, and
cannot compare across compilers. Serialization frameworks (protobuf,
FlatBuffers) solve a different problem — they add schema translation;
this talk is about verifying when you can skip it entirely.

Using a layout-signature library (TypeLayout) as the running case study,
the talk demonstrates eight P2996 reflection primitives generating a
compile-time **layout signature** — a deterministic string encoding
every field offset, size, alignment, and padding byte of any C++ type,
including classes with private members, inheritance hierarchies, and
bit-fields. Export signatures on each build target, feed them into a
compatibility matrix, and get a definitive per-type, per-platform-pair
answer: transfer-safe, layout mismatch, or pointer risk.

The talk opens with the compatibility matrix, then drills into *why*
each result is what it is — teaching reflection primitives, consteval
patterns, and safety classification by answering questions the matrix
raises. Attendees leave with concrete P2996 experience, reusable
consteval patterns, and a working open-source library they can try
immediately.

## Outline

### Part 1 — The Question and the Answer (10 min)

*"Six types, six targets. Which can I memcpy without serialization?"*

- Introduce six demo types — deliberately not flat POD structs, but
  real C++ code where `offsetof` is conditionally-supported or
  inapplicable:
  ```cpp
  class Timestamp {                          // private members
      int64_t seconds_; uint32_t nanos_;
  };
  class MessageHeader {                      // base class
      uint32_t magic_; uint16_t version_; uint16_t type_;
  };
  class TelemetryMsg : public MessageHeader { // inheritance + bit-fields
      Timestamp ts_; float values_[4];
      uint8_t quality_ : 4; uint8_t flags_ : 4;
  };
  class PlatformRiskyMsg : public MessageHeader { // platform-variant
      Timestamp ts_;
      long counter_;    // LP64: 8B, LLP64: 4B
      wchar_t label_[8]; // Linux/macOS: 4B, Windows: 2B
  };
  class EventRef : public MessageHeader {    // pointer member
      Timestamp ts_; EventRef* next_;
  };
  class BufferedReadings : public MessageHeader { // opaque container
      uint32_t sensor_id_;
      XVector<float> samples_; // offset_ptr-based, registered opaque
  };
  ```
- Six build targets spanning two data models and three compilers:
  ```
  x86_64_linux_gcc        LP64   long=8  wchar=4  ldbl=16
  x86_64_linux_clang      LP64   long=8  wchar=4  ldbl=16
  arm64_linux_gcc          LP64   long=8  wchar=4  ldbl=8(!)
  arm64_macos_clang        LP64   long=8  wchar=4  ldbl=8
  x86_64_windows_msvc      LLP64  long=4  wchar=2  ldbl=8
  x86_64_windows_clangcl   LLP64  long=4  wchar=2  ldbl=8
  ```
- Open with the compatibility matrix — the answer first, explanation
  later:
  ```
  Platforms: 6          ABI equivalence: {linux_gcc, linux_clang}
                                         {windows_msvc, windows_clangcl}

  Type              x86_linux  arm_linux  macOS   Windows   Safety
  ────────────────────────────────────────────────────────────────
  TelemetryMsg        MATCH      MATCH    MATCH    MATCH     **-
  PlatformRiskyMsg    MATCH      MATCH    MATCH    DIFFER    *--
  EventRef            MATCH      MATCH    MATCH    MATCH     *!-
  BufferedReadings    MATCH      MATCH    MATCH    MATCH     ---

  [DIFFER] PlatformRiskyMsg:
    linux_gcc:    ...@20:i64[s:8,a:8],@28:wchar[s:4,a:4]...
    windows_msvc: ...@20:i32[s:4,a:4],@24:wchar[s:2,a:2]...
                       ^^^                  ^^^
  ```
- The audience sees patterns immediately:
  - `linux_gcc == linux_clang` — same ABI, verified by signatures.
  - `TelemetryMsg` is safe everywhere — inheritance + bit-fields +
    private members, but all fixed-width at the leaf level.
  - `PlatformRiskyMsg` breaks on Windows — `long` and `wchar_t`.
  - `EventRef` matches everywhere but is flagged `*!-` — pointer.
  - `BufferedReadings` is `---` — opaque, requires user trust.
- Contrast with the status quo: "How would you verify this today?"
  ```cpp
  // Non-standard-layout: conditionally-supported, not portable:
  static_assert(offsetof(TelemetryMsg, ts_) == 8);  // may compile, not reliable
  // Bit-fields: completely impossible — no offsetof for bit-fields.
  // And even if offsetof worked: 6 targets x N fields = unmaintainable.
  ```
  With TypeLayout:
  ```cpp
  static_assert(
      get_layout_signature<SenderMsg>() ==
          get_layout_signature<ReceiverMsg>(),
      "Layout mismatch — see signature diff for details");
  ```

**Takeaway**: The question is "which types can I zero-copy between
which targets?" — and before P2996, standard C++ could not answer it
for real classes.

### Part 2 — What the Signatures Tell You (12 min)

*"Reading the diff."*

- Drill into `TelemetryMsg`'s signature to teach the format:
  ```
  [64-le]record[s:40,a:8]{
    @0:u32[s:4,a:4],               ← magic_    (from MessageHeader)
    @4:u16[s:2,a:2],               ← version_
    @6:u16[s:2,a:2],               ← type_
    @8:i64[s:8,a:8],               ← ts_.seconds_  (flattened!)
    @16:u32[s:4,a:4],              ← ts_.nanos_
    @20:array[s:16,a:4]<f32[s:4,a:4],4>,  ← values_[4]
    @36.0:bits<4,u8[s:1,a:1]>,    ← quality_ : 4
    @36.4:bits<4,u8[s:1,a:1]>     ← flags_ : 4
  }
  ```
  Key observations for the audience:
  - Base class fields are flattened — `magic_` is at `@0`, not nested
    inside a sub-record. Inheritance is erased; only byte identity
    remains.
  - `Timestamp` is flattened too — `seconds_` appears at `@8` because
    that's where it actually lives in `TelemetryMsg`'s layout.
  - Bit-fields have bit-level offsets: `@36.0` and `@36.4`.
  - **Private members are visible.** `access_context::unchecked()` is
    why this works — and why `offsetof` cannot compete.
- Drill into the `PlatformRiskyMsg` diff: same source, different
  signatures. Show the character-level divergence annotation and how
  `i64` vs `i32` (long) and `wchar[s:4]` vs `wchar[s:2]` tell you
  exactly which field broke.
- The gotcha: two types can have the same `sizeof` but different field
  offsets. Show a concrete example where inserting a field causes
  padding to shift, leaving `sizeof` unchanged. The signature catches
  it; `sizeof` doesn't.

**Takeaway**: Signatures are human-readable diffs. When they fail, you
can see exactly which field diverged and why — no debugger, no core
dump, no printf.

### Part 3 — How Reflection Builds Signatures (15 min)

*"Eight primitives, one library."*

- The complete P2996 API surface used:
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
- Code walkthrough: `TypeSignature<T>::calculate()` — recursive
  consteval traversal of fields and bases. Show the actual
  `layout_field_with_comma` function and how compile-time indexing
  replaces runtime iteration.
- Why `offset_of` is the key enabler: it returns the *compiler's own*
  layout decision. If the compiler says offset is 8, it is 8 — no
  external tool can disagree. This is authoritative in a way that
  `offsetof` (conditionally-supported on non-standard-layout,
  inapplicable to bit-fields) is not.
- Why `access_context::unchecked()` is essential: layout verification
  must see private members. Without it, the library cannot handle
  `TelemetryMsg` — and most real types have private members.
- **Dual-path padding validation** — a correctness pattern worth
  stealing:
  - Path 1: P2996 byte-coverage bitmap (`consteval`, marks bytes
    covered by fields using `offset_of` + `sizeof`).
  - Path 2: C++17 signature parser (parses the generated string,
    detects gaps between field offsets).
  - `static_assert(bitmap == parser)` — if these disagree, either
    the signature generator or the compiler has a bug.
- Friction points (honest report):
  - **Constexpr step limits**: `FixedString` concatenation is O(n^2).
    Types with >50 fields need `-fconstexpr-steps=5000000`.
  - **No `template for` yet**: all iteration is index-based template
    recursion. Show the before/after with `template for` (P1306).
  - **Bit-field cross-compiler divergence**: GCC and MSVC use different
    allocation unit strategies. P2996 correctly reports what each
    compiler chose — TypeLayout captures the difference, it doesn't
    resolve it.

**Takeaway**: P2996 provides exactly the right primitives for layout
analysis. The main adoption barrier is constexpr step limits — a
signal for tooling work, not an API deficiency.

### Part 4 — Beyond Layout Match: Transport Safety (13 min)

*"Matching layout is necessary but not sufficient."*

- Return to the matrix — two rows that need explaining:
  - `EventRef`: all MATCH, but `*!-`. Why?
  - `BufferedReadings`: all MATCH, but `---`. Why?
- **Byte-copy safety** (`is_byte_copy_safe_v<T>`):
  - Recursive compile-time check: T and every member/base must be
    trivially copyable and must not contain pointers.
  - `EventRef` fails: `next_` is a pointer — copying bytes produces
    a dangling pointer in the receiver's address space.
  - Formula: **signature match + byte-copy safe = safe to memcpy.**
- **Opaque types** — the `BufferedReadings` story:
  - Some types (offset_ptr containers, platform handles) are not
    fully analyzable by reflection. Register them as opaque with
    declared safety properties:
    ```cpp
    TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(XVector, "xvec")
    ```
    Signature: `O(xvec|24|8)<f32[s:4,a:4]>` — opaque shell with
    element type embedded. Safety recurses into the element type.
- **Runtime transfer verification** — the plugin `dlopen` pattern:
  ```cpp
  // Plugin exports its signature
  extern "C" const char* msg_layout() {
      static constexpr auto s = get_layout_signature<TelemetryMsg>();
      return s.c_str();
  }
  // Host verifies at load time
  if (!is_transfer_safe<TelemetryMsg>(plugin_get_sig()))
      return Error::ABIMismatch;
  ```
  Three checks in one call: byte-copy safe + no pointer risk +
  signature match.

**Takeaway**: Layout match tells you the bytes line up. Byte-copy
safety tells you whether copying those bytes is meaningful. Together
they answer: "can I memcpy this class to that endpoint?"

### Part 5 — The Cross-Platform Pipeline (5 min)

*"Export once, compare anywhere."*

- Two-phase workflow:
  - **Phase 1** (on each build target): compile a one-line exporter.
    ```cpp
    TYPELAYOUT_EXPORT_TYPES(
        TelemetryMsg, PlatformRiskyMsg, EventRef, BufferedReadings)
    ```
    Produces `x86_64_linux_gcc.sig.hpp` — constexpr signatures +
    ABI metadata (pointer size, sizeof(long), data model).
  - **Phase 2** (on any machine, C++17 only):
    ```cpp
    #include "sigs/x86_64_linux_gcc.sig.hpp"
    #include "sigs/arm64_macos_clang.sig.hpp"
    #include "sigs/x86_64_windows_msvc.sig.hpp"
    TYPELAYOUT_CHECK_COMPAT(
        x86_64_linux_gcc, arm64_macos_clang, x86_64_windows_msvc)
    ```
    Outputs the compatibility matrix from Part 1. Phase 2 does not
    require P2996 — compare signatures where the compiler is
    available, verify anywhere.
- ABI equivalence grouping: platforms with identical fingerprints
  (pointer size, sizeof(long), sizeof(wchar_t), sizeof(long double),
  max alignment) are grouped automatically. Same-ABI platforms are
  *likely* but not *guaranteed* to match — compiler-specific packing
  rules can still differ. The signature comparison is definitive.

**Takeaway**: You don't need P2996 on every machine. Export where you
have the compiler, verify anywhere.

### Q&A (5 min)

## Key Takeaways

1. The real question is *"which types can I zero-copy between which
   build targets?"* — and before P2996, standard C++ could not answer
   it for classes with private members, inheritance, or bit-fields.
2. `offsetof` is conditionally-supported on non-standard-layout types
   and inapplicable to bit-fields. P2996's `offset_of` works on any
   type the compiler can lay out.
3. Eight P2996 reflection primitives are sufficient to build a
   non-trivial consteval analysis system. The API is well-designed;
   the main friction is constexpr step limits.
4. Layout match alone is not enough. Byte-copy safety (no pointers,
   no non-relocatable opaque) completes the transport decision.
5. Export signatures where you have a P2996 compiler, compare them
   anywhere with C++17. Cross-compiler, cross-OS, cross-architecture.

## Target Audience

- **Primary**: C++ developers working with IPC, shared memory, network
  protocols, plugin systems, or binary file formats — anyone who has
  written `static_assert(sizeof(T) == N)` or wished `offsetof` worked
  on their real types.
- **Secondary**: Library authors exploring P2996 reflection as a
  building block for compile-time analysis.
- **Tertiary**: Compiler implementers and language designers interested
  in real-world P2996 implementation experience — especially
  `offset_of` on bit-fields and `access_context::unchecked()` across
  compilers.

## Prerequisites

- Familiarity with `sizeof`, `alignof`, `offsetof`
- Basic understanding of memory layout (padding, alignment)
- No prior P2996 or reflection knowledge required — explained in talk

## Why This Talk Matters

**P2996 is standardized, not theoretical.** The reflection API
(`offset_of`, `nonstatic_data_members_of`, `type_of`,
`access_context`) was voted into the C++26 working draft at the Sofia
meeting (June 2025). Bloomberg's Clang fork provides a
production-quality implementation today. Yet most public examples
remain toy demos (enum-to-string, JSON serialization sketches). This
talk presents a non-trivial consteval system — 200+ `static_assert`
validations across 17 type categories — that stress-tests the API
surface and reports what works, what's awkward, and what needs tooling
investment. It shows what reflection enables for **systems
programming**, not just metaprogramming convenience.

**Addresses a real problem.** Every team that ships binary data across
process boundaries, plugin interfaces, or network sockets has hit
layout mismatch bugs. The standard toolkit (`sizeof`/`offsetof`) fails
on real C++ types (private members, inheritance) and provides no
cross-platform story. The case study shows how P2996 replaces ad-hoc
assertions with a systematic, compiler-verified answer.

**Honest experience report.** This is not a sales pitch. The talk
reports what works well (`offset_of`, `access_context`,
`is_bit_field`), what's awkward (index-based recursion without
`template for`), and what's a real barrier (constexpr step limits).
It also reports cross-compiler differences (bit-field allocation
strategies, `long double` representation) that P2996 faithfully
captures but cannot resolve. Compiler teams and SG7 benefit from this
feedback.

**Immediately reproducible.** The case-study library is header-only,
open-source (Boost Software License), and zero dependencies. Attendees
can reproduce every example using the Bloomberg Clang P2996 fork today,
and the code will work unchanged when mainstream C++26 compilers ship.

## 30-Minute Condensed Version

| Part | Topic | Time |
|------|-------|------|
| 1 | The matrix — 6 types, 6 targets, which can zero-copy? | 5 min |
| 2 | Signature anatomy + the PlatformRiskyMsg diff | 8 min |
| 3 | Reflection: 8 primitives, access_context, friction | 10 min |
| 4 | Transport safety + opaque (condensed) | 4 min |
| 5 | Q&A | 3 min |

Parts 4 and 5 from the full version merge. Cross-platform pipeline
is shown as one slide with pointers to documentation.

## Supplementary Materials

*(Available upon acceptance; anonymous repository link provided to
reviewers on request.)*

- **Source code**: Complete library, header-only, Boost Software License
- **Test suite**: 12 tests (7 core P2996 + 5 tools), 200+ static_assert
- **Cross-platform demo**: Pre-generated signatures for 6 build targets
- **WG21 paper**: Implementation experience report drafted for SG7
