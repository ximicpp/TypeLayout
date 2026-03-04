# Boost.TypeLayout Core Value Analysis

## Core Value Proposition

The core value of this library is: **generating a "layout signature" string at compile time
for C++ types that precisely describes the byte-level memory layout, used to determine whether
zero-copy (serialization-free) data transfer between two endpoints is safe.**

This breaks down into three sub-goals:

1. **Signature Generation** -- generate self-describing, platform-aware layout signature strings
2. **Safety Classification** -- classify types by safety level based on signature properties
3. **Cross-Platform Compatibility Check** -- compare signatures of the same type across platforms


---

## Verification

### [OK] 1. Signature Generation Engine -- Correctly Implemented

**Architecture Prefix** (`signature.hpp`):

```cpp
consteval auto get_arch_prefix() noexcept {
    if constexpr (sizeof(void*) == 8)
        return FixedString{TYPELAYOUT_LITTLE_ENDIAN ? "[64-le]" : "[64-be]"};
    else if constexpr (sizeof(void*) == 4)
        return FixedString{TYPELAYOUT_LITTLE_ENDIAN ? "[32-le]" : "[32-be]"};
}
```

Correctly encodes two key platform-difference dimensions: pointer width + byte order.
Signatures are prefixed with this, ensuring that signatures from different architectures
are naturally non-matching.

**Fundamental Type Mapping** (`type_map.hpp`):

- Fixed-width integers (`int8_t` -> `i8[s:1,a:1]`), floats, character types, bool, byte,
  nullptr all have dedicated specializations [OK]
- `long`/`long long` and other types that may alias fixed-width types are correctly
  guarded by `is_distinct_fundamental_int_v` to avoid duplicate specializations [OK]
- CV qualifiers are correctly stripped [OK]
- Pointers/references correctly use `sizeof(T*)` rather than `sizeof(T&)` to reflect
  actual storage size [OK]

**Struct Flattening** (`signature_impl.hpp`) -- this is the core algorithm:

```
record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}
```

Key design decisions are all correctly implemented:

| Scenario | Handling | Status |
|----------|----------|--------|
| Non-empty, non-opaque class member | Recursively flatten into parent layout | [OK] |
| Empty base class (EBO) | `embedded_empty_signature` patches `s:1` to `s:0` | [OK] |
| Empty member (`[[no_unique_address]]`) | Same: uses `s:0` | [OK] |
| Bit-field | `@byte.bit:bits<width,type>` full-precision encoding | [OK] |
| Union | Not flattened; preserves `union[s:N,a:M]{...}` structure | [OK] |
| Opaque type | Emitted as leaf node, no recursion | [OK] |
| Enum | `enum[s:N,a:M]<underlying_type>` | [OK] |
| Array | Byte arrays folded to `bytes[s:N,a:1]`; others `array[s:N,a:M]<elem,count>` | [OK] |

**Offset Calculation** uses P2996 `offset_of(member)` to directly obtain the
compiler-calculated offset -- the only reliable source. Base class offsets are
propagated downward via the `OffsetAdj` template parameter, correctly implementing
recursive offset adjustment for multi-level inheritance.


### [OK] 2. Safety Classification -- Correctly Implemented with Cross-Validation

**Five-tier Classification Model** (`safety_level.hpp` + `classify.hpp`):

```
Opaque > PointerRisk > PlatformVariant > PaddingRisk > TrivialSafe
```

Priority ordering logic is correct:

- **Opaque** highest: internal structure unknowable, safety cannot be determined
- **PointerRisk** above PlatformVariant: pointers make memcpy **semantically incorrect
  on any platform** (dangling pointers), whereas platform differences are only
  problematic cross-platform
- **PlatformVariant** includes `wchar_t`, `long double`, bit-fields -- correctly
  identifies actual cross-platform risk sources
- **PaddingRisk**: layout is fixed but padding may leak uninitialized data

**Key Highlight -- Dual-Path Cross-Validation** (`layout_traits.hpp`):

```cpp
static_assert(
    !(std::is_class_v<T> && !std::is_union_v<T> && !std::is_empty_v<T>) ||
    has_padding == detail::sig_has_padding(std::string_view(signature)),
    "layout_traits cross-validation failure...");
```

`has_padding` is computed via two independent paths:
1. **Compile-time bitmap method** (`compute_has_padding`): traverses all fields via
   reflection, marking each covered byte in `[0, sizeof(T))`
2. **Runtime signature parsing method** (`sig_has_padding`): parses the generated
   signature string, extracting `@offset` and `[s:N]` to reconstruct coverage intervals

The two are cross-validated with `static_assert`; if they disagree, compilation fails.
This is an excellent defensive engineering practice.


### [OK] 3. Token-Boundary Matching -- Correctly Solves Substring False Matches

```cpp
// "nullptr[" must not match the "ptr[" detector
constexpr bool contains_token(const FixedString<M>& needle) const noexcept {
    // On match, check whether the preceding character is alphanumeric
    if (i == 0) return true;
    char prev = value[i - 1];
    bool prev_is_alnum = ...;
    if (!prev_is_alnum) return true;
}
```

Both `contains_token` (compile-time `FixedString`) and `sig_contains_token` (runtime
`string_view`) implement token-boundary checking, preventing `"ptr["` from falsely
matching inside `"nullptr["`. This detail is critical for signature scanning correctness.


### [OK] 4. Opaque Type System -- Correct Trust Boundary Design

Three tiers are provided:

| Macro | Purpose | Signature Format |
|-------|---------|------------------|
| `TYPELAYOUT_OPAQUE_TYPE` | Fixed-size non-template type | `O!name[s:N,a:M]` |
| `TYPELAYOUT_OPAQUE_CONTAINER` | Single-param template | `O!name[s:N,a:M]<elem>` |
| `TYPELAYOUT_REGISTER_OPAQUE` | Semantic tag + pointer declaration | `O(Tag\|size\|align)` |

All macros have `static_assert` guards for `sizeof`/`alignof` consistency.
`TYPELAYOUT_REGISTER_OPAQUE` additionally requires `is_trivially_copyable_v`.
The `pointer_free` flag (computed as `!(HasPointer)` from the macro's third argument)
allows users to declare whether the opaque type contains pointers, affecting
safety classification.

`has_opaque` detection is **recursive** (`type_has_opaque` -> `any_member_has_opaque`
-> `any_base_has_opaque`), ensuring that any opaque type embedded at any nesting level
is captured.

Additionally, auto-deducing variants (`TYPELAYOUT_OPAQUE_TYPE_AUTO`,
`TYPELAYOUT_OPAQUE_CONTAINER_AUTO`, `TYPELAYOUT_OPAQUE_MAP_AUTO`) are provided.
These use `to_fixed_string(sizeof(T))` inside the `consteval` body to eliminate the
need for callers to provide numeric size/align literals.


### [OK] 5. Cross-Platform Workflow -- Complete End-to-End Pipeline

```
[P2996 compile] -> SigExporter.write() -> .sig.hpp
                                              |
[C++17 compile] -> CompatReporter.compare() -> compatibility report
```

1. `sig_export.hpp`: generates `.sig.hpp` header files on the P2996 compiler
   (pure `constexpr` data)
2. `.sig.hpp` files do not require P2996; any C++17 compiler can read them
3. `compat_check.hpp`: compares multi-platform signatures, outputs a matrix report

This design correctly isolates P2996 dependency in the generation phase.


### [OK] 6. Serialization-Free Determination -- Three-Condition Model Correct

```cpp
// Conditions 1 + 2: compile-time local check
template <typename T>
struct is_local_serialization_free
    : std::bool_constant<
          std::is_trivially_copyable_v<T> &&
          !layout_traits<T>::has_pointer> {};

// Condition 3: runtime signature matching
bool is_serialization_free(std::string_view key) const {
    return local_it->second == remote_it->second;
}
```

Three necessary conditions are clearly layered:
1. `trivially_copyable` -- memcpy does not break the object model
2. `!has_pointer` -- no address-space dependencies
3. `signature_match` -- both endpoints have identical layout

Uses **exact string comparison** (not hashing). The comments explicitly state the
rationale: "zero collision risk (false positive = silent data corruption)".


---

## Identified Issues and Risks

### (!) 1. `FixedString::operator+` Has Potential Capacity Bloat

```cpp
template <size_t M>
constexpr auto operator+(const FixedString<M>& other) const noexcept {
    // ...
    while (pos < N && value[pos] != '\0') {  // stops at embedded '\0'
        result[pos] = value[pos];
        ++pos;
    }
```

If the left operand's actual length is less than `N` (i.e., it contains internal `'\0'`),
then `result`'s capacity is `N + M`, but only `length()` characters are actually copied.
The `other` portion is then written starting at `pos` (not `N`). This means `result`'s
capacity is `N + M`, but actual content may be shorter, wasting space but never overflowing.
This frequently occurs when `to_fixed_string` returns `FixedString<20>` -- each
concatenation inflates the signature's `FixedString` capacity by 20 bytes.

**Impact**: Compile-time memory consumption bloat. Does not affect correctness, but may
significantly slow compilation for large struct signatures.


### (!) 2. `skip_first()` Returns `FixedString<N>` Instead of `FixedString<N-1>`

```cpp
consteval auto skip_first() const noexcept {
    // ...
    return FixedString<N>(result);  // size unchanged
}
```

After stripping the first character, there should be `N-1` valid characters, but
`FixedString<N>` is returned. This does not affect correctness (there is a null
terminator), but the `size` template parameter is imprecise. Combined with issue 1,
this compounds the capacity bloat.


### (!) 3. `sig_has_padding` Runtime Parser Has 512-Field Hard Limit

```cpp
constexpr std::size_t MAX_FIELDS = 512;
Interval intervals[MAX_FIELDS]{};
```

Structs with more than 512 leaf fields will silently ignore subsequent fields,
potentially causing false padding reports. However, this is a `constexpr` function
used only in the cross-validation `static_assert` inside `layout_traits`. If the
bitmap method and parser method disagree, compilation fails, so in practice the
error would be caught.

**Mitigation**: The cross-validation `static_assert` acts as a safety net.


### (!) 4. `is_fixed_enum` is Defined but Not Used

```cpp
template <typename T>
[[nodiscard]] consteval bool is_fixed_enum() noexcept { ... }
```

This function is defined in `reflect.hpp` but never called in signature generation or
classification. The primary template's enum branch unconditionally generates a signature
without checking whether the underlying type is explicitly fixed.

This means `enum E { A, B, C };` (no explicit underlying type) will produce the same
signature as `enum E : int { A, B, C };`, even though the former's underlying type may
theoretically differ across platforms.

**Mitigation**: The function's own comments (lines 54-60 of `reflect.hpp`) acknowledge
this limitation -- C++ provides no API to distinguish an explicitly specified underlying
type from a compiler-inferred one, so the function is a best-effort approximation.
In practice, all major compilers use `int` as the default underlying type for unscoped
enums.


---

## Conclusion

| Dimension | Assessment |
|-----------|------------|
| **Signature Generation** | [OK] Correct. Flattening recursion, offset propagation, EBO/`[[no_unique_address]]`/bit-field handling all correct |
| **Safety Classification** | [OK] Correct. Priority ordering is sound; dual-path cross-validation is a strong guarantee |
| **Cross-Platform Compat** | [OK] Correct. P2996 isolation design is sound; exact signature matching has zero collision risk |
| **Opaque Mechanism** | [OK] Correct. Trust boundary is clear; `static_assert` guards size |
| **Serialization-Free** | [OK] Correct. Three-condition model is complete |

**The core value is correctly implemented.** The signature syntax is self-consistent
and self-describing. The safety classification priority ordering is sound. The cross-validation
mechanism between compile-time bitmap and runtime signature parsing is excellent defensive
engineering. The identified risk items are non-critical (compile efficiency, boundary capacity)
and do not affect core semantic correctness.
