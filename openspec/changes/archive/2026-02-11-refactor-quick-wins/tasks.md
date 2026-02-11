## 1. Extract test utility

- [x] 1.1 Create `test/test_util.hpp` with `contains()` helper
- [x] 1.2 Update `test/test_two_layer.cpp` to include `test_util.hpp` and remove local `contains()`
- [x] 1.3 Update `test/test_opaque.cpp` to include `test_util.hpp` and remove local `contains()`

## 2. FixedString improvements

- [x] 2.1 Add `constexpr operator std::string_view()` to `FixedString` in `fwd.hpp`

## 3. Naming consistency

- [x] 3.1 Replace `is_byte_element_v<T>` variable template with `is_byte_element<T>()` consteval function in `signature_detail.hpp`

## 4. Verify

- [x] 4.1 Build and run all tests locally