## MODIFIED Requirements

### Requirement: Two-Layer Signature Architecture
The library SHALL provide two distinct signature layers: Layout Compatibility (Layer 1 - Core) and Serialization Compatibility (Layer 2 - Utility).

**Architecture positioning:**
- **Layer 1 (Core)**: Layout Signature is the library's core value proposition
- **Layer 2 (Utility)**: Serialization Check is an application built on top of Layer 1

**Header organization:**
- Core functionality: `<boost/typelayout/core/...>`
- Utility functionality: `<boost/typelayout/util/...>`

#### Scenario: Layer 1 - Layout Compatibility (Core)
- **GIVEN** a user needs same-process or same-platform type checking
- **WHEN** using `get_layout_signature<T>()`
- **THEN** the signature SHALL reflect memory layout (size, alignment, field offsets)
- **AND** platform SHALL be implicitly the current build platform
- **AND** the function SHALL be available via `<boost/typelayout/typelayout.hpp>`

#### Scenario: Layer 2 - Serialization Compatibility (Utility)
- **GIVEN** a user needs cross-process/cross-machine memcpy-safe data transfer
- **WHEN** using `is_serializable_v<T, PlatformSet>`
- **THEN** a platform set P SHALL be explicitly specified
- **AND** the type SHALL be validated against Layer 1 (Layout) requirements first
- **AND** additional serialization constraints SHALL be applied
- **AND** the function SHALL be available via `<boost/typelayout/typelayout_util.hpp>`

#### Scenario: Layer hierarchy relationship
- **GIVEN** the two layers
- **THEN** Serialization compatibility(P) SHALL imply Layout compatibility
- **AND** Layout compatibility SHALL NOT imply Serialization compatibility(P)
- **AND** the relationship SHALL be: `Serialization(P) ⊂ Layout`
- **AND** utility code SHALL depend on core code, not vice versa

## ADDED Requirements

### Requirement: Header Organization
The library SHALL organize headers into `core/` and `util/` subdirectories reflecting the layered architecture.

#### Scenario: Core headers
- **GIVEN** the library's core functionality
- **THEN** the following headers SHALL be in `core/`:
  - `layout_signature.hpp` - signature generation
  - `compile_string.hpp` - compile-time string utilities
  - `hash.hpp` - hash algorithms
  - `config.hpp` - configuration macros
  - `reflection_helpers.hpp` - P2996 reflection utilities

#### Scenario: Utility headers
- **GIVEN** the library's utility functionality
- **THEN** the following headers SHALL be in `util/`:
  - `serialization_check.hpp` - serialization safety checking
  - `serialization_traits.hpp` - serialization type traits
  - `platform_set.hpp` - platform set definitions

#### Scenario: Top-level convenience headers
- **GIVEN** users who want simple includes
- **THEN** the following top-level headers SHALL be provided:
  - `typelayout.hpp` - includes core only
  - `typelayout_util.hpp` - includes util (which depends on core)
  - `typelayout_all.hpp` - includes everything

### Requirement: Backward Compatibility Layer
The library SHALL provide backward-compatible header paths for deprecated locations.

#### Scenario: Deprecated header includes
- **GIVEN** an existing user including `<boost/typelayout/detail/serialization_status.hpp>`
- **WHEN** compiling with the new version
- **THEN** the code SHALL still compile
- **AND** a deprecation warning SHALL be emitted
- **AND** the warning SHALL suggest the new header path
