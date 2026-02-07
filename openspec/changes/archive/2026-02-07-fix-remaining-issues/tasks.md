## 1. P0 Fix: qualified_name_for recursive depth
- [x] 1.1 Rewrite `qualified_name_for` in `signature_detail.hpp:25-45` to fully recurse the `parent_of` chain until reaching global namespace, instead of only checking grandparent
- [x] 1.2 Add deep namespace test case (>=3 levels) in `test_two_layer.cpp` to verify the fix

## 2. Missing Definition-layer tests
- [x] 2.1 Add virtual inheritance `~vbase<>` Definition signature test
- [x] 2.2 Add multiple inheritance Definition signature format test
- [x] 2.3 Add deeply nested struct Definition signature tree structure test
- [x] 2.4 Add union Definition field names test

## 3. Validation
- [x] 3.1 Build and run tests in Docker container