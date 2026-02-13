## ADDED Requirements

### Requirement: Correctness and Usability Baseline
The library SHALL maintain a documented baseline assessment of correctness
and usability, updated after each major architectural change, to guide
prioritization of improvements.

#### Scenario: Correctness baseline established
- **WHEN** a systematic correctness audit is performed against the formal model
- **THEN** the audit SHALL verify formal-implementation correspondence for all
  major definitions and theorems
- **AND** the audit SHALL identify correctness concerns with severity ratings
- **AND** the audit SHALL assess test coverage by type category

#### Scenario: Usability baseline established
- **WHEN** a systematic usability assessment is performed
- **THEN** the assessment SHALL evaluate API clarity, error experience,
  documentation completeness, pipeline friction, and interoperability
- **AND** the assessment SHALL produce a prioritized recommendation list

### Requirement: Unsupported Type Rejection Tests
The library SHALL have compile-time tests verifying that unsupported types
(void, unbounded arrays, bare function types) are correctly rejected by
the signature system.

#### Scenario: Concept-based rejection verification
- **WHEN** a type T is in the excluded set (void, T[], function types)
- **THEN** a concept checking whether TypeSignature<T, Mode>::calculate()
  is well-formed SHALL evaluate to false
- **AND** this SHALL be verified by static_assert in the test suite

### Requirement: Quickstart Documentation
The library SHALL provide a quickstart guide that enables a new user to
produce a working signature comparison within 5 minutes.

#### Scenario: Minimal working example
- **WHEN** a user reads docs/quickstart.md
- **THEN** the guide SHALL contain a complete, compilable code example
- **AND** the guide SHALL include the exact compile command for P2996 Clang
- **AND** the guide SHALL show expected output

### Requirement: End-to-End Pipeline Example
The library SHALL provide a runnable example demonstrating the complete
two-phase pipeline: export on platform A, export on platform B, compare.

#### Scenario: Pipeline example files
- **WHEN** a user reads examples/README.md
- **THEN** the directory SHALL contain a type-export program and a cross-check program
- **AND** the README SHALL explain how to run them on different platforms

### Requirement: API Reference Documentation
The library SHALL provide a standalone API reference document covering all
public symbols with signatures, return types, preconditions, and examples.

#### Scenario: Complete public API coverage
- **WHEN** a user reads docs/api-reference.md
- **THEN** every public function, macro, class, and enum SHALL be documented
- **AND** each entry SHALL include at minimum: signature, brief description, and one usage example