## ADDED Requirements

### Requirement: File Format Scenario Analysis

The signature system SHALL provide a formal analysis document explaining the relationship between the file format compatibility scenario and the two-layer signature architecture, demonstrating how both layers provide complementary guarantees for cross-platform file I/O safety and version evolution detection.

#### Scenario: Layout layer guarantees safe fread/fwrite
- **GIVEN** a binary file format with a fixed-size header structure (e.g., FileHeader)
- **WHEN** analyzing cross-platform file compatibility
- **THEN** the analysis SHALL demonstrate that Layout signature match guarantees safe fread/fwrite:
  - Theorem 3.2: matching signatures → identical byte layout
  - Therefore `fwrite(&header, sizeof(FileHeader), 1, f)` on platform A produces bytes that can be safely read by `fread(&header, sizeof(FileHeader), 1, f)` on platform B
  - The architecture prefix detects platform incompatibilities at compile time

#### Scenario: Definition layer detects version evolution
- **GIVEN** a file format header that evolves across software versions
- **WHEN** a field is renamed (e.g., `timestamp` → `created_at`) without changing its type or offset
- **THEN** the analysis SHALL demonstrate that:
  - Layout signatures remain identical (same byte layout) — safe to read old files
  - Definition signatures differ (field name changed) — alerting developers to semantic drift
  - This is the key advantage of the two-layer system: V1 answers "can I read this file?" while V2 answers "has the structure definition changed?"

#### Scenario: Two-layer complementarity with concrete example
- **GIVEN** FileHeader v1 with `uint32_t entry_count` and FileHeader v2 with `uint32_t num_records` at the same offset
- **WHEN** comparing both versions
- **THEN** the analysis SHALL show:
  - `layout_signatures_match<v1::FileHeader, v2::FileHeader>() == true` (byte compatible)
  - `definition_signatures_match<v1::FileHeader, v2::FileHeader>() == false` (structural change detected)
  - Formal basis: Theorem 4.3 (Strict Refinement) — ker(⟦·⟧_D) ⊊ ker(⟦·⟧_L)
