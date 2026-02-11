## Context

TypeLayout's `FixedString<N>` is a compile-time string type used as the
fundamental building block for signature generation. It appears in 100+
locations across the codebase. P2484 (`std::basic_fixed_string`) is the
leading standardization candidate for this pattern.

## Goals / Non-Goals

**Goals:**
- `N` means "character count" (P2484 convention), not "buffer size"
- Zero change to generated signature strings
- All existing tests pass without signature value changes
- Future compatibility with `std::fixed_string` when standardized

**Non-Goals:**
- Full P2484 API parity (no `substr()`, `find()`, `operator<=>` yet)
- CharT parameterization (stay `char`-only for now)
- Switching to an external library

## Decisions

### Decision 1: N = character count, buffer = N + 1

```cpp
// Before:
template <size_t N>  // N includes '\0'
struct FixedString {
    char value[N];
    static constexpr size_t size = N - 1;
};

// After:
template <size_t N>  // N = character count
struct FixedString {
    char value[N + 1];
    static constexpr size_t size = N;
};
```

### Decision 2: CTAD deduction guide

```cpp
// Deduction guide: "hello" has type const char[6], deduce N = 5
template <size_t N>
FixedString(const char (&)[N]) -> FixedString<N - 1>;
```

This means `FixedString{"hello"}` automatically deduces `N = 5` without
changing any call sites. The 111 `FixedString{"..."}` call sites need
**zero modification**.

### Decision 3: Concatenation formula

```cpp
// Before: N + M - 1 (subtract one overlapping '\0')
// After:  N + M (both N and M exclude '\0')
constexpr size_t new_size = N + M;
```

### Decision 4: to_fixed_string returns FixedString<20>

`uint64_t` max is 20 digits. Before: `FixedString<21>` (20 chars + '\0').
After: `FixedString<20>` (20 chars, buffer is 21 internally).

### Decision 5: reflect.hpp update

```cpp
// Before:
FixedString<self.size() + 1>(self)
// After:
FixedString<self.size()>(self)
```

The `+ 1` was compensating for the old "N includes '\0'" convention.

## Risks / Trade-offs

- **Risk**: Missed update in explicit `FixedString<N>` usage causes
  compilation error (off-by-one in buffer). Mitigated by: all such sites
  are in `detail/reflect.hpp` and `fixed_string.hpp` only.
- **Risk**: `skip_first()` previously returned `FixedString<N>` (same N,
  one fewer char). With new semantics, returning `FixedString<N>` means
  the buffer is still `N + 1` which is sufficient. No issue.

## Migration Plan

1. Update `FixedString` struct definition (buffer, size, constructors)
2. Add CTAD deduction guide
3. Update `operator+` formula
4. Update `to_fixed_string` return type
5. Update `detail/reflect.hpp` explicit sizes
6. Update `test_fixed_string.cpp`
7. Run all tests -- signature values must be identical
