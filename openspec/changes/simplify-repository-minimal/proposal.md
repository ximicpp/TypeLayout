# Change: Simplify repository to minimal two-layer core

## Why
The repository has accumulated significant cruft: outdated documentation (referencing the old 3-mode system), legacy test files testing removed APIs, non-core utilities (hash, verification, collision detection), and auxiliary artifacts (videos, slides, benchmarks, analysis docs). This makes the codebase hard to understand and maintain. We need a clean, minimal repository focused exclusively on the v2.0 two-layer signature core.

## What Changes

### DELETE — Documentation (all outdated)
- **`doc/` directory**: Entire directory removed (all content references old APIs)
- **`CONTRIBUTING.md`**: Remove (can be recreated later)
- **`CHANGELOG.md`**: Remove (can be recreated later)

### DELETE — Legacy tests (reference removed v1.0 APIs)
- `test/test_physical_mode.cpp` — uses removed `physical_signatures_match`, `signatures_match`
- `test/test_signature_modes.cpp` — uses removed `signatures_match`, `hashes_match`, `LayoutMatch`
- `test/test_all_types.cpp` — uses removed `signatures_match`, `TYPELAYOUT_BIND`, `LayoutMatch`, `LayoutHashMatch`
- `test/test_signature_comprehensive.cpp` — uses removed `signatures_match`
- `test/test_signature_extended.cpp` — uses removed APIs
- `test/test_stress.cpp` — uses removed `hashes_match`
- `test/test_signature_size.cpp` — uses `get_layout_signature` (compatible but redundant)
- `test/test_alignment.cpp` — uses `get_layout_signature` (compatible but redundant)
- `test/test_complex_cases.cpp` — uses `get_layout_signature` (compatible but can be simplified)
- `test/test_constexpr_limits.cpp` — uses `get_layout_signature` (compatible but can be simplified)
- `test/test_anonymous_member.cpp` — uses `get_layout_signature` (compatible but can be simplified)
- `test/test_user_defined_types.cpp` — uses `get_layout_signature` (compatible but can be simplified)
- `test/boost_test_typelayout.cpp` — uses removed `signatures_match`, `get_layout_verification`
- `test/analyze_signature.cpp` — analysis tool, not a test
- `test/CMakeLists.txt` — old test CMake config
- `test/Jamfile` — old test Jamfile

### DELETE — Non-core code
- **`include/boost/typelayout/utils/hash.hpp`** — hash utilities (FNV-1a, DJB2)
- **`include/boost/typelayout/core/verification.hpp`** — dual-hash verification, collision detection
- **`include/boost/typelayout/core/concepts.hpp`** — concepts (can be recreated minimally if needed)
- Hash-related functions in `signature.hpp`: `get_layout_hash`, `layout_hashes_match`, `get_definition_hash`, `definition_hashes_match`, hash variable templates, hash macros
- `_cstr` functions in `signature.hpp`: `get_layout_signature_cstr`, `get_definition_signature_cstr`

### DELETE — Auxiliary artifacts
- **`bench/`** — compile-time benchmarks
- **`scripts/`** — build scripts
- **`tools/`** — typelayout_tool.cpp
- **`meta/`** — Boost metadata
- **`example/`** — all examples (reference non-core hash APIs)
- **`Jamfile`** — root Jamfile (Boost.Build)
- **`cmake/`** — CMake package config template

### KEEP — Minimal core
- **`include/boost/typelayout/core/config.hpp`** — configuration, SignatureMode enum
- **`include/boost/typelayout/core/compile_string.hpp`** — CompileString utility
- **`include/boost/typelayout/core/reflection_helpers.hpp`** — layout/definition content engines
- **`include/boost/typelayout/core/type_signature.hpp`** — TypeSignature template
- **`include/boost/typelayout/core/signature.hpp`** — stripped to core: `get_layout_signature`, `get_definition_signature`, `layout_signatures_match`, `definition_signatures_match`
- **`include/boost/typelayout/typelayout.hpp`** — primary include header (simplified)
- **`include/boost/typelayout.hpp`** — top-level include
- **`test/test_two_layer.cpp`** — primary v2.0 test suite
- **`CMakeLists.txt`** — simplified for tests only
- **`README.md`** — kept and updated
- **`.gitignore`** — kept
- **`LICENSE` / `LICENSE_1_0.txt`** — kept
- **`AGENTS.md`** — kept
- **`openspec/`** — kept
- **`.github/`** — kept (CI)

## Impact
- Affected specs: `signature`, `repository-structure`, `documentation`, `build-system`, `ci`
- **BREAKING**: Removes hash, verification, concepts, all examples, all old tests
- Repository size dramatically reduced
- Clear, minimal codebase focused on the two-layer signature core