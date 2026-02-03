# project-setup Specification

## Purpose
TBD - created by archiving change add-boost-license. Update Purpose after archive.
## Requirements
### Requirement: Boost Software License

The project SHALL be licensed under Boost Software License 1.0.

#### Scenario: License file exists
- **WHEN** checking project root
- **THEN** `LICENSE_1_0.txt` contains the standard BSL-1.0 text

#### Scenario: Source files have license header
- **WHEN** opening any `.hpp` or `.cpp` file
- **THEN** the file begins with the standard Boost license header

