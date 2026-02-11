# Change: Quick Wins -- Code Quality and API Polish

## Why

The design review (D2.1a, D2.2a, D3.1c) identified three low-effort
improvements that reduce friction for users and maintainers:

1. `contains()` test helper is copy-pasted across two test files
2. `FixedString` lacks a convenient `as_string_view()` conversion
3. `is_byte_element_v` (variable template) vs `is_fixed_enum()` (function)
   naming style is inconsistent

## What Changes

- Extract `contains()` to `test/test_util.hpp` and update both test files
- Add `constexpr operator std::string_view()` to `FixedString`
- Rename `is_byte_element_v` to match existing function-style pattern

## Impact

- Affected specs: `signature`
- Affected code: `core/fwd.hpp`, `core/signature_detail.hpp`, `test/test_util.hpp`,
  `test/test_two_layer.cpp`, `test/test_opaque.cpp`
