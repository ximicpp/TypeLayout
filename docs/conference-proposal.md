# Conference Session Proposal

## Title

**Your Class is Lying to You: Cross-Platform Layout Verification with C++26 Reflection**

## Abstract

*Which of my types can I safely memcpy between which build targets
without serialization?* In domains where serialization overhead is
unacceptable — game engines, high-frequency trading, HPC,
shared-memory IPC — this question matters for every message type on
every target platform. Standard C++ alone cannot answer this portably
at compile time. `sizeof` misses field offsets. `offsetof` is
conditionally-supported on non-standard-layout types (private members,
inheritance). Manual assertions are platform-local and cannot compare
across compilers.

The running case study is TypeLayout, a layout-signature library built
on C++26 static reflection (P2996). It generates a compile-time
**layout signature** — a deterministic string encoding every field
offset, size, alignment, and padding gap of any C++ type, including
classes with private members and inheritance hierarchies — and provides
a verifiable answer for every type-target pair: transfer-safe, layout
mismatch, or pointer risk. Along the way, attendees gain hands-on
P2996 experience — the reflection patterns, API friction points, and
cross-compiler surprises encountered while building the library.

## Outline

### Part 1 — The Question and the Answer (8 min)

*"Four message types, six targets. Which can I memcpy without serialization?"*

- Introduce four message types built from two reusable building
  blocks (`Timestamp` and `MessageHeader`, flattened inside the
  others). Not trivial standard-layout structs, but
  real C++ code where `offsetof` is conditionally-supported or
  inapplicable:
  ```cpp
  class Timestamp {                          // private members
      int64_t seconds_; uint32_t nanos_;
  };
  class MessageHeader {                      // base class
      uint32_t magic_; uint16_t version_; uint16_t type_;
  };
  class TelemetryMsg : public MessageHeader { // inheritance
      Timestamp ts_; float values_[4];
  };
  class PlatformRiskyMsg : public MessageHeader { // platform-variant
      Timestamp ts_;
      long counter_;    // LP64: 8B, LLP64: 4B
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
    linux_gcc:    ...@24:i64[s:8,a:8]...
    windows_msvc: ...@24:i32[s:4,a:4]...
                       ^^^
  ```

### Part 2 — What the Signatures Tell You (12 min)

*"Reading the diff."*

- Drill into `TelemetryMsg`'s signature to teach the format:
  ```
  [64-le]record[s:40,a:8]{
    @0:u32[s:4,a:4],               // magic_    (from MessageHeader)
    @4:u16[s:2,a:2],               // version_
    @6:u16[s:2,a:2],               // type_
    @8:i64[s:8,a:8],               // ts_.seconds_  (flattened!)
    @16:u32[s:4,a:4],              // ts_.nanos_
    @24:array[s:16,a:4]<f32[s:4,a:4],4>   // values_[4]
  }
  ```
  Key observations: base class and nested struct fields are flattened
  (inheritance erased, only byte identity remains); private members
  are visible via `access_context::unchecked()`.
- Drill into the `PlatformRiskyMsg` diff: `i64` vs `i32` (`long`)
  pinpoints exactly which field broke.

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

### Part 4 — Beyond Layout Match: Transfer Safety (13 min)

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
- **Runtime transfer verification**: a single
  `is_transfer_safe<T>(remote_sig)` call checks both conditions
  (byte-copy safe + signature match) at load time.

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
