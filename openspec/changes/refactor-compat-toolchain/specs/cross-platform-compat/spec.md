## ADDED Requirements

### Requirement: Shared Type Definitions
The tools layer SHALL provide a shared header (`sig_types.hpp`) containing the canonical
definitions of `TypeEntry` and `PlatformInfo` used by both the signature export and
compatibility check components.

#### Scenario: TypeEntry shared across phases
- **GIVEN** a generated `.sig.hpp` file and `compat_check.hpp`
- **WHEN** both are included in a Phase 2 translation unit
- **THEN** they SHALL use the same `boost::typelayout::TypeEntry` definition from `sig_types.hpp`
- **AND** no `reinterpret_cast` SHALL be needed to bridge type entries between phases

#### Scenario: PlatformInfo convenience accessor
- **GIVEN** a generated `.sig.hpp` file for a platform
- **WHEN** the user calls `boost::typelayout::platform::<platform_ns>::get_platform_info()`
- **THEN** a `PlatformInfo` struct SHALL be returned containing all platform metadata and type entries

### Requirement: Declarative Type Registration
The library SHALL provide a macro `TYPELAYOUT_EXPORT_TYPES(...)` that generates a complete
Phase 1 export program from a list of type names.

#### Scenario: Macro-based export
- **GIVEN** a source file containing `TYPELAYOUT_EXPORT_TYPES(PacketHeader, SensorRecord)`
- **WHEN** compiled with P2996 Clang and executed with an output directory argument
- **THEN** a `.sig.hpp` file SHALL be generated containing signatures for both types

#### Scenario: Namespaced types
- **GIVEN** a source file containing `TYPELAYOUT_EXPORT_TYPES(ns::MyType)`
- **WHEN** compiled and executed
- **THEN** the type SHALL be registered with its stringified name `ns::MyType`

#### Scenario: Fallback to manual registration
- **GIVEN** a user with complex registration needs (templates, conditional types)
- **WHEN** the macro is insufficient
- **THEN** the user SHALL be able to write a manual `main()` using `SigExporter::add<T>()` directly
- **AND** both approaches SHALL produce compatible `.sig.hpp` output

### Requirement: Declarative Compatibility Check
The library SHALL provide a macro `TYPELAYOUT_CHECK_COMPAT(...)` that generates a complete
Phase 2 checker program from a list of platform namespace names.

#### Scenario: Macro-based comparison
- **GIVEN** included `.sig.hpp` headers for platforms `x86_64_linux_clang` and `arm64_macos_clang`
- **AND** a source file containing `TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang)`
- **WHEN** compiled with any C++17 compiler and executed
- **THEN** a compatibility report SHALL be printed to stdout

#### Scenario: Static assert mode
- **GIVEN** included `.sig.hpp` headers for two platforms
- **AND** a source file containing `TYPELAYOUT_ASSERT_COMPAT(plat_a, plat_b)`
- **WHEN** compiled with any C++17 compiler
- **THEN** compilation SHALL succeed if and only if all types have matching layout signatures
- **AND** a compile error SHALL identify the first mismatching type if layouts differ

## MODIFIED Requirements

### Requirement: CLI Toolchain
The library SHALL provide a command-line tool for automated cross-platform compatibility checking.

#### Scenario: Full pipeline check with user source
- **GIVEN** a user-written export source file using `TYPELAYOUT_EXPORT_TYPES` macro
- **WHEN** running `typelayout-compat check --source export.cpp --platforms x86_64-linux-clang,arm64-linux-clang`
- **THEN** the tool SHALL compile the export source on each platform (via Docker or local)
- **AND** collect `.sig.hpp` files from each platform
- **AND** generate and compile a Phase 2 compatibility checker
- **AND** output a compatibility report to stdout

#### Scenario: Full pipeline check with auto-detect (legacy)
- **GIVEN** a user types header `my_types.hpp` with struct definitions
- **WHEN** running `typelayout-compat check --types my_types.hpp --platforms x86_64-linux-clang`
- **THEN** the tool SHALL fall back to auto-detecting struct/class definitions via regex
- **AND** generate an export source, compile it, and produce signatures

#### Scenario: Export only
- **GIVEN** a user-written export source file
- **WHEN** running `typelayout-compat export --source export.cpp --output sigs/`
- **THEN** the tool SHALL compile and run the export source for the current platform
- **AND** generate a `.sig.hpp` file in the specified output directory

#### Scenario: Compare only
- **GIVEN** a directory with `.sig.hpp` files from multiple platforms
- **WHEN** running `typelayout-compat compare --sigs build/sigs/`
- **THEN** the tool SHALL generate and compile a Phase 2 checker
- **AND** output a compatibility report

#### Scenario: Platform registry
- **GIVEN** a `platforms.conf` file with platform definitions
- **WHEN** a user specifies `--platforms x86_64-linux-clang`
- **THEN** the tool SHALL look up the Docker image, compiler, and flags for that platform
- **AND** use Docker to build and run the export in the correct environment

### Requirement: CMake Integration
The library SHALL provide CMake utilities for automating the two-phase pipeline.

#### Scenario: Signature export target
- **GIVEN** a user calling `typelayout_add_sig_export(TARGET name SOURCE export.cpp ...)`
- **WHEN** the target is built and run
- **THEN** a `.sig.hpp` file SHALL be generated for the current platform

#### Scenario: Compatibility check target
- **GIVEN** a user calling `typelayout_add_compat_check(TARGET name SOURCE check.cpp SIGS_DIR ...)`
- **WHEN** the target is compiled
- **THEN** compilation SHALL succeed if and only if all types are compatible

#### Scenario: High-level pipeline
- **GIVEN** a user calling `typelayout_add_compat_pipeline(NAME proj EXPORT_SOURCE export.cpp PLATFORMS ...)`
- **WHEN** CMake configures and builds
- **THEN** Phase 1 and Phase 2 targets SHALL be created automatically
- **AND** a CTest test SHALL be registered for the compatibility check

### Requirement: GitHub Actions Integration
The library SHALL provide a reusable GitHub Actions workflow for CI/CD compatibility checking.

#### Scenario: Reusable workflow with user source
- **GIVEN** an external repository with an export source using `TYPELAYOUT_EXPORT_TYPES`
- **WHEN** the repository calls `uses: ximicpp/TypeLayout/.github/workflows/compat-check.yml@main`
- **WITH** `export_source: 'src/export_types.cpp'` and `platforms: 'x86_64-linux-clang,arm64-linux-clang'`
- **THEN** the workflow SHALL compile the user's export source on each platform
- **AND** compare all signatures in a final job
- **AND** upload a compatibility report artifact

#### Scenario: Reusable workflow with auto-detect (legacy)
- **GIVEN** an external repository with a types header
- **WHEN** the repository calls the workflow with `types_header: 'include/types.hpp'`
- **THEN** the workflow SHALL fall back to auto-detecting types from the header
