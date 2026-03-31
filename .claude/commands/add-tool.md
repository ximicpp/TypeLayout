Add a new analyzer tool to the TypeLayout tools layer.

Use this when: creating a new analysis, reporting, or utility tool in `include/boost/typelayout/tools/`.

## P2996 vs C++17 Decision

Before writing, decide if the tool needs P2996 reflection:
- **Needs P2996**: Can use `layout_traits<T>`, `get_layout_signature<T>()`, reflection. Pattern: `tools/classify.hpp`, `tools/serialization_free.hpp`
- **C++17 only**: Works with signature strings, no reflection needed. Pattern: `tools/safety_level.hpp`, `tools/compat_check.hpp`

This decision affects compile flags, test labels, and what headers can be included.

## Steps

1. **Create the header** in `include/boost/typelayout/tools/`:
   - Use header guard: `#ifndef BOOST_TYPELAYOUT_TOOLS_<NAME>_HPP`
   - Namespace: `boost::typelayout`
   - If C++17: do NOT include any core headers that use P2996 (signature.hpp, layout_traits.hpp, etc.)
   - If P2996: can include anything

2. **Implement the tool**:
   - Follow existing patterns: structs for data, free functions or class methods for logic
   - For P2996 tools: use `consteval` for compile-time analysis functions
   - For C++17 tools: use `constexpr` or runtime functions

3. **Update the umbrella header** (if the tool should be included by default):
   - Only add to `typelayout.hpp` if it requires P2996
   - C++17-only tools are typically included individually by users

4. **Create a test file** in `test/`:
   - Name: `test_<tool_name>.cpp`
   - Use `static_assert` for compile-time checks (if P2996)
   - Add runtime `std::cout` output for debugging visibility

5. **Register in CMakeLists.txt**:
   - For P2996 tests (link to `typelayout` interface library which provides compile flags):
     ```cmake
     add_executable(test_<name> test/test_<name>.cpp)
     target_link_libraries(test_<name> PRIVATE typelayout)
     add_test(NAME test_<name> COMMAND test_<name>)
     set_tests_properties(test_<name> PROPERTIES TIMEOUT 120 LABELS "tools")
     ```
   - For C++17 tests (set compile options manually, no typelayout link):
     ```cmake
     add_executable(test_<name> test/test_<name>.cpp)
     target_include_directories(test_<name> PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
     target_compile_options(test_<name> PRIVATE -std=c++17)
     add_test(NAME test_<name> COMMAND test_<name>)
     set_tests_properties(test_<name> PROPERTIES TIMEOUT 30 LABELS "tools")
     ```
     Note: Do not add `-stdlib=libc++` — it is Clang-specific and breaks GCC builds.

6. **Build and test**: Delete `build/` and run `/build-test` (new CMake target requires reconfigure).

## Key Files

- `include/boost/typelayout/tools/` -- tool headers live here
- `include/boost/typelayout/tools/sig_types.hpp` -- shared data types (TypeEntry, PlatformInfo)
- `test/` -- test files
- `CMakeLists.txt` -- test registration

## Important Notes

- C++17 tools must NOT include P2996-dependent headers (they won't compile)
- Check `tools/sig_types.hpp` for shared data structures before creating new ones
- The tools layer is designed for analysis consumers; keep it user-friendly
- Update AGENTS.md test table when adding a new test
