# Repository Guidelines

## Project Structure & Module Organization

Boost.TypeLayout is a header-only C++26 library built around P2996 static reflection. Public headers live in `include/boost/typelayout/`; keep implementation-only code under `detail/` and cross-platform reporting/export helpers under `tools/`. `include/boost/typelayout.hpp` is the public umbrella include. CMake helpers belong in `cmake/`, executable demonstrations and generated signature fixtures in `example/`, and regression sources in `test/`. CI workflows and toolchain images are under `.github/`; design notes and implementation plans are under `docs/`.

## Build, Test, and Development Commands

A P2996-capable compiler is required: GCC 16+ is preferred, while the Bloomberg Clang fork is the legacy alternative. Use WSL or the repository Docker images when the host compiler lacks reflection support.

```bash
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-16
cmake --build build --parallel
ctest --test-dir build --output-on-failure
ctest --test-dir build -R test_core --output-on-failure
```

The first command configures all default examples and compatibility targets; the next three build, run the complete suite, or select one CTest test. See `CLAUDE.md` for tested WSL and Docker invocations and required runtime library paths.

## Coding Style & Naming Conventions

Follow adjacent C++ style: four-space indentation, braces on the declaration line, and project headers before standard-library headers. Public APIs belong in `boost::typelayout::inline namespace v1`; internals belong in `detail`. Use `BOOST_TYPELAYOUT_<NAME>_HPP` header guards, snake_case functions, PascalCase types, and `TYPELAYOUT_*` macros. Signature-producing APIs are `consteval`, and compile-time properties should be guarded by `static_assert`. No formatter configuration is checked in, so preserve local formatting. Do not add emoji to code, docs, or commits.

## Testing Guidelines

Name new test files and CTest entries `test_<area>`. Define fixture types inside the test source and prefer compile-time assertions. `test/test_gate_negative.cpp` must not compile; its CTest passes only when the expected admission error appears. Add targets and `add_test` entries in `CMakeLists.txt`, and run the full suite before submitting. Update checked-in `.sig.hpp` fixtures only when signature changes are intentional and validated across affected platforms.

## Commit & Pull Request Guidelines

Use `type: imperative summary`, matching history (`feat:`, `fix:`, `test:`, `docs:`, `refactor:`, `chore:`). Keep commits focused. Pull requests should explain the rationale and user-visible effect, list the compiler and CTest commands run, link relevant issues, and include sample compatibility output when reports or signature diagnostics change. Ensure both standard CI and compatibility-pipeline checks pass.
