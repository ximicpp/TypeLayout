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

*No open issues*

### Medium Priority

*No open issues*

### Low Priority

*No open issues*

## Documentation Enhancements

| ID | Enhancement | Priority |
|----|-------------|----------|
| D1 | Add network protocol verification examples | Medium |
| D2 | Add file format verification examples | Medium |
| D3 | Add best practices guide for runtime scenarios | Medium |

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

### Signature Format Versioning (Resolved - Not Needed)
Signatures do not include a format version prefix.

**Rationale**:
- Signatures are generated at compile-time, not stored persistently
- When library upgrades change the format, new signatures are generated automatically
- Users update `TYPELAYOUT_BIND` strings as part of normal upgrade process
- This is expected behavior: library change = code update required
- If runtime verification (H1) is implemented, versioning would be part of that design
- Adding version prefix to compile-time-only signatures adds complexity without benefit

### Error Messages on Mismatch (Known Limitation)
`static_assert` failures show only "Layout mismatch for Type" without signature details.

**Rationale**:
- This is a C++ language limitation, not a library design issue
- `static_assert` messages must be string literals; dynamic content is not allowed
- Cannot include actual vs expected signatures in the error message

**Mitigations**:
- Documentation guides users to use `get_layout_signature<T>()` to inspect actual signatures
- Example code and troubleshooting guide help users diagnose mismatches
- Future: Add `print_signature<T>()` utility for debugging convenience

### Dual-Hash API (Resolved - Correct Design)
Library provides both single-hash and dual-hash verification APIs.

**Rationale**:
- Layered choice: `get_layout_hash<T>()` (64-bit) for simple cases, `get_layout_verification<T>()` (128-bit+) for high-security
- Users choose based on their security requirements
- Dual-hash is optional, not mandatory
- No API complexity: simple use cases use simple API
- Security-sensitive domains (finance, medical) benefit from higher collision resistance

### Runtime Verification (Resolved - Already Supported)
Initial concern: No runtime verification API for network/file data.

**Analysis**:
- `get_layout_hash<T>()` returns `constexpr uint64_t` which IS usable at runtime
- Hash can be embedded in data structures, files, or network packets
- Receiver compares embedded hash with local `get_layout_hash<T>()`

**Example - Network Protocol**:
```cpp
struct Packet { uint64_t layout_hash; char payload[]; };
// Sender: pkt.layout_hash = get_layout_hash<Payload>();
// Receiver: if (pkt.layout_hash == get_layout_hash<Payload>()) { /* safe */ }
```

**Example - File Format**:
```cpp
struct FileHeader { char magic[4]; uint64_t layout_hash; };
// Write: hdr.layout_hash = get_layout_hash<MyData>();
// Read: if (hdr.layout_hash == get_layout_hash<MyData>()) { /* compatible */ }
```

**Conclusion**: API already supports runtime verification. Need documentation examples (see D1, D2, D3).
