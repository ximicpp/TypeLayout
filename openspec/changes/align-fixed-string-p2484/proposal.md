# Change: Align FixedString<N> Semantics with P2484 (std::basic_fixed_string)

## Why

C++ proposal P2484 defines `std::basic_fixed_string<CharT, N>` where `N` is the
**character count** (excluding the null terminator). TypeLayout's current
`FixedString<N>` uses `N` as the **buffer size** (including the null terminator).

This semantic mismatch means:
- `FixedString{"hello"}` deduces `N = 6` (TypeLayout) vs `N = 5` (P2484).
- Concatenation uses `N + M - 1` (TypeLayout) vs `N + M` (P2484).
- `FixedString<sv.size() + 1>(sv)` would become `FixedString<sv.size()>(sv)`.

Aligning now (while the library is pre-1.0) avoids a harder migration later when
`std::fixed_string` enters the standard. It also enables future drop-in
replacement with the standard type.

## What Changes

- Redefine `N` as character count: internal buffer becomes `char value[N + 1]`.
- Update CTAD: `FixedString("hello")` deduces `N = 5`.
- Update `operator+`: result size `N + M` (not `N + M - 1`).
- Update `to_fixed_string`: return `FixedString<20>` (not `FixedString<21>`).
- Update `skip_first`: return `FixedString<N>` (buffer still large enough).
- Update all `FixedString<sv.size() + 1>(sv)` to `FixedString<sv.size()>(sv)`.
- Add CTAD deduction guide for `const char (&)[N]` -> `FixedString<N - 1>`.
- Update `static constexpr size_t size = N` (not `N - 1`).
- No change to signature output strings (identical runtime behavior).

## Impact

- Affected specs: `signature`
- **BREAKING** (internal): Template parameter `N` changes meaning.
  All explicit `FixedString<N>` instantiations must be updated.
  However, the vast majority of uses are `FixedString{"literal"}` which are
  handled automatically by the updated CTAD.
- No change to generated signature strings (all tests must still pass).
- Affected code: `fixed_string.hpp`, `detail/reflect.hpp`, `test_fixed_string.cpp`
