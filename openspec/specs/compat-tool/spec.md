# compat-tool Specification

## Purpose
TBD - created by archiving change add-ci-compat-check-tool. Update Purpose after archive.
## Requirements
### Requirement: Cross-Platform Compatibility Check Tool

The system SHALL provide a complete toolset for verifying type layout compatibility across multiple platforms in CI environments.

#### Scenario: User registers types for compatibility checking
- **GIVEN** a user has types that need cross-platform compatibility verification
- **WHEN** the user creates a configuration header with `TYPELAYOUT_TYPES(Type1, Type2, ...)`
- **THEN** those types are registered for signature generation

#### Scenario: User specifies target platforms
- **GIVEN** a user wants to check compatibility on specific platforms
- **WHEN** the user adds `TYPELAYOUT_PLATFORMS(linux_x64, windows_x64, ...)`
- **THEN** the CI workflow runs on those specified platforms
- **AND** if no platforms specified, defaults to linux_x64 and windows_x64

#### Scenario: Signature generation on each platform
- **GIVEN** the signature generator is compiled on a target platform
- **WHEN** executed
- **THEN** it outputs a text file containing:
  - Platform identifier line: `__PLATFORM__ <platform-name>`
  - Architecture prefix line: `__ARCH__ <arch-prefix>`
  - For each registered type: `<TypeName> <Hash> <Size> <Align>`

#### Scenario: Cross-platform signature comparison
- **GIVEN** signature files from multiple platforms
- **WHEN** the compare tool is executed with all signature files
- **THEN** it compares the hash of each type across all platforms
- **AND** outputs a compatibility report showing:
  - List of compatible types (same hash on all platforms)
  - List of incompatible types (different hashes) with details
- **AND** returns exit code 0 if all types compatible
- **AND** returns exit code 1 if any type incompatible

### Requirement: Reusable GitHub Workflow

The system SHALL provide a reusable GitHub Actions workflow that can be called from external repositories.

#### Scenario: Zero-configuration usage
- **GIVEN** a user repository with `typelayout.config.hpp` in project root
- **WHEN** the user adds a workflow calling `uses: <org>/typelayout/.github/workflows/compat-check.yml@v1`
- **THEN** the workflow automatically finds and uses the config file

#### Scenario: Custom configuration path
- **GIVEN** a user wants to use a non-default config path
- **WHEN** the user specifies `with: config: 'path/to/config.hpp'`
- **THEN** the workflow uses the specified config file

#### Scenario: Custom platform selection
- **GIVEN** a user wants to check specific platforms
- **WHEN** the user specifies `with: platforms: 'linux-x64,macos-arm64'`
- **THEN** the workflow runs only on those specified platforms

### Requirement: Platform Support

The system SHALL support the following target platforms:

| Platform ID | OS | Architecture | Data Model |
|-------------|-----|--------------|------------|
| linux-x64 | Linux | x86_64 | LP64 |
| linux-arm64 | Linux | AArch64 | LP64 |
| windows-x64 | Windows | x86_64 | LLP64 |
| macos-arm64 | macOS | ARM64 | LP64 |

#### Scenario: Platform to CI runner mapping
- **GIVEN** a platform identifier
- **WHEN** the workflow processes the platform matrix
- **THEN** it maps to the appropriate GitHub Actions runner:
  - linux-x64 → ubuntu-latest
  - linux-arm64 → ubuntu-latest with QEMU
  - windows-x64 → windows-latest
  - macos-arm64 → macos-latest

### Requirement: User Configuration Interface

The system SHALL provide simple macros for user configuration.

#### Scenario: Type registration macro
- **GIVEN** user types `struct A`, `struct B`
- **WHEN** user writes `TYPELAYOUT_TYPES(A, B)`
- **THEN** a type list is defined for the signature generator

#### Scenario: Platform specification macro
- **GIVEN** user wants to target linux and windows
- **WHEN** user writes `TYPELAYOUT_PLATFORMS(linux_x64, windows_x64)`
- **THEN** the platforms are recorded for CI workflow selection

