# Conference Session Proposal

## Title

**Your Class is Lying to You: Cross-Platform Layout Verification with C++26 Reflection**

## Session Format

Preferred: Standard Session (60 minutes)
Also available as: Half Session (30 minutes)

## Session Level

Intermediate to Advanced

## Abstract

This talk is a P2996 experience report: what happens when you build a
non-trivial consteval library on C++26 static reflection — and what
you learn about the API surface, its limits, and cross-compiler
behavior along the way.

The driving question is practical: *which of my types can I safely
memcpy between which build targets without serialization?* In
domains where serialization overhead is unacceptable — game engines,
high-frequency trading, HPC, shared-memory IPC — this question
matters for every message type on every target platform. Standard
C++ alone cannot answer this portably at compile time. `sizeof` misses
field offsets. `offsetof` is conditionally-supported on
non-standard-layout types (private members, inheritance). Manual assertions are platform-local and cannot compare across compilers.

The running case study is TypeLayout, a layout-signature library that
uses a small set of reflection primitives to generate a compile-time
**layout signature** — a deterministic string encoding every field offset, size,
alignment, and padding gap of any C++ type, including classes with
private members and inheritance hierarchies. Export
signatures where you have a P2996 compiler, verify compatibility
anywhere without P2996 — and get a definitive answer for every
type-target pair: transfer-safe, layout mismatch, or pointer risk.

## Outline

### Part 1 — The Question and the Answer (8 min)

*"Six types, six targets. Which can I memcpy without serialization?"*

- Introduce six demo types (four top-level message types plus two
  building blocks — `Timestamp` and `MessageHeader` — that appear
  flattened inside the others). Not trivial standard-layout structs, but
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
- Open with the compatibility matrix (six build targets, two data
  models, three compilers) — the answer first, explanation
  later:
  ```
  Build targets: 6     ABI equivalence: {x86_linux_gcc, x86_linux_clang, arm64_linux_gcc}
                                         {win_msvc, win_clangcl}

  Type              x86_linux  arm_linux  macOS   Windows   Safety
  ────────────────────────────────────────────────────────────────
  TelemetryMsg        MATCH      MATCH    MATCH    MATCH     Safe
  PlatformRiskyMsg    MATCH      MATCH    MATCH    DIFFER    Safe
  EventRef            MATCH      MATCH    MATCH    MATCH     Pointer!
  BufferedReadings    MATCH      MATCH    MATCH    MATCH     Opaque

  Safety: Safe / Pointer! / Opaque.  MATCH + Safe = transfer-safe.

  [DIFFER] PlatformRiskyMsg:
    linux_gcc:    ...@24:i64[s:8,a:8],@32:array[s:32,a:4]<wchar[s:4,a:4],8>...
    windows_msvc: ...@24:i32[s:4,a:4],@28:array[s:16,a:2]<wchar[s:2,a:2],8>...
                       ^^^                  ^^^
  ```

### Part 2 — What the Signatures Tell You (12 min)

*"Reading the diff."*

- Drill into `TelemetryMsg`'s signature to teach the format:
  ```
  [64-le]record[s:48,a:8]{
    @0:u32[s:4,a:4],               // magic_    (from MessageHeader)
    @4:u16[s:2,a:2],               // version_
    @6:u16[s:2,a:2],               // type_
    @8:i64[s:8,a:8],               // ts_.seconds_  (flattened!)
    @16:u32[s:4,a:4],              // ts_.nanos_
    @24:array[s:16,a:4]<f32[s:4,a:4],4>,  // values_[4]
    @40.0:bits<4,u8[s:1,a:1]>,    // quality_ : 4
    @40.4:bits<4,u8[s:1,a:1]>     // flags_ : 4
  }
  ```
  Key observations: base class and nested struct fields are flattened
  (inheritance erased, only byte identity remains); private members
  are visible via `access_context::unchecked()`.
- Drill into the `PlatformRiskyMsg` diff: `i64` vs `i32` (long) and
  `wchar[s:4]` vs `wchar[s:2]` pinpoint exactly which field broke.

### Part 3 — How Reflection Builds Signatures (15 min, dense)

*"Reflection patterns for layout analysis."*

- The P2996 primitives central to signature generation:
  ```
  ^^T                              — reflect a type
  [:type_of(member):]              — splice back to a type
  nonstatic_data_members_of(^^T, access_context::unchecked())
  bases_of(^^T, access_context::unchecked())
  type_of(member)
  size_of(type)                    — size in bytes (reflection version of sizeof)
  offset_of(member)                — returns {.bytes, .bits}
  is_bit_field(member)
  bit_size_of(member)
  ```
- Show that the core pattern works **without a library** — a minimal
  consteval layout check in ~10 lines of raw P2996:
  ```cpp
  // Simplified: does not recurse into bases or nested structs.
  template <typename T, typename U>
  consteval bool layout_matches() {
      constexpr auto a = nonstatic_data_members_of(^^T, access_context::unchecked());
      constexpr auto b = nonstatic_data_members_of(^^U, access_context::unchecked());
      if (a.size() != b.size()) return false;
      for (size_t i = 0; i < a.size(); ++i)
          if (offset_of(a[i]) != offset_of(b[i])
              || size_of(type_of(a[i])) != size_of(type_of(b[i])))
              return false;
      return sizeof(T) == sizeof(U);
  }
  ```
- Friction points: constexpr step limits on large types, missing
  expansion statements (P1306).

### Part 4 — Beyond Layout Match: Transport Safety (13 min)

*"Matching layout is necessary but not sufficient."*

- Return to the matrix — two rows that need explaining:
  - `EventRef`: all MATCH, but `Pointer!`. Why?
  - `BufferedReadings`: all MATCH, but `Opaque`. Why?
- **Byte-copy safety** (`is_byte_copy_safe_v<T>`):
  - Recursive compile-time check: T and every member/base must be
    trivially copyable and must not contain pointers.
  - `EventRef` fails: `next_` is a pointer — copying bytes produces
    a dangling pointer in the receiver's address space.
  - Formula: **signature match + byte-copy safe = safe to memcpy.**
- **Opaque types** — the `BufferedReadings` story:
  - Some types (offset_ptr containers, platform handles) are not
    fully analyzable by reflection. Register them as opaque with a
    one-line macro that declares safety properties and a tag.
    The signature encodes tag, size, alignment, and element type.
    Safety recurses into the element type.
- **Runtime transfer verification** — the plugin `dlopen` pattern:
  a plugin exports its signature as a C string; the host calls
  `is_transfer_safe<T>(plugin_sig)` at load time. One call checks
  two conditions: byte-copy safe + signature match — the definition
  of *transfer-safe* from Part 1's matrix.

### Part 5 — The Cross-Platform Pipeline (7 min)

*"Export once, compare anywhere."*

- Two-phase workflow:
  - **Phase 1** (on each build target): a one-line macro exports
    compile-time signatures + ABI metadata to a header file.
  - **Phase 2** (on any machine, no P2996 needed): include the exported
    headers and run a compatibility check that produces the matrix
    from Part 1.
- ABI equivalence grouping: same-fingerprint build targets are
  grouped automatically, but the signature comparison is the
  definitive answer.

### Q&A (5 min)

## Target Audience

- **Primary**: C++ developers working with IPC, shared memory, network
  protocols, plugin systems, or binary file formats — anyone who has
  written `static_assert(sizeof(T) == N)` or wished `offsetof` worked
  on their real types.
- **Secondary**: Library authors exploring P2996 reflection as a
  building block for compile-time analysis.
- **Tertiary**: Compiler implementers and language designers interested
  in real-world P2996 implementation experience — especially
  `offset_of` and `access_context::unchecked()` across compilers.

## Prerequisites

- Familiarity with `sizeof`, `alignof`, `offsetof`
- Basic understanding of memory layout (padding, alignment)
- No prior P2996 or reflection knowledge required — explained in talk

## Supplementary Materials

*(Available upon acceptance. Anonymous repository link provided to
reviewers on request — full implementation, test suite, and
cross-platform demo can be inspected before the acceptance decision.)*

- **Source code**: Complete library, header-only, Boost Software License
- **Test suite**: 12 tests (7 core P2996 + 5 tools), 200+ static_assert
- **Cross-platform demo**: Pre-generated signatures for 6 build targets
- **WG21 paper**: Implementation experience report drafted for SG7
