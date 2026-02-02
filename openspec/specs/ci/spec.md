# ci Specification

## Purpose
TBD - created by archiving change verify-ci-docker-pipeline. Update Purpose after archive.
## Requirements
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

### Requirement: Container Runtime Library Path
CI 容器环境 SHALL 配置正确的运行时库搜索路径，确保 P2996 工具链编译的二进制能够找到 `libc++.so.1`。

#### Scenario: CTest 运行测试成功
- **WHEN** CTest 在容器内运行测试二进制
- **THEN** 测试进程能找到 `/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu/libc++.so.1`
- **AND** 所有测试通过

#### Scenario: 环境变量继承
- **WHEN** CI job 使用 `container:` 指令运行
- **THEN** `LD_LIBRARY_PATH` 环境变量被设置为包含 P2996 工具链库路径
- **AND** 所有 step 都能继承此环境变量

