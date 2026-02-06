## ADDED Requirements

### Requirement: Cross-Platform Signature Extraction Demo
The project SHALL provide an example program that outputs layout and definition signatures of representative types in a machine-readable JSON format, suitable for cross-platform comparison.

#### Scenario: Extract signatures on a single platform
- **WHEN** the user compiles and runs `example/cross_platform_check.cpp` on any supported platform
- **THEN** the program outputs a JSON document containing the platform identifier, and for each registered type: the type name, layout signature, and definition signature

#### Scenario: Compare signatures across platforms
- **WHEN** the user collects JSON outputs from multiple platforms
- **THEN** running `scripts/compare_signatures.py` on those files produces a compatibility report showing which types are serialization-free across which platform pairs
