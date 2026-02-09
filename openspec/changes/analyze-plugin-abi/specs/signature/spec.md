## ADDED Requirements

### Requirement: Plugin ABI / ODR Detection Scenario Analysis

The signature system SHALL provide a formal analysis document explaining the relationship between the plugin ABI compatibility / ODR violation detection scenario and the two-layer signature architecture, covering both compile-time and runtime verification modes.

#### Scenario: Layout layer verifies binary ABI compatibility
- **GIVEN** a host application dynamically loading a plugin via dlopen/LoadLibrary
- **WHEN** host and plugin share struct definitions for data exchange
- **THEN** the analysis SHALL demonstrate that Layout signature (V1) is the primary ABI verification tool:
  - Layout match guarantees that host and plugin agree on byte layout (Theorem 3.2)
  - This prevents undefined behavior from reading/writing misaligned or differently-sized fields
  - Runtime verification mode: plugin exports its Layout signature string via dlsym, host compares at load time

#### Scenario: Definition layer detects ODR violations
- **GIVEN** host and plugin independently define `struct Config { ... }` with the same name
- **WHEN** the definitions differ (different field names, reordered fields, different inheritance)
- **THEN** the analysis SHALL demonstrate that Definition signature (V2) detects ODR violations:
  - Even if Layout signatures match (same byte layout), Definition signatures may differ
  - Definition mismatch with Layout match implies: binary compatible but structurally different (potential ODR violation)
  - This detection capability is unique — compilers and linkers typically cannot catch cross-TU ODR violations for struct definitions
  - Formal basis: Theorem 4.3 (Strict Refinement) — there exist types where Layout matches but Definition does not

#### Scenario: Two verification modes for plugin architectures
- **GIVEN** two deployment models: co-compiled (host + plugin built together) and independently compiled
- **WHEN** choosing verification strategy
- **THEN** the analysis SHALL explain:
  - Co-compiled: use `static_assert(definition_signatures_match<Host::Config, Plugin::Config>())` at compile time (strongest guarantee, both V1 and V2)
  - Independent: export Layout signature string at plugin boundary, compare at load time (V1 runtime verification)
  - The V3 projection theorem guarantees that compile-time Definition match (when possible) is strictly stronger than runtime Layout match
