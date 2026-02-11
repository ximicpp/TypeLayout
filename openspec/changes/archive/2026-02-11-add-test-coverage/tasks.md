## 1. FixedString unit tests

- [x] 1.1 Create `test/test_fixed_string.cpp`
- [x] 1.2 Test constructors: from literal, from string_view, default
- [x] 1.3 Test `operator+`: normal, empty operand, large concatenation
- [x] 1.4 Test `operator==`: same size, cross-size, empty vs non-empty, c-string
- [x] 1.5 Test `length()`: zero, short, full buffer
- [x] 1.6 Test `skip_first()`: normal, empty, single-char
- [x] 1.7 Test `to_fixed_string()`: zero, small, large, negative
- [x] 1.8 Add CMakeLists.txt target for test_fixed_string

## 2. SigExporter output test

- [x] 2.1 Create `test/test_sig_export.cpp`
- [x] 2.2 Test `SigExporter::write()` output to stringstream
- [x] 2.3 Verify output contains expected signature strings
- [x] 2.4 Verify output contains platform metadata
- [x] 2.5 Add CMakeLists.txt target for test_sig_export

## 3. Verify

- [x] 3.1 Build and run all tests (old + new)