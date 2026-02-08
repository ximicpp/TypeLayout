## 1. Shared Type Definitions
- [ ] 1.1 Create `include/boost/typelayout/tools/sig_types.hpp`
  - `TypeEntry` struct (name, layout_sig, definition_sig)
  - `PlatformInfo` struct (all metadata + types array + get helper)
  - C++17 compatible, no P2996

## 2. Declarative Macros
- [ ] 2.1 Implement `TYPELAYOUT_EXPORT_TYPES(...)` macro in `sig_export.hpp`
  - Self-contained FOR_EACH (no Boost.PP)
  - Generates complete main() with SigExporter
  - Support up to 64 types
- [ ] 2.2 Create `include/boost/typelayout/tools/compat_auto.hpp`
  - `TYPELAYOUT_CHECK_COMPAT(plat1, plat2, ...)` — runtime report
  - `TYPELAYOUT_ASSERT_COMPAT(plat1, plat2, ...)` — static_assert only

## 3. Update Signature Generator
- [ ] 3.1 Update `SigExporter::write_*()` to emit `#include <boost/typelayout/tools/sig_types.hpp>`
  - Remove inline TypeEntry from generated .sig.hpp
  - Add `get_platform_info()` to generated .sig.hpp
- [ ] 3.2 Update `compat_check.hpp` to use `sig_types.hpp`
  - Remove local TypeEntry/PlatformData, use shared definitions
  - Remove reinterpret_cast

## 4. Update Examples
- [ ] 4.1 Rewrite `example/cross_platform_check.cpp` using `TYPELAYOUT_EXPORT_TYPES`
- [ ] 4.2 Rewrite `example/compat_check.cpp` using `TYPELAYOUT_CHECK_COMPAT`
- [ ] 4.3 Regenerate `example/sigs/*.sig.hpp` with new format

## 5. Simplify CLI
- [ ] 5.1 Rewrite `tools/typelayout-compat`
  - `export --source <file.cpp>` — compile+run on platform
  - `compare --source <file.cpp>` — compile+run locally
  - `check --export-source <file.cpp> --check-source <file.cpp> --platforms ...`
  - Remove generate_export_source(), remove template dependencies
- [ ] 5.2 Delete `tools/sig_export_template.cpp.in`
- [ ] 5.3 Delete `tools/compat_check_template.cpp.in`

## 6. Update CMake
- [ ] 6.1 Add `typelayout_add_compat_pipeline()` to TypeLayoutCompat.cmake
  - High-level function: one call creates Phase 1 + Phase 2 targets
  - Auto-register CTest
- [ ] 6.2 Update existing CMake functions for new header structure

## 7. Update CI
- [ ] 7.1 Rewrite `.github/workflows/compat-check.yml`
  - Accept `export_source` + `check_source` inputs
  - Compile user source directly (no template generation)

## 8. Documentation
- [ ] 8.1 Rewrite `tools/README.md`
- [ ] 8.2 Update `example/README.md`
- [ ] 8.3 Create example using TYPELAYOUT_EXPORT_TYPES macro
- [ ] 8.4 Create example using TYPELAYOUT_CHECK_COMPAT macro

## 9. Testing
- [ ] 9.1 Test `TYPELAYOUT_EXPORT_TYPES` macro (compile with P2996 + run)
- [ ] 9.2 Test `TYPELAYOUT_CHECK_COMPAT` macro (compile C++17 + run)
- [ ] 9.3 Test `TYPELAYOUT_ASSERT_COMPAT` macro (compile-only)
- [ ] 9.4 E2E: CLI export + compare with new workflow