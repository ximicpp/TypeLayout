## ADDED Requirements

### Requirement: CTest 集成

构建系统 SHALL 支持 CMake CTest，允许通过标准 `ctest` 命令运行所有测试。

#### Scenario: 用户运行 CTest

- **WHEN** 用户在构建目录执行 `ctest --output-on-failure`
- **THEN** 运行所有注册的测试
- **AND** 显示测试结果摘要
- **AND** 失败时显示详细输出

#### Scenario: CI 运行测试

- **WHEN** CI 执行构建流程
- **THEN** 自动运行 CTest
- **AND** 测试失败时构建失败
