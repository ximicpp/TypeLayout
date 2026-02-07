# Change: Clean Code Style and Simplify

## Why
Current code has verbose AI-generated comments, redundant helpers, and
over-documented obvious logic. Strip to professional C++ library style.

## What Changes

### Comments / style
- Remove `=====` banner separators — use blank lines
- Remove `///` doc-comments that just restate function name
- Remove emoji in source files (⚠️ etc.)
- Remove numbered comments ("1.", "2.", "3.") on struct definitions
- Remove "Helper:" prefixes — function name should be self-documenting
- Shorten file headers to 2 lines (copyright + license)
- Remove `@brief` doxygen style — not using doxygen
- Remove `DESIGN:` block comments — design is in spec, not code
- Remove `Limitation (v1):` — this is the only version

### Code simplification
- **config.hpp**: Remove `has_determinable_layout_v` (unused). Remove `default_signature_mode` (unused). Remove `always_false_v` (unused, only `always_false` struct used). Remove `BOOST_TYPELAYOUT_CPLUSPLUS` macro (P2996 requires C++26, the C++20 check is pointless). Remove version macros (no consumer uses them).
- **compile_string.hpp**: Remove `fixed_string` struct (unused anywhere). Remove `c_str()` (just use `.value` directly). Remove `string_view` constructor (unused).
- **reflection_helpers.hpp**: `has_bases()` is only a wrapper for `get_base_count<T>() > 0` — inline it at call sites and remove.
- **signature.hpp**: `get_arch_prefix()` handles exotic pointer sizes nobody will ever use — simplify to just 32/64 with static_assert fallback.

### Example
- Remove emoji from comments
- Simplify numbered comments to plain descriptions

## Impact
- Affected specs: signature (no API changes)
- Affected code: all `.hpp` files, `cross_platform_check.cpp`
- **Zero** API surface changes — purely internal cleanup
