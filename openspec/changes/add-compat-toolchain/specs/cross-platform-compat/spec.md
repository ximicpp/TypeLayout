## MODIFIED Requirements

### Requirement: CMake Integration
The library SHALL provide CMake utilities for automating the two-phase pipeline.

#### Scenario: Signature export target
- **GIVEN** a user calling `typelayout_add_sig_export(TARGET name ...)`
- **WHEN** the target is built and run
- **THEN** a `.sig.hpp` file SHALL be generated for the current platform

#### Scenario: Compatibility check target
- **GIVEN** a user calling `typelayout_add_compat_check(TARGET name SIGNATURES sig1.hpp sig2.hpp)`
- **WHEN** the target is compiled
- **THEN** compilation SHALL succeed if and only if all types are compatible across all listed signature files

## ADDED Requirements

### Requirement: CLI Toolchain
The library SHALL provide a command-line tool for automated cross-platform compatibility checking.

#### Scenario: Full pipeline check
- **GIVEN** a user types header `my_types.hpp` with type registrations
- **WHEN** running `typelayout-compat check --types my_types.hpp --platforms x86_64-linux-clang,arm64-macos-clang`
- **THEN** the tool SHALL automatically build sig_export for each platform (via Docker or local)
- **AND** collect `.sig.hpp` files from each platform
- **AND** generate and compile a Phase 2 compatibility checker
- **AND** output a compatibility report to stdout

#### Scenario: Export only
- **GIVEN** a user types header `my_types.hpp`
- **WHEN** running `typelayout-compat export --types my_types.hpp --output sigs/`
- **THEN** the tool SHALL compile sig_export for the current platform
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
- **AND** use Docker to build and run sig_export in the correct environment

### Requirement: GitHub Actions Integration
The library SHALL provide a reusable GitHub Actions workflow for CI/CD compatibility checking.

#### Scenario: Reusable workflow
- **GIVEN** an external repository that uses TypeLayout
- **WHEN** the repository calls `uses: ximicpp/TypeLayout/.github/workflows/compat-check.yml@main`
- **THEN** the workflow SHALL export signatures on each specified platform
- **AND** compare all signatures in a final job
- **AND** report results as a GitHub Actions job summary
