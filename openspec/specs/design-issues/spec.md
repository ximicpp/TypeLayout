# design-issues Specification

## Purpose
Document known design issues, their impact on core use cases, and suggested improvements for the TypeLayout library.

## Core Use Cases

1. **Same-platform ABI Verification** - Verify binary compatibility on the same platform
2. **Cross-platform Data Compatibility** - Verify data structures are compatible across platforms (same arch)
3. **Network Protocol Verification** - Validate wire format of network messages
4. **Shared Memory Verification** - Ensure shared memory structures match
5. **Library ABI Upgrade Checking** - Detect breaking changes in library updates

## Requirements

### Requirement: Design Issue Tracking
The project SHALL maintain a documented list of known design issues and their impact on core use cases.

#### Scenario: Issue documentation
- **GIVEN** a design issue is identified
- **WHEN** the issue impacts one or more core use cases
- **THEN** the issue is documented with severity, affected use cases, and suggested remediation

### Requirement: Core Use Case Definition
The library SHALL clearly define its core use cases and design decisions SHALL be evaluated against them.

#### Scenario: Use case prioritization
- **GIVEN** a proposed design change
- **WHEN** evaluating the change
- **THEN** impact on all core use cases is considered

## Known Issues

### High Priority

| ID | Issue | Affected Use Cases | Status |
|----|-------|-------------------|--------|
| H1 | No runtime verification API for network/file data | Network, File I/O | Open |

### Medium Priority

| ID | Issue | Affected Use Cases | Status |
|----|-------|-------------------|--------|
| M1 | Member names in signatures cause false incompatibility on rename | ABI Upgrade | Open |
| M2 | TYPELAYOUT_BIND requires platform-specific signature strings | All | Open |
| M3 | Poor error messages on signature mismatch | All | Open |

### Low Priority

| ID | Issue | Suggested Improvement | Status |
|----|-------|----------------------|--------|
| L1 | No signature format versioning | Add version prefix | Open |
| L2 | Dual-hash may be overkill for most cases | Make optional | Open |

## Design Decisions

### Architecture Prefix Design (Resolved)
The `[64-le]` architecture prefix includes only pointer size and endianness, not OS-specific information.

**Rationale**: 
- Same architecture (e.g., 64-le) across different OS (Windows, Linux, macOS) produces identical signatures
- This is correct behavior as memory layouts are identical on same-architecture systems
- The prefix serves as a necessary safety measure to prevent 32-bit vs 64-bit mismatches