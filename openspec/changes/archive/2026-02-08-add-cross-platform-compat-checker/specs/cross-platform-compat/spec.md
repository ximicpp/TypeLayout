# cross-platform-compat Specification

> **Layer**: Tools (`<boost/typelayout/tools/...>`)

## Purpose

Provides a pure C++ two-phase pipeline for determining whether types are
binary-compatible across a set of target platforms, enabling serialization-free
data transfer (shared memory, mmap, network sockets, file I/O).

Depends on the core `signature` capability but does not modify it.

## ADDED Requirements

### Requirement: Platform Detection
The library SHALL provide automatic detection of the current platform identity.

#### Scenario: Auto-detected platform name
- **GIVEN** a compilation on x86-64 Linux with Clang
- **WHEN** calling `get_platform_name()`
- **THEN** the result SHALL be `"x86_64_linux_clang"`

#### Scenario: Platform name format
- **GIVEN** any supported platform
- **WHEN** calling `get_platform_name()`
- **THEN** the result SHALL follow the format `{arch}_{os}_{compiler}`
- **AND** the result SHALL be a valid C++ identifier (lowercase + underscore only)

#### Scenario: Manual platform name override
- **GIVEN** a user who specifies a custom platform name
- **WHEN** constructing `SigExporter("custom_platform")`
- **THEN** the custom name SHALL be used instead of auto-detection

### Requirement: Signature Export
The library SHALL provide a tool to export TypeLayout signatures to C++ header files.

#### Scenario: Basic export
- **GIVEN** a set of user-defined types registered via `SigExporter::add<T>(name)`
- **WHEN** calling `SigExporter::write(path)`
- **THEN** a `.sig.hpp` header file SHALL be generated at the specified path
- **AND** the file SHALL contain `inline constexpr const char[]` variables for each type's layout and definition signatures
- **AND** the file SHALL be compilable by any C++17 or later compiler (no P2996 required)

#### Scenario: Generated header structure
- **GIVEN** a type `PacketHeader` exported on platform `x86_64_linux_clang`
- **WHEN** the `.sig.hpp` is generated
- **THEN** it SHALL contain:
  - `inline constexpr const char PacketHeader_layout[] = "...";`
  - `inline constexpr const char PacketHeader_definition[] = "...";`
  - Platform metadata: `platform_name`, `arch_prefix`, `pointer_size`, `sizeof_long`, `sizeof_wchar_t`, `sizeof_long_double`
  - A `TypeEntry` struct and `types[]` registry array for runtime iteration
- **AND** all content SHALL be wrapped in `namespace boost::typelayout::platform::{platform_name}`

#### Scenario: Signature values are real TypeLayout signatures
- **GIVEN** a type T
- **WHEN** exported via `SigExporter::add<T>(name)`
- **THEN** the stored layout signature SHALL be identical to `get_layout_signature<T>()`
- **AND** the stored definition signature SHALL be identical to `get_definition_signature<T>()`

### Requirement: Compile-Time Compatibility Check
The library SHALL provide constexpr functions for comparing signatures across platforms.

#### Scenario: Layout match check
- **GIVEN** two `constexpr const char[]` layout signatures from different platforms
- **WHEN** calling `constexpr sig_match(sig_a, sig_b)`
- **THEN** the result SHALL be `true` if and only if the string contents are identical
- **AND** the function SHALL be usable in `static_assert`

#### Scenario: Static assert workflow
- **GIVEN** `.sig.hpp` headers from platform A and platform B
- **WHEN** both are `#include`-d in a single translation unit
- **THEN** `static_assert(sig_match(plat_a::Type_layout, plat_b::Type_layout))` SHALL compile if and only if the signatures match
- **AND** a clear error message SHALL be provided on mismatch

### Requirement: Runtime Compatibility Report
The library SHALL provide a runtime reporter for human-readable compatibility analysis.

#### Scenario: Multi-platform report
- **GIVEN** signature data from N platforms registered via `CompatReporter::add_platform()`
- **WHEN** calling `CompatReporter::print_report()`
- **THEN** the output SHALL include:
  - A list of compared platforms with their metadata
  - A per-type compatibility matrix showing MATCH/DIFFER for layout and definition
  - A verdict per type: "Serialization-free" or "Needs serialization"
  - Summary statistics: count and percentage of compatible types

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
