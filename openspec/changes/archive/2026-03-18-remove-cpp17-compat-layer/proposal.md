## Why

The project's core value proposition is C++26 static reflection (P2996). The current "C++17-compatible tools layer" adds architectural complexity (dual-path padding detection invariant, two CMake test build modes, layer separation rules) but provides no real user benefit: anyone using TypeLayout already needs a P2996 compiler to generate signatures. The C++17 layer is not a competitive differentiator and its maintenance cost is disproportionate to its value. Removing it simplifies the codebase, build system, documentation, and contributor onboarding.

## What Changes

- **BREAKING**: Remove the guarantee that `safety_level.hpp`, `sig_parser.hpp`, `sig_types.hpp`, `compat_check.hpp`, `platform_detect.hpp`, and `detail/foreach.hpp` can be compiled independently with only C++17.
- **BREAKING**: Remove the `compat_check` example target's C++17-only build mode; the compat check example now requires P2996 like everything else.
- Convert `test_rt_padding.cpp` and `test_compat_check.cpp` from C++17-only tests to standard P2996 tests (link to `typelayout` interface library, use standard compile flags).
- Update CMakeLists.txt: remove manual `-std=c++17 -stdlib=libc++` for the two C++17-only test targets and the compat_check example; unify all targets to link `typelayout`.
- Update `TypeLayoutCompat.cmake`: Phase 2 (`typelayout_add_compat_check`) no longer uses `cxx_std_17`; uses the same P2996 flags as Phase 1.
- Remove all "C++17, no P2996" / "C++17 only" / "C++17 is sufficient" annotations from code comments and documentation.
- Update `AGENTS.md` and `.codemaker/rules/rules.mdc`: remove layer separation rules, C++17-only test patterns, and dual build mode documentation.
- Update `README.md`, `docs/quickstart.md`, `docs/api-reference.md`, `docs/applications.md`, `example/README.md`: remove C++17 claims.
- Update `docs/paper/` sections that reference C++17-only Phase 2.
- Remove the `compat_auto.hpp` file from `example/` (move into library headers or keep as-is but remove C++17 claims in comments).

## Capabilities

### New Capabilities
(none)

### Modified Capabilities
(none -- this is a build/documentation simplification, not a requirement-level change)

## Impact

- **Code**: Comment changes in ~8 header files; build config changes in CMakeLists.txt and TypeLayoutCompat.cmake.
- **Tests**: 2 tests change build mode (C++17 -> P2996). Total test count unchanged (11). Test content unchanged.
- **Examples**: `compat_check` example now requires P2996 compiler. Users who previously ran Phase 2 with a C++17-only compiler must now use P2996 for everything.
- **Documentation**: ~15 files need "C++17" reference removal or rewording.
- **API**: No API changes. All public symbols remain.
- **Breaking for users**: Anyone relying on compiling Phase 2 tools with a non-P2996 compiler will need to switch. This is expected to affect zero real users.
