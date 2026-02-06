## ADDED Requirements

### Requirement: Core Value Derivation Document
The documentation SHALL include a core value derivation document that logically derives the library's value proposition from its technical capabilities.

#### Scenario: Complete derivation chain
- **WHEN** a reader examines the derivation document
- **THEN** they can trace a logical path from API functions → core guarantees → value proposition → application scenarios
- **AND** each step is supported by formal reasoning or proof

#### Scenario: API capability inventory
- **WHEN** the document lists API capabilities
- **THEN** each API function is described with its inputs, outputs, and information type
- **AND** the relationship between APIs is documented

#### Scenario: Formal proof of core guarantee
- **WHEN** the core guarantee "Same Signature ⟺ Same Layout" is presented
- **THEN** it is accompanied by a formal proof of both directions
- **AND** the proof references specific code implementations

#### Scenario: Application scenario necessity
- **WHEN** application scenarios are presented
- **THEN** each scenario is logically derived from the core guarantee
- **AND** the completeness of scenarios is argued

## MODIFIED Requirements

### Requirement: Value Proposition Document
The documentation SHALL include a value proposition document that explains why users should adopt TypeLayout.

#### Scenario: Reference to derivation
- **WHEN** the value proposition is stated
- **THEN** it references the formal derivation document
- **AND** readers can verify the claims through logical reasoning
