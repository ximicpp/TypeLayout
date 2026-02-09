## ADDED Requirements

### Requirement: Serialization Version Check Scenario Analysis

The signature system SHALL provide a formal analysis document explaining the relationship between the serialization version checking scenario and the two-layer signature architecture, demonstrating why Definition layer is primary for detecting structural evolution and how Layout layer serves as a complementary binary safety check.

#### Scenario: Definition layer is primary for serialization versioning
- **GIVEN** a serialization framework (JSON, protobuf, custom binary) that maps struct fields by name
- **WHEN** analyzing which signature layer detects serialization-breaking changes
- **THEN** the analysis SHALL demonstrate that Definition signature (V2) is the primary tool because:
  - Serialization frameworks depend on field names and types, not raw byte offsets
  - Definition signature encodes field names, qualified types, and inheritance hierarchy (Theorem 3.5)
  - Field rename (`timestamp` → `created_at`) breaks name-based serialization; Definition detects this
  - Field reordering within the same byte layout breaks ordinal-based serialization; Definition detects this

#### Scenario: Layout layer detects binary serialization incompatibility
- **GIVEN** a raw binary serialization that writes struct bytes directly (fwrite-style)
- **WHEN** a struct's padding or alignment changes across compiler versions
- **THEN** the analysis SHALL explain that Layout signature (V1) detects binary serialization incompatibility:
  - Layout captures exact byte offsets and sizes (Theorem 3.1)
  - Padding changes (e.g., new field insertion causing alignment shift) are automatically detected
  - This complements Definition layer: Definition detects semantic changes, Layout detects mechanical changes

#### Scenario: Version evolution detection with concrete example
- **GIVEN** ConfigV1 `{ uint32_t timeout_ms; string name; }` and ConfigV2 `{ uint32_t timeout_seconds; string label; }`
- **WHEN** comparing versions
- **THEN** the analysis SHALL show:
  - Layout signatures may match (if field sizes and offsets are identical)
  - Definition signatures differ (field names changed: `timeout_ms→timeout_seconds`, `name→label`)
  - The Definition mismatch correctly warns that serialization logic must be updated
  - Formal basis: Corollary 3.5.1 — Definition match implies complete structural identity including names
