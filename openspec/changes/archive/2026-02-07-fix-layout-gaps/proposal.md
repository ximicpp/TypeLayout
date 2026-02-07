# Change: Fix Layout signature gaps from audit

## Why
The Layout signature audit (analyze-layout-signature) found 1 P2 semantic label gap
and 7 test coverage gaps. This proposal addresses all of them.

## What Changes
- Add missing `R(*)(Args..., ...) noexcept` function pointer specialization (P2)
- Add Layout-layer tests for: pointers, references, function pointers, platform types,
  anonymous members, multidimensional arrays, array fields, and CV-qualified fields

## Impact
- Affected specs: signature
- Affected code:
  - `include/boost/typelayout/core/signature_detail.hpp` (add 1 specialization)
  - `test/test_two_layer.cpp` (add test cases)
