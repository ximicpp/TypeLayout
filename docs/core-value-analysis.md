# Boost.TypeLayout — Core Value, Design, and Implementation Analysis

> Last updated: 2026-03-06

## 1. Core Value Proposition

**Can struct T be safely `memcpy`'d between endpoint A and endpoint B?**

The library answers this question at compile time by generating a deterministic,
human-readable "layout signature" string that encodes the byte-level memory identity
of any C++ type. Two types with identical signatures have identical memory layouts
and can be safely `memcpy`'d; different signatures mean the `memcpy` path is unsafe.

This breaks down into three pillars:

| Pillar | What it does | Key API |
|--------|-------------|---------|
| **Signature Generation** | Produce a self-describing string encoding field types, sizes, alignments, offsets, and padding | `get_layout_signature<T>()` |
| **Safety Classification** | Classify types into five safety tiers based on signature properties | `classify<T>`, `classify_signature()` |
| **Cross-Platform Compatibility** | Compare signatures of the same type across different platforms | `SigExporter` → `.sig.hpp` → `CompatReporter` |


---

## 2. Signature Grammar

The signature is a self-describing string following a formal grammar
(documented in `signature_impl.hpp`):

```
full-signature ::= arch-prefix type-signature
arch-prefix    ::= '[' ('32'|'64') '-' ('le'|'be') ']'
type-signature ::= leaf | record | union | enum | array | opaque

leaf    ::= kind '[' 's:' SIZE ',a:' ALIGN ']'
record  ::= 'record[s:' SIZE ',a:' ALIGN ']{' member-list '}'
union   ::= 'union[s:'  SIZE ',a:' ALIGN ']{' member-list '}'
enum    ::= 'enum[s:'   SIZE ',a:' ALIGN ']<' underlying '>'
array   ::= 'array[s:'  SIZE ',a:' ALIGN ']<' element ',' COUNT '>'
          | 'bytes[s:' SIZE ',a:1]'
opaque  ::= 'O!' name '[s:' SIZE ',a:' ALIGN ']' ('<' args '>')?
          | 'O(' tag '|' SIZE '|' ALIGN ')'

member  ::= '@' OFFSET ':' type-signature
          | '@' BYTE '.' BIT ':bits<' WIDTH ',' leaf '>'
```

Example:

```
[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:f64[s:8,a:8]}
 ^^^^^ ^^^^^^          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 arch   outer record   flattened fields with offsets
```

Key properties:
- **Deterministic** — same type always produces same string
- **Self-describing** — the string IS the diagnostic; no lookup tables needed
- **Platform-aware** — architecture prefix ensures cross-platform signatures never accidentally match
- **Exact comparison** — no hashing; zero collision risk (false positive = silent data corruption)


---

## 3. Architecture

Header-only, two-layer design:

```
┌──────────────────────────────────────────────────┐
│                  Tools Layer                      │
│  (mostly C++17, no P2996 required)               │
│                                                  │
│  safety_level.hpp    classify.hpp                 │
│  serialization_free.hpp                          │
│  sig_export.hpp  ──►  .sig.hpp  ──►  compat_check│
└──────────────────────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────┐
│                  Core Layer                       │
│  (requires P2996 static reflection)              │
│                                                  │
│  signature.hpp       layout_traits.hpp           │
│  detail/signature_impl.hpp   detail/reflect.hpp  │
│  detail/type_map.hpp         opaque.hpp          │
│  fixed_string.hpp            config.hpp          │
└──────────────────────────────────────────────────┘
```

The Core layer generates signatures and computes layout traits via P2996 reflection.
The Tools layer consumes these products for classification, serialization-free
determination, signature export, and cross-platform compatibility checking.

The cross-platform pipeline deliberately isolates the P2996 dependency:

```
Phase 1 (P2996): SigExporter.add<T>() → .sig.hpp (constexpr data)
Phase 2 (C++17): CompatReporter.compare() → compatibility report
```


---

## 4. Signature Generation Engine

### 4.1 Type Mapping (`detail/type_map.hpp`)

Every C++ type is mapped to a canonical signature token:

| Category | Examples | Signature |
|----------|----------|-----------|
| Fixed-width integers | `int32_t`, `uint64_t` | `i32[s:4,a:4]`, `u64[s:8,a:8]` |
| Fundamental integers | `long` (if distinct from fixed-width) | `i32[s:4,a:4]` or `i64[s:8,a:8]` |
| Floating point | `float`, `double`, `long double` | `f32[s:4,a:4]`, `f64[s:8,a:8]`, `f80[s:N,a:M]` |
| Characters | `char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t` | `char[s:1,a:1]`, `wchar[s:N,a:M]`, ... |
| Other | `bool`, `std::byte`, `std::nullptr_t` | `bool[s:1,a:1]`, `byte[s:1,a:1]`, `nullptr[s:N,a:M]` |
| Pointers | `T*` | `ptr[s:N,a:N]` |
| Function pointers | `R(*)(Args...)` | `fnptr[s:N,a:N]` |
| Member pointers | `T C::*` | `memptr[s:N,a:M]` |
| References | `T&`, `T&&` | `ref[s:N,a:N]`, `rref[s:N,a:N]` |
| Enums | `enum E : uint8_t` | `enum[s:1,a:1]<u8[s:1,a:1]>` |
| Arrays | `int[10]`, `char[32]` | `array[s:40,a:4]<i32[s:4,a:4],10>`, `bytes[s:32,a:1]` |
| Unions | `union U { ... }` | `union[s:N,a:M]{@0:...,@0:...}` |
| Classes | `struct S { ... }` | `record[s:N,a:M]{@off:..., ...}` |
| Opaque | registered types | `O!tag[s:N,a:M]` or `O(Tag\|N\|M)` |

**Aliasing guard**: `long` and `long long` may alias `int32_t`/`int64_t` on some
platforms. The `is_distinct_fundamental_int_v` trait prevents duplicate `TypeSignature`
specializations by checking `std::is_same_v` at compile time.

**CV qualifiers**: `const T`, `volatile T`, `const volatile T` are all stripped and
forwarded to `TypeSignature<T>`. This is correct because qualifiers do not affect layout.

**Unsupported types**: `void`, unbounded arrays `T[]`, bare function types, and
compiler extensions (`__int128`, `_Float16`, `_BitInt(N)`) produce a clear
`static_assert` error.


### 4.2 Recursive Flattening (`detail/signature_impl.hpp`)

The core algorithm recursively flattens struct hierarchies into a flat list of
`@offset:type` entries:

```cpp
template <typename T, std::size_t OffsetAdj>
consteval auto layout_all_prefixed() noexcept {
    // 1. Flatten all base classes (with offset adjustment)
    // 2. Flatten all direct members (with offset adjustment)
    // 3. Concatenate with comma-prefixed fold expression
}
```

| Scenario | Handling |
|----------|----------|
| Non-empty, non-opaque class member | Recursively flatten into parent layout |
| Empty base class (EBO) | `embedded_empty_signature()` patches `s:1` to `s:0` |
| Empty member (`[[no_unique_address]]`) | Same as EBO: uses `s:0` |
| Bit-field | `@byte.bit:bits<width,type>` with full-precision offset encoding |
| Union member | Not flattened; emitted as `union[s:N,a:M]{...}` leaf node |
| Opaque member/base | Emitted as leaf node, no recursion |
| Virtual inheritance | **Rejected** with `static_assert` (see §6.1) |

**Offset calculation**: Uses P2996 `offset_of(member).bytes` to obtain
compiler-calculated offsets. The `OffsetAdj` template parameter propagates
the parent's base offset for nested flattening.

**Comma-prefixed concatenation**: All helpers return comma-prefixed strings.
The top-level `get_layout_content<T>()` strips the leading comma via
`skip_first()`, which returns an exact-sized `FixedString<N-1>`.


### 4.3 FixedString (`fixed_string.hpp`)

`FixedString<N>` is a compile-time string where N is the exact character count
(excluding null terminator). Key operations:

- **`operator+`**: Returns `FixedString<N+M>` (sum of capacities)
- **`to_fixed_string<V>()`**: NTTP form returns `FixedString<count_digits(V)>`
  with zero wasted capacity — `sizeof(42)` → `FixedString<2>("42")`
- **`skip_first()`**: Returns `FixedString<N-1>` (exact-sized)
- **`contains_token()`**: Token-boundary-aware search; prevents "ptr[" from
  matching inside "nullptr[" by checking the preceding character is not alphanumeric


---

## 5. Safety Classification

### 5.1 Five-Tier Model

Severity ordering (highest risk first):

```
Opaque > PointerRisk > PlatformVariant > PaddingRisk > TrivialSafe
```

| Level | Meaning | Cause |
|-------|---------|-------|
| `Opaque` | Safety unknown | Contains opaque (unanalyzable) fields |
| `PointerRisk` | memcpy semantically incorrect | Contains pointers/references, or non-trivially-copyable |
| `PlatformVariant` | Layout may differ across platforms | Contains `wchar_t`, `long double`, or bit-fields |
| `PaddingRisk` | Layout fixed but may leak data | Has alignment padding bytes |
| `TrivialSafe` | Safe for zero-copy transfer | None of the above |

Two parallel classifiers produce the same result:

- **Compile-time**: `classify<T>::value` — consumes `layout_traits<T>` properties
- **Runtime**: `classify_signature(string_view)` — parses the signature string

### 5.2 Dual-Path Padding Detection with Cross-Validation

`has_padding` is computed via two independent algorithms:

1. **Byte coverage bitmap** (`compute_has_padding`): Creates a `bool[sizeof(T)]`
   array, recursively marks every byte covered by a leaf field using P2996
   reflection. Any uncovered byte = padding.

2. **Signature string parser** (`sig_has_padding_impl`): Scans the generated
   signature for all `record[s:SIZE]{...}` blocks, parses `@offset` and `[s:N]`
   entries, builds coverage intervals, checks for gaps.

These are cross-validated with `static_assert` in `layout_traits<T>`:

```cpp
static_assert(
    !(is_class && !is_union && !is_empty) ||
    check_padding_consistency(has_padding, signature),
    "cross-validation failure");
```

If the two methods disagree, compilation fails. This is a strong correctness guarantee.

**Union handling**: Records nested inside `union{...}` blocks are skipped by
`sig_has_padding_impl` (via `union_depth_at()` depth tracking), matching
`compute_has_padding`'s treatment of unions as atomic leaf nodes. Union "padding"
is semantically ambiguous (depends on the active member at runtime).


---

## 6. Safety Guards

### 6.1 Virtual Inheritance Rejection

Virtual bases introduce hidden vbptrs whose layout is compiler-specific, and
diamond inheritance causes the flattening engine to double-count the shared
virtual base. TypeLayout rejects virtual inheritance at compile time:

```cpp
// reflect.hpp: recursive detection
template <typename T>
consteval bool has_virtual_base() noexcept;

// signature_impl.hpp: rejection at entry point
template <typename T, std::size_t OffsetAdj>
consteval auto layout_all_prefixed() noexcept {
    static_assert(!has_virtual_base<T>(), "...");
    ...
}
```

This catches both direct virtual bases and inherited virtual bases
(e.g., `struct D : VDerived {}` where `VDerived : virtual VBase`).

### 6.2 SigExporter Trivially-Copyable Guard

`SigExporter::add<T>()` requires `std::is_trivially_copyable_v<T>`:

```cpp
static_assert(std::is_trivially_copyable_v<T>,
    "SigExporter::add<T>: only trivially copyable types should be exported.");
```

This prevents exporting polymorphic types, types with virtual bases, or types
with non-trivial copy semantics. Phase 2's `CompatReporter` cannot detect
these properties from the signature string alone.

### 6.3 Serialization-Free Three-Condition Model

```cpp
is_local_serialization_free<T> =
    is_trivially_copyable_v<T> && !layout_traits<T>::has_pointer;
```

Three conditions must hold for zero-copy transfer:
1. `trivially_copyable` — memcpy does not break the object model
2. `!has_pointer` — no address-space dependencies
3. `signature_match` — both endpoints have identical layout (runtime check)


---

## 7. Opaque Type System

Opaque types have size/alignment identity but no internal structural identity.
The user guarantees internal layout consistency.

| Macro | Use case | Signature |
|-------|----------|-----------|
| `TYPELAYOUT_OPAQUE_TYPE(T, name, sz, al)` | Non-template type | `O!name[s:N,a:M]` |
| `TYPELAYOUT_OPAQUE_CONTAINER(T, name, sz, al)` | Single-param template | `O!name[s:N,a:M]<elem>` |
| `TYPELAYOUT_OPAQUE_MAP(T, name, sz, al)` | Two-param template | `O!name[s:N,a:M]<key,val>` |
| `TYPELAYOUT_REGISTER_OPAQUE(T, tag, hasPtr)` | With pointer declaration | `O(tag\|N\|M)` |
| `*_AUTO` variants | Auto-deduce size/align | Same as above |

All macros enforce `sizeof`/`alignof` consistency with `static_assert`.
`TYPELAYOUT_REGISTER_OPAQUE` additionally requires `is_trivially_copyable_v`.

The `pointer_free` flag enables precise safety classification: if the user
declares `HasPointer = false`, the type can achieve `TrivialSafe` classification.

Detection is recursive: `type_has_opaque<T>()` traverses all members and base
classes transitively. Any opaque type at any nesting level is captured.


---

## 8. Cross-Platform Pipeline

```
    Platform A (P2996)         Platform B (P2996)
    ┌─────────────────┐       ┌─────────────────┐
    │ SigExporter      │       │ SigExporter      │
    │   .add<Msg>()    │       │   .add<Msg>()    │
    │   .write("a/")   │       │   .write("b/")   │
    └────────┬────────┘       └────────┬────────┘
             │                         │
             ▼                         ▼
    a/x86_64_linux.sig.hpp    b/aarch64_linux.sig.hpp
             │                         │
             └──────────┬──────────────┘
                        ▼
              ┌─────────────────┐
              │ CompatReporter   │  (C++17, no P2996)
              │   .compare()     │
              │   .print_report()│
              └─────────────────┘
                        │
                        ▼
              Compatibility Matrix
```

`.sig.hpp` files contain `constexpr` data (signatures, platform metadata) and
can be compiled by any C++17 compiler. This isolates the P2996 dependency to
the generation phase.


---

## 9. Verified Edge Cases

All edge cases have been verified with compilation tests:

| Edge Case | Status | Behavior |
|-----------|--------|----------|
| Diamond inheritance (non-virtual) | Correct | All 5 fields correctly flattened |
| Virtual inheritance (direct or inherited) | Rejected | `static_assert` with clear error message |
| Empty base optimization (EBO) | Correct | `s:0` in host signature, `s:1` standalone |
| `[[no_unique_address]]` | Correct | `s:0`, overlapping offsets handled |
| Multiple empty bases at offset 0 | Correct | All `s:0`, padding correctly detected |
| Bit-fields in structs and unions | Correct | Full-precision `@byte.bit:bits<width,type>` |
| Union containing padded struct | Correct | Cross-validation passes; union-internal padding not flagged |
| Array of padded struct | Correct | `has_padding=true` via `any_member_array_elem_has_padding` |
| Nested multidimensional arrays | Correct | Dimensions preserved: `array<array<i32,4>,3>` |
| Recursive types (self-referencing) | Correct | Pointers are leaf nodes; no infinite recursion |
| `std::atomic<T>` | Non-trivially-copyable | SigExporter rejects; direct use produces implementation-specific signature |
| `__int128`, `_Float16` | Rejected | `static_assert("unsupported type")` |
| `long` aliasing `int32_t` | Correct | `is_distinct_fundamental_int_v` guard prevents duplicate specializations |
| Struct with only bases, no members | Correct | Base fields flattened; `field_count=0` |
| Empty struct (no bases, no members) | Correct | `record[s:1,a:1]{}` |


---

## 10. Known Limitations

### 10.1 Union Padding Semantics

`compute_has_padding` and `sig_has_padding` both return `false` for unions.
This is a deliberate design decision: union "padding" is semantically ambiguous
because the unused bytes depend on which member is active at runtime. Users in
information-leakage scenarios should manually audit union-containing types.

### 10.2 Runtime Parser Field Limit

`sig_has_padding_impl` uses a fixed `MAX_FIELDS = 512` limit. Types exceeding
this return `{true, true}` (conservative, may-be-truncated). The cross-validation
`static_assert` in `layout_traits` accepts truncated results, falling back to
the bitmap method which has no field-count limit.

### 10.3 Compiler Extension Types

`__int128`, `_Float16`, `__fp16`, `_BitInt(N)`, `__float128` are not supported.
Attempting to use them in a struct will produce a clear `static_assert` error.

### 10.4 Multidimensional Array Identity

`int[12]` and `int[3][4]` produce different signatures. This is correct behavior
(they are different C++ types), but `layout_signatures_match` will return `false`
even though their memory layouts are identical. This is a conservative (safe)
false negative.


---

## 11. Conclusion

| Dimension | Assessment |
|-----------|------------|
| **Signature Generation** | Correct. Recursive flattening, offset propagation, EBO/NUA/bit-field handling all verified |
| **Safety Classification** | Correct. Five-tier priority ordering is sound; dual-path cross-validation enforced by `static_assert` |
| **Cross-Platform Pipeline** | Correct. P2996 isolation design is sound; exact string matching has zero collision risk |
| **Opaque Mechanism** | Correct. Trust boundary is clear; recursive detection covers all nesting levels |
| **Safety Guards** | Complete. Virtual inheritance rejected, SigExporter guards trivially-copyable, union padding cross-validation consistent |
| **Serialization-Free** | Correct. Three-condition model (trivially_copyable + no pointers + signature match) is complete |

**The core value is correctly implemented with no known correctness issues.**
The signature syntax is self-consistent and self-describing. The cross-validation
mechanism between compile-time bitmap and runtime signature parsing provides
a strong correctness guarantee that catches implementation bugs at compile time.
