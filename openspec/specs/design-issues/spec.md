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

### Member Names in Signatures (Resolved)
Signatures include field names by design, causing mismatches when fields are renamed.

**Rationale**:
- Field rename vs field swap have identical memory layouts but completely different semantics
- Example: `{int x; int y;}` renamed to `{int a; int b;}` vs swapped to `{int y; int x;}`
- Both have same offsets (0, 4) but swapped version reads data incorrectly
- Including names is a conservative safety measure that prevents silent data corruption
- Users who confirm only rename (not swap) can use future `get_structural_signature()` API

**Optional Enhancement**: Add `get_structural_signature<T>()` for users who explicitly want layout-only comparison (at their own risk).

### TYPELAYOUT_BIND String Format (Resolved)
TYPELAYOUT_BIND requires full signature strings which can be long.

**Rationale**:
- Hash-based binding would not solve cross-platform issues (different platforms = different layouts = different hashes)
- Full signature strings are self-describing and show exact layout details
- When mismatches occur, developers can see exactly what differs
- Hash binding loses readability and debugging value while providing no functional benefit
- The "cost" of long strings is justified by transparency and debuggability

**Optional Enhancement**: Provide `print_signature<T>()` utility or CLI tool to help generate signatures for copy-paste.
