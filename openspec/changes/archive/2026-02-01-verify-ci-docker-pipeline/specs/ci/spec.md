## ADDED Requirements

### Requirement: Docker Container Environment
The CI system SHALL provide a Docker container with Bloomberg Clang P2996 compiler for running tests that require C++26 static reflection support.

#### Scenario: Docker image build success
- **GIVEN** the docker-build.yml workflow is triggered
- **WHEN** the LLVM/Clang P2996 compilation completes
- **THEN** a Docker image is pushed to ghcr.io/ximicpp/typelayout-p2996:latest

#### Scenario: Docker image availability check
- **GIVEN** the CI workflow starts
- **WHEN** the check-image job runs
- **THEN** it correctly detects whether the Docker image exists in GHCR

#### Scenario: Graceful degradation when image missing
- **GIVEN** the Docker image does not exist in GHCR
- **WHEN** the CI workflow runs
- **THEN** container-based jobs are skipped with clear status messages

### Requirement: Automated CI Triggering
The CI workflow SHALL automatically trigger when the Docker image build completes successfully.

#### Scenario: workflow_run trigger
- **GIVEN** the Docker build workflow completes with success
- **WHEN** the workflow_run event fires
- **THEN** the CI workflow is triggered and runs full test suite

### Requirement: Comprehensive Test Execution
The CI workflow SHALL execute all test categories including compile-time tests, unit tests, static analysis, and header-only verification.

#### Scenario: Full test suite execution
- **GIVEN** the Docker image is available
- **WHEN** the CI workflow runs
- **THEN** Build & Test, Static Analysis, and Header-Only Check jobs all execute

#### Scenario: Test results reporting
- **GIVEN** tests have completed
- **WHEN** the CI Summary job runs
- **THEN** a summary of all job statuses is generated