## Context

TypeLayout currently maintains a two-layer architecture:
- **Core layer** (C++26/P2996): signature generation, layout_traits, reflection helpers
- **Tools layer** (partially C++17): sig_parser, safety_level, compat_check, platform_detect

The C++17 layer exists to support a "Phase 2" workflow where signature comparison can run on any C++17 compiler. This design adds complexity:
1. Dual-path padding detection (bitmap vs string parser) with a cross-validating static_assert
2. Two CMake test build modes (P2996 link vs manual C++17 flags)
3. Layer separation rules in AGENTS.md/rules.mdc
4. Documentation must explain which headers need P2996 and which do not

All of this serves a use case (Phase 2 on a non-P2996 compiler) that has no real users.

## Goals / Non-Goals

**Goals:**
- Unify all code to require C++26/P2996 -- no special C++17-only build paths
- Simplify CMake: all tests link `typelayout`, all use same compile flags
- Remove "C++17" annotations from code comments, headers, and documentation
- Simplify AGENTS.md and rules.mdc by removing layer separation rules
- Reduce cognitive load for contributors

**Non-Goals:**
- Deleting any header files (sig_parser.hpp, safety_level.hpp, etc. stay)
- Removing any public API symbols
- Changing signature format or behavior
- Removing the cross-platform pipeline (Phase 1 + Phase 2 remain, just both require P2996 now)
- Removing the dual-path padding static_assert (it remains as a correctness check, just no longer motivated by C++17 compatibility)

## Decisions

### D1: Keep all existing header files

**Decision**: No header files are deleted. All tools headers remain in `include/boost/typelayout/tools/`.

**Rationale**: These files contain valuable functionality (safety classification, compat reporting, signature parsing). The change is about removing the *build-mode guarantee*, not the code. The files will simply no longer carry "C++17, no P2996" comments.

**Alternative considered**: Moving sig_parser/safety_level into `detail/` since they're no longer independently usable. Rejected: they're already well-placed, and moving them would break existing include paths.

### D2: Unify CMake test build mode

**Decision**: Convert `test_rt_padding` and `test_compat_check` from:
```cmake
target_include_directories(... PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(... PRIVATE -std=c++17 -stdlib=libc++)
target_link_options(... PRIVATE -stdlib=libc++)
```
To:
```cmake
target_link_libraries(... PRIVATE typelayout)
```

Also convert `compat_check` example target to link `typelayout`.

**Rationale**: Uniform build system. The `typelayout` interface library already provides include directories and compile flags.

### D3: Update test properties

**Decision**: Change the two converted tests from `TIMEOUT 30` to `TIMEOUT 120` to match all other P2996 tests.

**Rationale**: P2996 compilation is significantly slower than C++17. The 30s timeout was chosen because C++17 compilation is fast. Now that they compile with P2996, they need the same 120s budget.

### D4: Keep dual-path padding static_assert

**Decision**: The `static_assert` in `layout_traits.hpp` that cross-validates `compute_has_padding<T>()` (bitmap) and `sig_has_padding()` (parser) remains.

**Rationale**: This is a valuable correctness invariant regardless of C++17 compatibility. Two independent implementations checking the same property catches bugs. The motivation changes from "layer separation" to "defense in depth".

### D5: Update compat_auto.hpp and TypeLayoutCompat.cmake

**Decision**: 
- `example/compat_auto.hpp` keeps its code but loses C++17 comments
- `TypeLayoutCompat.cmake` Phase 2 function (`typelayout_add_compat_check`) changes from `target_compile_features(... PRIVATE cxx_std_17)` to linking `typelayout` interface library (same as Phase 1)

### D6: Documentation sweep

**Decision**: Systematic removal of all C++17 references across:
- Source file header comments (~6 files)
- README.md, AGENTS.md, rules.mdc
- docs/quickstart.md, docs/api-reference.md, docs/applications.md
- docs/paper/sec1, sec5, sec8
- example/README.md
- docs/conference-proposal.md

The narrative changes from "Phase 2 runs on any C++17 compiler" to "the entire library requires C++26/P2996".

## Risks / Trade-offs

- **[Risk] Users running Phase 2 with C++17-only compilers** -> Mitigation: This scenario has no known real users. The `.sig.hpp` output files remain plain C++ headers with `const char*` arrays, so they're technically still parseable by C++17 code -- we just don't officially support or test it.
- **[Risk] Large number of files to edit** -> Mitigation: Changes are mechanical (comment removal, CMake simplification). No logic changes. Low risk of introducing bugs.
- **[Risk] Paper/conference-proposal accuracy** -> Mitigation: Update paper sections to accurately describe the unified C++26 requirement. The cross-platform pipeline concept still works; it just requires P2996 on both ends.
