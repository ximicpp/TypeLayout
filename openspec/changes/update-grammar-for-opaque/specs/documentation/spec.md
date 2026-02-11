## MODIFIED Requirements
### Requirement: Conference Presentation Materials
The system SHALL maintain conference presentation materials (proposal, outline, supplementary references) that are technically accurate relative to the project's formal proofs and evaluation data, and optimized for anonymous peer review.

#### Scenario: Signature grammar covers all signature-producing code paths
- **WHEN** the library can produce a signature string (via TypeSignature specializations or opaque macros)
- **THEN** the grammar in the paper (sec3-system.md, 3.2) SHALL have a production that parses that string
- **AND** the supported type table (3.5) SHALL list the corresponding type category