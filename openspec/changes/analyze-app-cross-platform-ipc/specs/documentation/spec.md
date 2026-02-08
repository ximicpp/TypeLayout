## ADDED Requirements

### Requirement: IPC Application Scenario Documentation
The documentation SHALL include a dedicated section analyzing the shared memory / IPC application scenario.

#### Scenario: End-to-end IPC workflow
- **GIVEN** two processes sharing memory via mmap or POSIX shared memory
- **THEN** the documentation SHALL demonstrate the complete workflow from type definition through signature export to compile-time verification
- **AND** the documentation SHALL show that verified types can be used with zero-copy transfer (direct memcpy/mmap access)

#### Scenario: IPC comparison with alternatives
- **GIVEN** alternative approaches to cross-process data sharing (manual serialization, boost::interprocess, protobuf)
- **THEN** the documentation SHALL provide a comparison matrix covering: verification timing, runtime overhead, code complexity, and cross-platform safety
- **AND** the documentation SHALL position TypeLayout as complementary to serialization frameworks for dynamic-size data
