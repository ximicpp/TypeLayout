## ADDED Requirements

### Requirement: Killer Application Scenarios

The library documentation SHALL clearly identify and explain the primary killer application scenarios:

**Tier 1 - Dual Killer Applications (🥇):**
1. **🥇-A Cross-Process Shared Memory Verification** - Zero-overhead layout validation for IPC
2. **🥇-B Zero-Copy Network Protocol** - IDL-free, zero-encode/decode network transmission

**Tier 2 - High-Value Applications:**
3. **🥈 Binary File Format Versioning** - Automatic compatibility detection for persistent storage
4. **🥉 Network Protocol ABI Safety** - Runtime verification for distributed systems

**Tier 3 - Specialized Applications:**
5. **Compile-Time ABI Protection** - Static guarantees for library developers
6. **Hardware Register Mapping Verification** - Offset validation for embedded systems
7. **Cross-Language FFI Verification** - Standard layout descriptions for polyglot systems

#### Scenario: Documentation provides killer app guidance
- **WHEN** a user reads the library documentation
- **THEN** they can identify which killer application best fits their use case
- **AND** find relevant code examples for each scenario

#### Scenario: Shared memory demo is available
- **WHEN** a user wants to use TypeLayout for shared memory verification
- **THEN** a complete working example demonstrates the pattern
- **AND** the example shows both producer and consumer processes

### Requirement: Zero-Copy Network Protocol Support

The library SHALL provide concepts and utilities for zero-copy network transmission:

1. A `ZeroCopyTransmittable` concept that combines `Portable<T>` with `std::is_trivially_copyable_v<T>`
2. Documentation explaining the pattern for embedding layout hashes in packet headers
3. Examples demonstrating sender/receiver verification workflow

#### Scenario: Zero-copy concept validates types
- **WHEN** a user applies `ZeroCopyTransmittable<T>` concept to a type
- **THEN** the concept validates the type is safe for direct memory transmission
- **AND** non-portable or non-trivially-copyable types are rejected at compile time

#### Scenario: Network protocol example available
- **WHEN** a user wants to implement zero-copy network protocol
- **THEN** a complete example demonstrates the send/receive pattern with hash verification
- **AND** the example shows how to handle layout mismatch errors gracefully

### Requirement: Unified Value Proposition

The library documentation SHALL communicate a unified value proposition:

> "Native C++ structs as zero-overhead data protocols — No IDL, No codegen, Automatic layout verification"

This positioning differentiates TypeLayout from:
- **Protobuf/JSON**: By eliminating encode/decode overhead
- **Cap'n Proto/FlatBuffers**: By eliminating IDL files and code generation
- **Manual versioning**: By providing automatic layout change detection

#### Scenario: Competitive positioning is clear
- **WHEN** a user compares TypeLayout to existing solutions
- **THEN** the documentation provides clear comparison tables
- **AND** the unique value proposition is immediately understandable
