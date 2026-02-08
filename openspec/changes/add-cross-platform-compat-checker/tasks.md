## 1. Foundation: Platform Detection
- [x] 1.1 Create `include/boost/typelayout/tools/platform_detect.hpp`
  - Detect arch via `__x86_64__`, `_M_X64`, `__aarch64__`, `_M_ARM64`, `__i386__`, `__arm__`
  - Detect OS via `__linux__`, `_WIN32`, `__APPLE__`
  - Detect compiler via `__clang__`, `__GNUC__`, `_MSC_VER`
  - Provide `get_platform_name()` → `std::string` (runtime) e.g. `"x86_64_linux_clang"`
  - Provide `get_platform_display_name()` → human-readable e.g. `"x86-64 Linux (Clang)"`

## 2. Signature Export Tool
- [x] 2.1 Create `include/boost/typelayout/tools/sig_export.hpp`
  - `SigExporter` class with `add<T>(name)` template method
  - `add<T>()` uses `get_layout_signature<T>()` and `get_definition_signature<T>()` at compile time
  - Convert `FixedString` signatures to `std::string` for file output
  - `write(path)` method: generates a `.sig.hpp` header file
  - Generated header contains:
    - Platform metadata (name, arch_prefix, pointer_size, sizeof_long, sizeof_wchar_t, sizeof_long_double)
    - Per-type `inline constexpr const char TYPE_layout[] = "...";`
    - Per-type `inline constexpr const char TYPE_definition[] = "...";`
    - `TypeEntry` struct and `types[]` registry array
    - `type_count` constexpr variable
  - Proper escaping of `\` and `"` in signature strings
  - Include guard and namespace wrapping

## 3. Compatibility Check Utilities
- [x] 3.1 Create `include/boost/typelayout/tools/compat_check.hpp`
  - `constexpr bool sig_match(const char* a, const char* b)` — string_view comparison
  - `constexpr bool layout_match(const char* a, const char* b)` — alias for sig_match
  - `constexpr bool definition_match(const char* a, const char* b)` — alias for sig_match
  - `CompatReporter` class (runtime):
    - `add_platform(name, TypeEntry* types, int count)`
    - `print_report(std::ostream&)` — formatted compatibility matrix
    - Summary statistics: N/M types compatible, percentage

## 4. Example: Pure C++ Workflow
- [x] 4.1 Rewrite `example/cross_platform_check.cpp` as signature exporter
  - Use `SigExporter` to export signatures
  - Include representative types (PacketHeader, SharedMemRegion, etc.)
  - Auto-detect platform name
- [x] 4.2 Create `example/compat_check.cpp`
  - Include pre-generated `.sig.hpp` files
  - Demonstrate `static_assert` compile-time checks
  - Demonstrate `CompatReporter` runtime report
- [x] 4.3 Create `example/sigs/` with sample pre-generated signature headers
  - At least 2 platform signatures for testing

## 5. CMake Integration
- [x] 5.1 Create `cmake/TypeLayoutCompat.cmake` module
  - `typelayout_add_sig_export(TARGET name TYPES_HEADER header PLATFORM_NAME name)`
  - `typelayout_add_compat_check(TARGET name SIGNATURES sig1.hpp sig2.hpp ...)`
- [x] 5.2 Update `CMakeLists.txt`
  - Add `tools/` subdirectory support
  - Add example build targets for export + check
  - Add test target for compat_check

## 6. Documentation
- [x] 6.1 Update `example/README.md` with pure C++ workflow
  - Remove Python references
  - Document two-phase build pipeline
  - Quick start guide with Docker examples
- [x] 6.2 Update `openspec/project.md` — add tools layer description

## 7. Testing
- [x] 7.1 Create `test/test_compat_check.cpp`
  - Test `sig_match` with matching and non-matching signatures
  - Test `CompatReporter` output
  - Test platform detection
- [x] 7.2 Generate test signature headers (from Docker build)
  - Build and run exporter in Docker to produce actual `.sig.hpp`
  - Verify generated header compiles correctly

## 8. Cleanup
- [x] 8.1 Remove `scripts/compare_signatures.py`
- [x] 8.2 Remove `scripts/` directory if empty