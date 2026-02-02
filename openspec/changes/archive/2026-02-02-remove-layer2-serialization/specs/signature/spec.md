# Signature Specification Delta - Remove Layer 2

## REMOVED Requirements

### Requirement: Two-Layer Signature Architecture
**Reason**: Simplifying to single-layer (Layout only). Serialization compatibility is out of scope.
**Migration**: Use only `get_layout_signature<T>()` for layout verification.

### Requirement: Platform-Relative Serialization Compatibility
**Reason**: Layer 2 feature not implemented, removed from spec.
**Migration**: For serialization safety, use fixed-width types and manual validation.

### Requirement: Serialization Compatibility Check
**Reason**: Layer 2 feature not implemented, removed from spec.
**Migration**: Use `std::is_trivially_copyable_v<T>` for basic serialization checks.

### Requirement: Serialization Signature Generation
**Reason**: Layer 2 feature not implemented, removed from spec.
**Migration**: N/A - feature not available.

### Requirement: Platform Constraints (Required)
**Reason**: Layer 2 feature not implemented, removed from spec.
**Migration**: N/A - feature not available.

### Requirement: Predefined Platform Sets
**Reason**: Layer 2 feature not implemented, removed from spec.
**Migration**: N/A - feature not available.

### Requirement: is_serializable Trait
**Reason**: Layer 2 feature not implemented, removed from spec.
**Migration**: Use `std::is_trivially_copyable_v<T>` as alternative.

### Requirement: Serialization Blocker Diagnostic
**Reason**: Layer 2 feature not implemented, removed from spec.
**Migration**: N/A - feature not available.

## MODIFIED Requirements

### Requirement: Layout Signature Architecture
The library SHALL provide layout signature generation for compile-time memory layout analysis.

#### Scenario: Layout Compatibility
- **GIVEN** a user needs same-process or same-platform type checking
- **WHEN** using `get_layout_signature<T>()`
- **THEN** the signature SHALL reflect memory layout (size, alignment, field offsets)
- **AND** platform SHALL be implicitly the current build platform

#### Scenario: Signature format
- **GIVEN** a type T
- **WHEN** generating layout signature
- **THEN** the signature SHALL include platform prefix (e.g., `[64-le]`)
- **AND** the signature SHALL include type category (struct/class/union/enum)
- **AND** the signature SHALL include size and alignment
- **AND** for aggregate types, member information SHALL be included
