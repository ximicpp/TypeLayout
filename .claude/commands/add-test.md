Write a new test for the TypeLayout project.

Use this when: adding a test for new functionality, regression tests, or expanding coverage.

## Steps

1. **Determine test layer**:
   - **Core (P2996)**: Tests signature generation, layout_traits, reflection. Compile with C++26 + P2996 flags.
   - **Tools (C++17)**: Tests analyzers that work on signature strings. Compile with C++17 only.

2. **Create test file** in `test/`:
   - Name: `test_<descriptive_name>.cpp`
   - Define test types inline (structs, enums, etc.) -- do not use shared type headers

3. **Write dual-verification tests**:
   ```cpp
   // Compile-time verification (preferred)
   static_assert(some_property<T>(), "descriptive message");

   // Runtime output for debugging
   std::cout << "T signature: " << get_layout_signature<T>().value << "\n";
   ```

4. **Register in CMakeLists.txt**:

   For P2996 core tests (link to `typelayout` interface library which provides compile flags):
   ```cmake
   add_executable(test_<name> test/test_<name>.cpp)
   target_link_libraries(test_<name> PRIVATE typelayout)
   add_test(NAME test_<name> COMMAND test_<name>)
   set_tests_properties(test_<name> PROPERTIES TIMEOUT 120 LABELS "core")
   ```

   For P2996 tools tests:
   ```cmake
   add_executable(test_<name> test/test_<name>.cpp)
   target_link_libraries(test_<name> PRIVATE typelayout)
   add_test(NAME test_<name> COMMAND test_<name>)
   set_tests_properties(test_<name> PROPERTIES TIMEOUT 120 LABELS "tools")
   ```

   For C++17-only tests (set compile options manually, no typelayout link):
   ```cmake
   add_executable(test_<name> test/test_<name>.cpp)
   target_include_directories(test_<name> PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
   target_compile_options(test_<name> PRIVATE -std=c++17 -stdlib=libc++)
   target_link_options(test_<name> PRIVATE -stdlib=libc++)
   add_test(NAME test_<name> COMMAND test_<name>)
   set_tests_properties(test_<name> PROPERTIES TIMEOUT 30 LABELS "tools")
   ```

5. **Build and test**: Delete `build/` and run `/build-test` (new CMake target requires reconfigure).

6. **Update AGENTS.md**: Add the new test to the Tests table.

## Test Structure Pattern

```cpp
#include <boost/typelayout/typelayout.hpp>  // or specific headers
#include <iostream>

using namespace boost::typelayout;

// Define test types inline
struct MyTestStruct {
    int a;
    double b;
};

int main() {
    // static_assert checks first
    constexpr auto sig = get_layout_signature<MyTestStruct>();
    static_assert(sig.value[0] == '[', "signature must start with arch prefix");

    // Runtime output for visibility
    std::cout << "MyTestStruct: " << sig.value << "\n";
    std::cout << "All checks passed.\n";
    return 0;
}
```

## Key Files

- `test/` -- all test files
- `CMakeLists.txt` -- test target registration
- `AGENTS.md` -- test catalog (update when adding tests)

## Important Notes

- P2996 compilation is slow; 120s timeout accounts for this
- Prefer `static_assert` over runtime assertions -- catch errors at compile time
- Each test must be self-contained: inline type definitions, no shared test utilities
- Test names in CMake must match the executable name
